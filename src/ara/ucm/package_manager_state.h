#ifndef PACKAGE_MANAGER_STATE_H
#define PACKAGE_MANAGER_STATE_H

#include <cstdint>
#include <string>

namespace ara
{
    namespace ucm
    {
        /// @brief Opaque session handle returned by TransferStart
        using TransferIdType = uint32_t;

        /// @brief UCM state machine states (spec §9.7.2)
        enum class UpdateStateType : uint8_t
        {
            kIdle = 0,        ///< No active transfer
            kTransferring = 1,///< Binary chunks are being received
            kProcessing = 2,  ///< Extracting and staging the SW Package
            kActivating = 3,  ///< Copying binary + updating execution manifest
            kActivated = 4,   ///< Activation complete; EM restart will apply
            kRollingBack = 5, ///< Reverting a failed activation
            kRolledBack = 6   ///< Rollback complete; state reset to Idle on next call
        };

        /// @brief SW Package metadata sent in TransferStart (and stored in registry)
        struct PackageInfoType
        {
            std::string name;     ///< Phase 1: abs path on shared FS. Phase 2: cluster name.
            std::string version;  ///< Semantic version string, e.g. "1.0.0"
            uint64_t size{0};     ///< Total bytes of binary data (0 in Phase 1)
        };

    } // namespace ucm
} // namespace ara

#endif
