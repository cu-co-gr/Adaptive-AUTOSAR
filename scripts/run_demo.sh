#!/usr/bin/env bash
# Two-machine Adaptive AUTOSAR demo
# Launches Machine A and Machine B on the same host.
# Output lines are prefixed with [A] and [B] so you can tell them apart.
# Use Ctrl+C to stop both machines.
#
# To capture traffic with tcpdump (optional):
#   sudo tcpdump -i lo -w /tmp/autosar_demo.pcap &
# Then open the .pcap in Wireshark and filter: udp.port==5555 or tcp.port==8081 or tcp.port==8082

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
BIN="$REPO_DIR/build/bin/adaptive_autosar"

if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not found. Run 'cmake --build build' first."
    exit 1
fi

cleanup() {
    echo ""
    echo "Stopping both machines..."
    kill %1 %2 2>/dev/null
    wait
    echo "Done."
}
trap cleanup INT TERM

echo "Machine A: Service Instance 0, RPC :18080, DoIP :8081"
echo "Machine B: Service Instance 1, RPC :18081, DoIP :8082"
echo "Both discovering each other via SOME/IP SD on multicast 239.0.0.1:5555"
echo "Press Ctrl+C to stop."
echo ""

"$BIN" \
    "$REPO_DIR/configuration/machine_a/execution_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/extended_vehicle_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/diagnostic_manager_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/health_monitoring_manifest.arxml" \
    2>&1 | sed 's/^/[A] /' &

"$BIN" \
    "$REPO_DIR/configuration/machine_b/execution_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/extended_vehicle_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/diagnostic_manager_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/health_monitoring_manifest.arxml" \
    2>&1 | sed 's/^/[B] /' &

wait
