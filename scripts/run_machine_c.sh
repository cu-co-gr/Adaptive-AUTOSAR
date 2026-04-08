#!/usr/bin/env bash
# Run Machine C (Arduino UNOQ, 192.168.68.64)
# Role: heartbeat provider — EV service instance 2, SOME/IP multicast 239.0.0.1:5555
#
# Run this script on the UNOQ from the deployment directory, e.g.:
#   cd ~/<deploy-dir>
#   ./scripts/run_machine_c.sh
#
# Machine A and Machine B on the Linux server will subscribe to this heartbeat.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="$(dirname "$SCRIPT_DIR")"

BIN="$DEPLOY_DIR/deploy/bin/adaptive_autosar"

if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not found."
    echo "Unpack the deployment tarball and run from the extracted directory."
    exit 1
fi

exec "$BIN" \
    "$DEPLOY_DIR/configuration/machine_c/execution_manifest.arxml" \
    "$DEPLOY_DIR/configuration/machine_c/extended_vehicle_manifest.arxml" \
    "$DEPLOY_DIR/configuration/machine_c/diagnostic_manager_manifest.arxml" \
    "$DEPLOY_DIR/configuration/machine_c/health_monitoring_manifest.arxml"
