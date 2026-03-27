#!/usr/bin/env bash
# Functional test: VehicleStatus pub/sub + Watchdog timeout detection
#
# Architecture (split-binary design):
#   Machine A: StateManagement + PlatformHealthManagement +
#              ExtendedVehicle (publisher) + DiagnosticManager
#   Machine B: StateManagement + WatchdogApplication (subscriber/monitor)
#
# What it does:
#   1. Starts Machine A and Machine B (each with their own log file)
#   2. Waits for the event pipeline to warm up (SD + subscription)
#   3. Checks all components initialized on both machines
#   4. Confirms periodic heartbeat from the publisher (≥ 3 in 8 s)
#   5. Checks that no watchdog expiry occurred while publisher is alive
#   6. Kills Machine A and verifies each child process terminated cleanly
#   7. Verifies Machine B's watchdog fires within the 2 s timeout window
#   8. Kills Machine B and verifies each child process terminated cleanly
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
    # Kill any orphaned child processes that EM may not have reaped
    pkill -f "build/bin/state_management" 2>/dev/null || true
    pkill -f "build/bin/platform_health_management" 2>/dev/null || true
    pkill -f "build/bin/extended_vehicle" 2>/dev/null || true
    pkill -f "build/bin/diagnostic_manager" 2>/dev/null || true
    pkill -f "build/bin/watchdog_application" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Child processes spawned by EM use paths relative to the repo root.
cd "$REPO_DIR"

echo "=== Watchdog functional test ==="
echo "Logs: $LOG_A  $LOG_B"
echo ""

# ── 1. Start both machines ────────────────────────────────────────────────────
# Machine A: StateManagement + PlatformHealthManagement +
#            ExtendedVehicle (publisher) + DiagnosticManager
stdbuf -o0 "$BIN" ./configuration/machine_a/execution_manifest.arxml \
    > "$LOG_A" 2>&1 &
PID_A=$!

# Machine B: StateManagement + WatchdogApplication (subscriber/monitor)
stdbuf -o0 "$BIN" ./configuration/machine_b/execution_manifest.arxml \
    > "$LOG_B" 2>&1 &
PID_B=$!

echo "Started Machine A (PID $PID_A) and Machine B (PID $PID_B)"

# ── 2. Wait for warm-up (SD handshake + first event cycle ~1 s) ───────────────
echo "Waiting 8 s for event pipeline to stabilise..."
sleep 8

# ── 3. Liveness ───────────────────────────────────────────────────────────────
echo ""
echo "--- Liveness ---"
kill -0 "$PID_A" 2>/dev/null; check "Machine A still running" $?
kill -0 "$PID_B" 2>/dev/null; check "Machine B still running" $?

# ── 4. Initialization ─────────────────────────────────────────────────────────
echo ""
echo "--- Initialization (Machine A) ---"
grep -qa "Execution management has been initialized." "$LOG_A" 2>/dev/null
check "Machine A: EM initialized" $?
grep -qa "State management has been initialized." "$LOG_A" 2>/dev/null
check "Machine A: SM initialized" $?
grep -qa "Plafrom health management has been initialized." "$LOG_A" 2>/dev/null
check "Machine A: PHM initialized" $?
grep -qa "Extended Vehicle AA has been initialized." "$LOG_A" 2>/dev/null
check "Machine A: ExtendedVehicle initialized" $?
grep -qa "Diagnostic Manager has been initialized." "$LOG_A" 2>/dev/null
check "Machine A: DiagnosticManager initialized" $?

echo ""
echo "--- Initialization (Machine B) ---"
grep -qa "Execution management has been initialized." "$LOG_B" 2>/dev/null
check "Machine B: EM initialized" $?
grep -qa "State management has been initialized." "$LOG_B" 2>/dev/null
check "Machine B: SM initialized" $?
grep -qa "\[Watchdog\] Started" "$LOG_B" 2>/dev/null
check "Machine B: WatchdogApplication started" $?

# ── 5. Event pipeline ─────────────────────────────────────────────────────────
echo ""
echo "--- Event pipeline ---"
_hb_count=$(grep -ca "\[Heartbeat\] ExtendedVehicle alive" "$LOG_A" 2>/dev/null || true)
[ "$_hb_count" -ge 3 ] && _r=0 || _r=1
check "Machine A: Heartbeat periodic (≥ 3 in 8 s, got $_hb_count)" $_r

grep -qa "\[Watchdog\] Event expired" "$LOG_A" 2>/dev/null && _r=1 || _r=0
check "Machine A: No watchdog expiry while publisher is alive" $_r

grep -qa "\[Watchdog\] Event expired" "$LOG_B" 2>/dev/null && _r=1 || _r=0
check "Machine B: No watchdog expiry while publisher is alive" $_r

# ── 6. Kill Machine A — verify watchdog fires and shutdown is clean ────────────
echo ""
echo "--- Killing Machine A (PID $PID_A) ---"
kill "$PID_A" 2>/dev/null || true
wait "$PID_A" 2>/dev/null || true
PID_A=0

echo ""
echo "--- Machine A clean shutdown ---"
grep -qa "Extended Vehicle AA has been terminated." "$LOG_A" 2>/dev/null
check "Machine A: ExtendedVehicle terminated" $?
grep -qa "Diagnostic Manager has been terminated." "$LOG_A" 2>/dev/null
check "Machine A: DiagnosticManager terminated" $?
grep -qa "Plafrom health management has been terminated." "$LOG_A" 2>/dev/null
check "Machine A: PHM terminated" $?
grep -qa "State management has been terminated." "$LOG_A" 2>/dev/null
check "Machine A: SM terminated" $?
grep -qa "Execution management has been terminated." "$LOG_A" 2>/dev/null
check "Machine A: EM terminated" $?

echo ""
echo "Waiting 4 s (> 2 s watchdog timeout)..."
sleep 4

# ── 7. Kill Machine B — verify shutdown is clean ──────────────────────────────
# Note: the watchdog-fired check is performed after killing Machine B so that
# all log buffers are guaranteed to be flushed before grepping.
echo ""
echo "--- Killing Machine B (PID $PID_B) ---"
kill "$PID_B" 2>/dev/null || true
wait "$PID_B" 2>/dev/null || true
PID_B=0

echo ""
echo "--- Machine B watchdog fired (log check after shutdown flush) ---"
grep -qa "\[Watchdog\] Event expired" "$LOG_B" 2>/dev/null
check "Machine B: Watchdog fired after publisher went silent" $?

echo ""
echo "--- Machine B clean shutdown ---"
grep -qa "State management has been terminated." "$LOG_B" 2>/dev/null
check "Machine B: SM terminated" $?
grep -qa "Execution management has been terminated." "$LOG_B" 2>/dev/null
check "Machine B: EM terminated" $?

# ── 8. Summary ────────────────────────────────────────────────────────────────
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
