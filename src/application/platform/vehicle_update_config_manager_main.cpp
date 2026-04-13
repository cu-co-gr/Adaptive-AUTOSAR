#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <asyncbsdsocket/poller.h>
#include "./vehicle_update_config_manager.h"

/// @brief VUCM main entry point.
///
/// Usage:
///   vehicle_update_config_manager <targetIp> <targetPort>
///                                 <packagePath> <installRoot>
///                                 <executionManifestPath>
///
/// Example (Phase 1 — Machine A → Machine B, same box):
///   vehicle_update_config_manager 127.0.0.1 8091 \
///       /tmp/my_package-1.0.0.swpkg.tar.gz \
///       /opt/adaptive_autosar/bin \
///       ./configuration/machine_b/execution_manifest.arxml

int main(int argc, char *argv[])
{
    if (argc < 6)
    {
        std::printf(
            "Usage: %s <targetIp> <targetPort>"
            " <packagePath> <installRoot> <manifestPath>\n",
            argv[0]);
        return 1;
    }

    const std::string _targetIp{argv[1]};
    const uint16_t _targetPort{
        static_cast<uint16_t>(std::stoi(argv[2]))};
    const std::string _packagePath{argv[3]};
    const std::string _installRoot{argv[4]};
    const std::string _manifestPath{argv[5]};

    std::printf("[VUCM] Starting — target UCM at %s:%u\n",
        _targetIp.c_str(), static_cast<unsigned>(_targetPort));
    std::printf("[VUCM] Package: %s\n", _packagePath.c_str());

    // Give the target UCM time to start listening before connecting.
    const std::chrono::seconds cStartupDelay{3};
    std::printf("[VUCM] Waiting %us for UCM to be ready...\n",
        static_cast<unsigned>(cStartupDelay.count()));
    std::this_thread::sleep_for(cStartupDelay);

    AsyncBsdSocketLib::Poller _poller;

    application::platform::VehicleUpdateConfigManager _vucm(
        &_poller,
        _targetIp,
        _targetPort,
        _packagePath,
        _installRoot,
        _manifestPath);

    int _result{_vucm.Run()};
    std::printf("[VUCM] Exiting (status %d)\n", _result);
    return _result;
}
