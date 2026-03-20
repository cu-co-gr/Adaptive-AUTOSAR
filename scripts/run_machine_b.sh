#!/usr/bin/env bash
# Run Machine B (Service ID 5, Instance 1, ports 18081/8082)
# SD client: discovers ExtendedVehicle service on multicast 239.0.0.1:5555
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"

exec "$REPO_DIR/build/bin/adaptive_autosar" \
    "$REPO_DIR/configuration/machine_b/execution_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/extended_vehicle_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/diagnostic_manager_manifest.arxml" \
    "$REPO_DIR/configuration/machine_b/health_monitoring_manifest.arxml"
