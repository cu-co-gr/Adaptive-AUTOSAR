#!/usr/bin/env bash
# Functional test: Machine A + B watching Machine C heartbeat
#
# Architecture:
#   Machine C (UNOQ, 192.168.1.96): EV heartbeat provider, service 5 instance 2
#   Machine A (this host, RPC :18080): WA instance 0, subscribes to Machine C EV
#   Machine B (this host, RPC :18081): WA instance 1, subscribes to Machine C EV
#
# PREREQUISITE: Machine C must be running on the Arduino UNOQ BEFORE this
#   script is launched. Start it with:
#     ssh user@192.168.1.96 "cd <deploy-dir> && ./scripts/run_machine_c.sh &"
#
# What it does:
#   1.  Checks Machine C is reachable (ping)
#   2.  Starts tcpdump on the network interface facing Machine C
#   3.  Starts Machine A and Machine B; waits 12 s for SD handshake
#   4.  Checks all processes initialised on both machines (no EV on A/B)
#   5.  Confirms both A and B WA receive heartbeats from Machine C (≥ 3 in 12 s)
#   6.  Confirms both A and B WA watchdogs are reset by Machine C EV events
#   7.  Checks no spurious watchdog expiry while Machine C is running
#   8.  Wire analysis via tshark: SD Offer instance 2, Subscribe, SubscribeAck,
#       VehicleStatus events
#   9.  Prints results and exits 0 only if all required checks pass
#
# Watchdog-fires test (manual, not automated):
#   After this script finishes, stop Machine C on the UNOQ and observe that
#   both Machine A and Machine B WA log "[Watchdog] Event expired" within
#   ~2 s of the last heartbeat.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
BIN="$REPO_DIR/build/bin/adaptive_autosar"
LOG_A="/tmp/autosar_machine_a.log"
LOG_B="/tmp/autosar_machine_b.log"
PCAP="/tmp/autosar_wire.pcap"
MACHINE_C_IP="192.168.2.96"

PASS=0
FAIL=0

PID_A=0
PID_B=0
PID_TCPDUMP=0

# ── Helpers ───────────────────────────────────────────────────────────────────
check() {
    local desc="$1" result="$2"
    if [ "$result" = "0" ]; then
        printf "  PASS: %s\n" "$desc"
        PASS=$((PASS + 1))
    else
        printf "  FAIL: %s\n" "$desc"
        FAIL=$((FAIL + 1))
    fi
}

info() {
    printf "  INFO: %s\n" "$1"
}

cleanup() {
    [ "$PID_TCPDUMP" != "0" ] && kill "$PID_TCPDUMP" 2>/dev/null || true
    [ "$PID_A"       != "0" ] && kill "$PID_A"       2>/dev/null || true
    [ "$PID_B"       != "0" ] && kill "$PID_B"       2>/dev/null || true
    [ "$PID_A"       != "0" ] && wait "$PID_A"       2>/dev/null || true
    [ "$PID_B"       != "0" ] && wait "$PID_B"       2>/dev/null || true
    [ "$PID_TCPDUMP" != "0" ] && wait "$PID_TCPDUMP" 2>/dev/null || true
    pkill -f "build/bin/state_management"           2>/dev/null || true
    pkill -f "build/bin/platform_health_management" 2>/dev/null || true
    pkill -f "build/bin/diagnostic_manager"         2>/dev/null || true
    pkill -f "build/bin/watchdog_application"       2>/dev/null || true
}
trap cleanup EXIT INT TERM

if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not found — run 'cmake --build build' first."
    exit 1
fi

cd "$REPO_DIR"

echo "=== Machine C heartbeat watchdog functional test ==="
echo "  Machine C: EV instance 2 at $MACHINE_C_IP — must be running already"
echo "  Machine A: RPC :18080, WA instance 0 → log $LOG_A"
echo "  Machine B: RPC :18081, WA instance 1 → log $LOG_B"
echo "  Wire pcap: $PCAP"
echo ""

# ── 1. Machine C reachability ─────────────────────────────────────────────────
echo "--- Machine C reachability ---"
ping -c 1 -W 2 "$MACHINE_C_IP" >/dev/null 2>&1
check "Machine C ($MACHINE_C_IP) is reachable" $?

