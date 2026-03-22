#!/usr/bin/env bash
# Functional test: VehicleStatus pub/sub + Watchdog timeout detection
#
# What it does:
#   1. Starts Machine A and Machine B (each with their own log file)
#   2. Waits for the event pipeline to warm up (SD + subscription)
#   3. Checks that periodic events ARE flowing (no "Event expired" in 8 s)
#   4. Stops Machine A's publisher mid-run and verifies the watchdog on
#      Machine B fires "Event expired" within the 2 s timeout window
#
# Pass/fail is printed at the end.  Exit code 0 = all checks passed.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
BIN="$REPO_DIR/build/bin/adaptive_autosar"
LOG_A="/tmp/autosar_machine_a.log"
LOG_B="/tmp/autosar_machine_b.log"

PASS=0
FAIL=0

check() {
    local desc="$1" result="$2"
    if [ "$result" = "0" ]; then
        echo "  PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $desc"
        FAIL=$((FAIL + 1))
    fi
}

if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not found — run 'cmake --build build' first."
    exit 1
fi

cleanup() {
    [ -n "${PID_A:-}" ] && [ "$PID_A" != "0" ] && kill "$PID_A" 2>/dev/null || true
    [ -n "${PID_B:-}" ] && [ "$PID_B" != "0" ] && kill "$PID_B" 2>/dev/null || true
    [ -n "${PID_A:-}" ] && [ "$PID_A" != "0" ] && wait "$PID_A" 2>/dev/null || true
    [ -n "${PID_B:-}" ] && [ "$PID_B" != "0" ] && wait "$PID_B" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "=== Watchdog functional test ==="
echo "Logs: $LOG_A  $LOG_B"
echo ""

# ── 1. Start both machines ────────────────────────────────────────────────────
{ sleep 30; echo; echo; } | "$BIN" \
    "$REPO_DIR/configuration/machine_a/execution_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/extended_vehicle_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/diagnostic_manager_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/health_monitoring_manifest.arxml" \
    > "$LOG_A" 2>&1 &
PID_A=$!

{ sleep 30; echo; echo; } | "$BIN" \
    "$REPO_DIR/configuration/machine_b/execution_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/extended_vehicle_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/diagnostic_manager_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/health_monitoring_manifest.arxml" \
    > "$LOG_B" 2>&1 &
PID_B=$!

echo "Started Machine A (PID $PID_A) and Machine B (PID $PID_B)"

# ── 2. Wait for warm-up (SD handshake + first event cycle ~1 s) ───────────────
echo "Waiting 8 s for event pipeline to stabilise..."
sleep 8

# ── 3. Basic liveness checks ─────────────────────────────────────────────────
echo ""
echo "--- Liveness ---"
kill -0 "$PID_A" 2>/dev/null; check "Machine A still running" $?
kill -0 "$PID_B" 2>/dev/null; check "Machine B still running" $?

echo ""
echo "--- Log content after 8 s ---"
grep -q "\[Watchdog\] Started" "$LOG_A" 2>/dev/null; check "Machine A: Watchdog initialised" $?
grep -q "\[Watchdog\] Started" "$LOG_B" 2>/dev/null; check "Machine B: Watchdog initialised" $?
grep -q "\[Heartbeat\] ExtendedVehicle alive" "$LOG_A" 2>/dev/null; check "Machine A: Heartbeat (publisher running)" $?
grep -q "\[Heartbeat\] ExtendedVehicle alive" "$LOG_B" 2>/dev/null; check "Machine B: Heartbeat (publisher running)" $?

# Events should be flowing — watchdog must NOT have expired yet
grep -q "\[Watchdog\] Event expired" "$LOG_A" 2>/dev/null && _r=1 || _r=0
check "Machine A: No watchdog expiry while publisher is alive" $_r

grep -q "\[Watchdog\] Event expired" "$LOG_B" 2>/dev/null && _r=1 || _r=0
check "Machine B: No watchdog expiry while publisher is alive" $_r

# ── 4. Kill Machine A's publisher and wait for Machine B's watchdog to fire ───
echo ""
echo "--- Killing Machine A (PID $PID_A) to test B's watchdog ---"
kill "$PID_A" 2>/dev/null || true
wait "$PID_A" 2>/dev/null || true
PID_A=0

echo "Waiting 4 s (> 2 s watchdog timeout)..."
sleep 4

grep -q "\[Watchdog\] Event expired" "$LOG_B" 2>/dev/null; check "Machine B: Watchdog fired after publisher went silent" $?

# ── 5. Summary ────────────────────────────────────────────────────────────────
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
