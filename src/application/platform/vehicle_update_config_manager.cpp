#include <cstdio>
#include "./vehicle_update_config_manager.h"
#include "package_management_proxy.h"

namespace application
{
    namespace platform
    {
        VehicleUpdateConfigManager::VehicleUpdateConfigManager(
            AsyncBsdSocketLib::Poller *poller,
            const std::string &targetIp,
            uint16_t targetPort,
            const std::string &packagePath,
            const std::string &installRoot,
            const std::string &manifestPath)
            : mPoller{poller},
              mProxy{std::make_unique<package_management::PackageManagementProxy>(
                  poller, targetIp, targetPort)},
              mPackagePath{packagePath},
              mInstallRoot{installRoot},
              mManifestPath{manifestPath}
        {
        }

        VehicleUpdateConfigManager::~VehicleUpdateConfigManager() = default;

        int VehicleUpdateConfigManager::Run()
        {
            // ── Phase 1 workflow ──────────────────────────────────────────────
            // Package file is already on the shared filesystem; no binary
            // chunks are sent.  Name = absolute path; size = 0.

            ara::ucm::PackageInfoType _info;
            _info.name    = mPackagePath;
            _info.version = "1.0.0";
            _info.size    = 0;

            // TransferStart
            std::printf("[VUCM] TransferStart: %s\n", mPackagePath.c_str());
            auto _start{mProxy->TransferStart(_info)};
            if (!_start.HasValue())
            {
                std::printf("[VUCM] TransferStart FAILED (error %llu)\n",
                    static_cast<unsigned long long>(_start.Error().Value()));
                return 1;
            }
            ara::ucm::TransferIdType _id{std::move(_start).Value()};
            std::printf("[VUCM] TransferStart OK, id=%u\n", _id);

            // TransferData (empty block — Phase 1 path)
            auto _data{mProxy->TransferData(_id, {}, 0)};
            if (!_data.HasValue())
            {
                std::printf("[VUCM] TransferData FAILED (error %llu)\n",
                    static_cast<unsigned long long>(_data.Error().Value()));
                return 1;
            }
            std::printf("[VUCM] TransferData OK\n");

            // TransferExit
            auto _exit{mProxy->TransferExit(_id)};
            if (!_exit.HasValue())
            {
                std::printf("[VUCM] TransferExit FAILED (error %llu)\n",
                    static_cast<unsigned long long>(_exit.Error().Value()));
                return 1;
            }
            std::printf("[VUCM] TransferExit OK\n");

            // ProcessSwPackage
            auto _process{mProxy->ProcessSwPackage(_id)};
            if (!_process.HasValue())
            {
                std::printf("[VUCM] ProcessSwPackage FAILED (error %llu)\n",
                    static_cast<unsigned long long>(_process.Error().Value()));
                return 1;
            }
            std::printf("[VUCM] ProcessSwPackage OK\n");

            // Activate
            auto _activate{mProxy->Activate(mInstallRoot, mManifestPath)};
            if (!_activate.HasValue())
            {
                std::printf("[VUCM] Activate FAILED (error %llu)\n",
                    static_cast<unsigned long long>(_activate.Error().Value()));
                return 1;
            }
            std::printf("[VUCM] Transfer complete. Processing. Activation complete.\n");
            return 0;
        }

    } // namespace platform
} // namespace application
