#!/usr/bin/env bash
# create_sw_package.sh
#
# Builds and packages the time_synchronization placeholder application into
# a UCM-compatible SW Package:
#
#   test_packages/time_sync_placeholder-1.0.0.swpkg.tar.gz
#   └── metadata.json
#   └── bin/time_synchronization
#   └── manifest/process_entry.xml
#
# Usage:
#   ./scripts/create_sw_package.sh [build_dir]
#
# Arguments:
#   build_dir  Optional path to the CMake build directory. Default: ./build

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${REPO_ROOT}/build}"
PKG_NAME="time_sync_placeholder"
PKG_VERSION="1.0.0"
PKG_FILENAME="${PKG_NAME}-${PKG_VERSION}.swpkg.tar.gz"
STAGING_DIR="/tmp/${PKG_NAME}_staging"
OUTPUT_DIR="${REPO_ROOT}/test_packages"

echo "[create_sw_package] Building time_synchronization binary..."
cmake --build "${BUILD_DIR}" --target time_synchronization

echo "[create_sw_package] Staging package contents..."
rm -rf "${STAGING_DIR}"
mkdir -p "${STAGING_DIR}/bin"
mkdir -p "${STAGING_DIR}/manifest"

# Copy binary
cp "${BUILD_DIR}/bin/time_synchronization" "${STAGING_DIR}/bin/"

# Write metadata.json
cat > "${STAGING_DIR}/metadata.json" <<EOF
{
    "name": "${PKG_NAME}",
    "version": "${PKG_VERSION}",
    "cluster_type": "application",
    "binary_name": "time_synchronization"
}
EOF

# Write process_entry.xml (PROCESS fragment injected into execution_manifest)
# Format must match the existing processes in execution_manifest.arxml so that
# ExecutionManagement can parse and spawn the process on next restart.
cat > "${STAGING_DIR}/manifest/process_entry.xml" <<EOF
<PROCESS>
    <SHORT-NAME>TimeSynchronization</SHORT-NAME>
    <EXECUTABLE-PATH>./deploy/bin/time_synchronization</EXECUTABLE-PATH>
    <BOOTSTRAP>false</BOOTSTRAP>
    <FUNCTION-GROUP-STATE-IREFS>
        <FUNCTION-GROUP-STATE-IREF>
            <FUNCTION-GROUP-REF>MachineFG</FUNCTION-GROUP-REF>
            <FUNCTION-GROUP-STATE-REF>StartUp</FUNCTION-GROUP-STATE-REF>
        </FUNCTION-GROUP-STATE-IREF>
    </FUNCTION-GROUP-STATE-IREFS>
</PROCESS>
EOF

echo "[create_sw_package] Creating tarball..."
mkdir -p "${OUTPUT_DIR}"
tar czf "${OUTPUT_DIR}/${PKG_FILENAME}" -C "${STAGING_DIR}" .

echo "[create_sw_package] Created: ${OUTPUT_DIR}/${PKG_FILENAME}"
ls -lh "${OUTPUT_DIR}/${PKG_FILENAME}"
