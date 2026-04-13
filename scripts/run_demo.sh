#!/usr/bin/env bash
# Three-machine Adaptive AUTOSAR demo
#
# Topology:
#   Machine C (UNOQ, 192.168.68.64) — heartbeat provider (EV instance 2)
#   Machine A (this host, RPC :18080) — watchdog subscriber to Machine C EV
#   Machine B (this host, RPC :18081) — watchdog subscriber to Machine C EV
#
# PREREQUISITE: Machine C must already be running on the Arduino UNOQ:
#   ssh user@192.168.68.64
#   cd <deploy-dir> && ./scripts/run_machine_c.sh
#
# This script starts Machine A and Machine B on the local host.
# Output lines are prefixed with [A] and [B].
# Use Ctrl+C to stop both machines.
#
# To capture SOME/IP traffic (replace <iface> with your network interface,
# e.g. eth0 or wlan0 — NOT lo, since Machine C is on a real network):
#   IFACE=$(ip route get 192.168.68.64 | grep -oP 'dev \K\S+' | head -1)
#   sudo tcpdump -i "$IFACE" -w /tmp/autosar_demo.pcap \
#     'udp port 5555 or tcp port 8081 or tcp port 18080 or tcp port 18081'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
BIN="$REPO_DIR/build/bin/adaptive_autosar"

if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not found. Run 'cmake --build build' first."
    exit 1
fi

cleanup() {
    echo ""
    echo "Stopping Machine A and Machine B..."
    kill %1 %2 2>/dev/null
    wait
    pkill -f "build/bin/state_management"              2>/dev/null || true
    pkill -f "build/bin/platform_health_management"    2>/dev/null || true
    pkill -f "build/bin/diagnostic_manager"            2>/dev/null || true
    pkill -f "build/bin/watchdog_application"          2>/dev/null || true
    pkill -f "build/bin/update_config_manager"         2>/dev/null || true
    pkill -f "build/bin/vehicle_update_config_manager" 2>/dev/null || true
    echo "Done. (Machine C on UNOQ continues running.)"
}
trap cleanup INT TERM

cd "$REPO_DIR"

echo "Machine A: WA instance 0, RPC :18080 — watching Machine C EV instance 2"
echo "Machine B: WA instance 1, RPC :18081 — watching Machine C EV instance 2"
echo "Machine C: EV instance 2, heartbeat provider at 192.168.68.64"
echo "Both A and B discovering Machine C via SOME/IP SD multicast 239.0.0.1:5555"
echo "Press Ctrl+C to stop A and B."
echo ""

stdbuf -oL "$BIN" ./configuration/machine_a/execution_manifest.arxml \
    2>&1 | sed -u 's/^/[A] /' &

stdbuf -oL "$BIN" ./configuration/machine_b/execution_manifest.arxml \
    2>&1 | sed -u 's/^/[B] /' &

wait
