#!/usr/bin/env bash
# Run Machine A (Service ID 5, Instance 0, ports 18080/8081)
# SD server: advertises ExtendedVehicle service on multicast 239.0.0.1:5555
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"

exec "$REPO_DIR/build/bin/adaptive_autosar" \
    "$REPO_DIR/configuration/machine_a/execution_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/extended_vehicle_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/diagnostic_manager_manifest.arxml" \
    "$REPO_DIR/configuration/machine_a/health_monitoring_manifest.arxml"
