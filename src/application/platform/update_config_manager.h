#ifndef UPDATE_CONFIG_MANAGER_H
#define UPDATE_CONFIG_MANAGER_H

#include <memory>
#include <string>
#include <asyncbsdsocket/poller.h>
#include "../../ara/ucm/package_manager.h"

namespace package_management { class PackageManagementSkeleton; }

namespace application
{
    namespace platform
    {
        /// @brief UCM (Update and Configuration Management) platform service.
        ///
        /// Owns a PackageManager (state machine + FileStorage) and a
        /// PackageManagementSkeleton (SOME/IP RPC server).  The main process
        /// loop calls Poller::TryPoll() to drive network I/O.
        ///
        /// Manifest ARXML element names expected:
        ///   networkEndpoint     "UcmServerEP"
        ///   applicationEndpoint "PackageManagementTcp"
        class UpdateConfigManager
        {
        public:
            /// @brief Constructor.
            /// @param poller       Global async I/O poller.
            /// @param manifestPath Path to the UCM ARXML manifest.
            /// @param storageRoot  Root directory for package staging and
            ///                     the installed-cluster registry.
            /// @throws std::runtime_error if the manifest cannot be parsed or
            ///         the RPC server socket cannot be set up.
            UpdateConfigManager(
                AsyncBsdSocketLib::Poller *poller,
                const std::string &manifestPath,
                const std::string &storageRoot);

            ~UpdateConfigManager();

            // Non-copyable, non-movable.
            UpdateConfigManager(const UpdateConfigManager &) = delete;
            UpdateConfigManager &operator=(const UpdateConfigManager &) = delete;

        private:
            ara::ucm::PackageManager mPackageManager;
            std::unique_ptr<package_management::PackageManagementSkeleton> mSkeleton;
        };

    } // namespace platform
} // namespace application

#endif
