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
    pkill -f "build/bin/state_management" 2>/dev/null || true
    pkill -f "build/bin/platform_health_management" 2>/dev/null || true
    pkill -f "build/bin/extended_vehicle" 2>/dev/null || true
    pkill -f "build/bin/diagnostic_manager" 2>/dev/null || true
    pkill -f "build/bin/watchdog_application" 2>/dev/null || true
    echo "Done."
}
trap cleanup INT TERM

# Child processes are spawned via fork/exec with paths relative to the repo
# root, so we must set the working directory before launching.
cd "$REPO_DIR"

echo "Machine A: Service Instance 0, RPC :18080, DoIP :8081"
echo "Machine B: Service Instance 1, RPC :18081, DoIP :8082"
echo "Both discovering each other via SOME/IP SD on multicast 239.0.0.1:5555"
echo "Press Ctrl+C to stop."
echo ""

"$BIN" ./configuration/machine_a/execution_manifest.arxml \
    2>&1 | sed 's/^/[A] /' &

"$BIN" ./configuration/machine_b/execution_manifest.arxml \
    2>&1 | sed 's/^/[B] /' &

wait
