#include <cstdio>
#include "./update_config_manager.h"
#include "package_management_skeleton.h"

namespace application
{
    namespace platform
    {
        UpdateConfigManager::UpdateConfigManager(
            AsyncBsdSocketLib::Poller *poller,
            const std::string &manifestPath,
            const std::string &storageRoot)
            : mPackageManager{storageRoot},
              mSkeleton{
                  std::make_unique<package_management::PackageManagementSkeleton>(
                      poller, manifestPath, &mPackageManager)}
        {
            std::printf("[UCM] Package management service ready\n");
        }

        UpdateConfigManager::~UpdateConfigManager() = default;

    } // namespace platform
} // namespace application