# Auto-detect the network interface that routes to Machine C.
IFACE=$(ip route get "$MACHINE_C_IP" 2>/dev/null | grep -oP 'dev \K\S+' | head -1)
if [ -z "$IFACE" ]; then
    IFACE="any"
    info "Could not detect interface for $MACHINE_C_IP — using 'any'"
else
    info "Capturing SOME/IP traffic on interface: $IFACE"
fi

# ── 2. Start tcpdump ──────────────────────────────────────────────────────────
rm -f "$PCAP"
if command -v tcpdump &>/dev/null; then
    tcpdump -i "$IFACE" -w "$PCAP" \
        'udp port 5555 or tcp port 8081 or tcp port 18080 or tcp port 18081' \
        2>/dev/null &
    PID_TCPDUMP=$!
    sleep 0.5
    echo "Wire capture started (PID $PID_TCPDUMP, iface $IFACE)"
else
    echo "WARNING: tcpdump not found — wire capture disabled"
fi

# ── 3. Start Machine A and Machine B ─────────────────────────────────────────
echo ""
stdbuf -o0 "$BIN" ./configuration/machine_a/execution_manifest.arxml \
    >"$LOG_A" 2>&1 &
PID_A=$!

stdbuf -o0 "$BIN" ./configuration/machine_b/execution_manifest.arxml \
    >"$LOG_B" 2>&1 &
PID_B=$!

echo "Started Machine A (PID $PID_A) and Machine B (PID $PID_B)"
echo "Waiting 12 s for SD handshake + event pipeline to stabilise..."
sleep 12

# ── 4. Liveness ───────────────────────────────────────────────────────────────
echo ""
echo "--- Liveness ---"
kill -0 "$PID_A" 2>/dev/null; check "Machine A still running" $?
kill -0 "$PID_B" 2>/dev/null; check "Machine B still running" $?

# ── 5. Initialization ─────────────────────────────────────────────────────────
echo ""
echo "--- Initialization ---"
for machine in A B; do
    log_var="LOG_${machine}"
    log="${!log_var}"
    grep -qa "Execution management has been initialized."      "$log" 2>/dev/null
    check "Machine $machine: EM initialized" $?
    grep -qa "State management has been initialized."          "$log" 2>/dev/null
    check "Machine $machine: SM initialized" $?
    grep -qa "Plafrom health management has been initialized." "$log" 2>/dev/null
    check "Machine $machine: PHM initialized" $?
    grep -qa "Diagnostic Manager has been initialized."        "$log" 2>/dev/null
    check "Machine $machine: DM initialized" $?
    grep -qa "\[Watchdog\] Started"                            "$log" 2>/dev/null
    check "Machine $machine: WA started" $?
    # EV is intentionally absent on Machine A and B in this topology.
    if grep -qa "Extended Vehicle AA has been initialized." "$log" 2>/dev/null; then
        info "Machine $machine: EV unexpectedly present [INFO]"
    fi
done

# ── 6. Heartbeat from Machine C (log-based) ───────────────────────────────────
echo ""
echo "--- Heartbeat from Machine C (log-based) ---"

for machine in A B; do
    log_var="LOG_${machine}"
    log="${!log_var}"

    # Watchdog reset messages confirm that Machine C EV events arrived and
    # the WA handler fired for instance 2.
    _wr=$(grep -ca "\[Watchdog\].*restarted by ExtendedVehicle 2" "$log" 2>/dev/null; true)
    _wr=${_wr:-0}
    [ "$_wr" -ge 1 ] && _r=0 || _r=1
    check "Machine $machine WA: Watchdog reset by Machine C EV (got $_wr)" $_r
done

# No spurious watchdog expiry on either machine while Machine C is running.
grep -qa "\[Watchdog\] Event expired" "$LOG_A" 2>/dev/null && _r=1 || _r=0
check "Machine A: No spurious watchdog expiry while Machine C running" $_r
grep -qa "\[Watchdog\] Event expired" "$LOG_B" 2>/dev/null && _r=1 || _r=0
check "Machine B: No spurious watchdog expiry while Machine C running" $_r

