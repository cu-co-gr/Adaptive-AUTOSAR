#ifndef VEHICLE_UPDATE_CONFIG_MANAGER_H
#define VEHICLE_UPDATE_CONFIG_MANAGER_H

#include <string>
#include <asyncbsdsocket/poller.h>
#include "../../ara/ucm/package_manager.h"

namespace package_management { class PackageManagementProxy; }

namespace application
{
    namespace platform
    {
        /// @brief VUCM (Vehicle UCM) — orchestrates a full software package
        ///        update workflow against one target UCM instance.
        ///
        /// Phase 1 (implemented): the SW package already exists at a path on
        /// the shared filesystem.  PackageInfoType::name is the absolute path;
        /// PackageInfoType::size is 0 (no binary chunks are sent).
        ///
        /// Workflow: TransferStart → TransferData(empty) → TransferExit →
        ///           ProcessSwPackage → Activate.
        class VehicleUpdateConfigManager
        {
        public:
            /// @brief Constructor — connects the proxy to the target UCM.
            /// @param poller        Global async I/O poller.
            /// @param targetIp      Target UCM IP address.
            /// @param targetPort    Target UCM TCP port.
            /// @param packagePath   Absolute path to .swpkg.tar.gz on shared fs.
            /// @param installRoot   Directory to install extracted binary into.
            /// @param manifestPath  Execution-manifest ARXML to be patched.
            VehicleUpdateConfigManager(
                AsyncBsdSocketLib::Poller *poller,
                const std::string &targetIp,
                uint16_t targetPort,
                const std::string &packagePath,
                const std::string &installRoot,
                const std::string &manifestPath);

            ~VehicleUpdateConfigManager();

            VehicleUpdateConfigManager(const VehicleUpdateConfigManager &) = delete;
            VehicleUpdateConfigManager &operator=(
                const VehicleUpdateConfigManager &) = delete;

            /// @brief Execute the full update workflow.
            /// @return 0 on success, non-zero on failure.
            int Run();

        private:
            AsyncBsdSocketLib::Poller *mPoller;
            std::unique_ptr<package_management::PackageManagementProxy> mProxy;
            std::string mPackagePath;
            std::string mInstallRoot;
            std::string mManifestPath;
        };

    } // namespace platform
} // namespace application

#endif