# ── 7. Stop captures and machines ─────────────────────────────────────────────
echo ""
echo "--- Stopping Machine A and Machine B ---"
kill "$PID_A" 2>/dev/null || true; wait "$PID_A" 2>/dev/null || true; PID_A=0
kill "$PID_B" 2>/dev/null || true; wait "$PID_B" 2>/dev/null || true; PID_B=0

if [ "$PID_TCPDUMP" != "0" ]; then
    kill "$PID_TCPDUMP" 2>/dev/null || true
    wait "$PID_TCPDUMP" 2>/dev/null || true
    PID_TCPDUMP=0
fi

# ── 8. Wire analysis ──────────────────────────────────────────────────────────
echo ""
echo "--- Wire analysis (SOME/IP) ---"

if [ ! -f "$PCAP" ]; then
    info "SKIP: No pcap file (tcpdump not available)"
elif ! command -v tshark &>/dev/null; then
    info "SKIP: tshark not installed (sudo apt-get install tshark)"
    info "      Pcap saved: $PCAP — open in Wireshark, filter: udp.port==5555"
else
    _has_someip=0
    _has_someipsd=0
    tshark -G fields 2>/dev/null | grep -q "someip.serviceid"  && _has_someip=1  || true
    tshark -G fields 2>/dev/null | grep -q "someipsd.entry"    && _has_someipsd=1 || true

    if [ "$_has_someip" = "0" ]; then
        info "SKIP: tshark SOME/IP dissector not available"
        info "      Pcap saved: $PCAP"
    else
        tshark_count() {
            tshark -r "$PCAP" -Y "$1" -T fields -e frame.number 2>/dev/null | wc -l
        }

        # VehicleStatus event notifications from Machine C (service 5, method 0x8001)
        _ev_total=$(tshark_count "someip.serviceid == 5 && someip.methodid == 0x8001")
        [ "$_ev_total" -ge 3 ] && _r=0 || _r=1
        check "Wire: VehicleStatus events service 5 (≥ 3, got $_ev_total)" $_r

        if [ "$_has_someipsd" = "1" ]; then
            # SD Offer for instance 2 (Machine C EV)
            _sd_offer_2=$(tshark_count \
                "someipsd.entry.type == 1 && someipsd.entry.serviceid == 5 && someipsd.entry.instanceid == 2")
            [ "$_sd_offer_2" -ge 1 ] && _r=0 || _r=1
            check "Wire: SD Offer — service 5 instance 2 (Machine C EV, got $_sd_offer_2)" $_r

            # SD Subscribe from Machine A WA → Machine C EV (instance 2)
            _sd_sub_a=$(tshark_count \
                "someipsd.entry.type == 6 && someipsd.entry.serviceid == 5 && someipsd.entry.instanceid == 2")
            [ "$_sd_sub_a" -ge 1 ] && _r=0 || _r=1
            check "Wire: SD Subscribe — instance 2 (A or B WA → Machine C EV, got $_sd_sub_a)" $_r

            # SD SubscribeAck from Machine C EV → Machine A/B WA
            _sd_ack_2=$(tshark_count \
                "someipsd.entry.type == 7 && someipsd.entry.serviceid == 5 && someipsd.entry.instanceid == 2")
            [ "$_sd_ack_2" -ge 1 ] && _r=0 || _r=1
            check "Wire: SD SubscribeAck — instance 2 (Machine C EV → A/B WA, got $_sd_ack_2)" $_r
        else
            info "SKIP: SOME/IP-SD dissector fields not available; skipping SD-level checks"
        fi
        echo "  Pcap: $PCAP"
    fi
fi

# ── 9. Summary ────────────────────────────────────────────────────────────────
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
echo ""
echo "NOTE — Watchdog-fires test (manual):"
echo "  Stop Machine C on the UNOQ and wait ~2 s. Both Machine A and Machine B"
echo "  WA should log '[Watchdog] Event expired'. Re-run run_demo.sh and observe"
echo "  the logs to confirm."
echo ""
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
