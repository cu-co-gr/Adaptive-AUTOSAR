#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "../core/result.h"
#include "../per/file_storage.h"
#include "../per/key_value_storage.h"
#include "./ucm_error_domain.h"
#include "./package_manager_state.h"

namespace ara
{
    namespace ucm
    {
        /// @brief UCM PackageManager: owns the state machine, FileStorage buffer,
        ///        and KeyValueStorage registry of installed SW Clusters.
        ///
        /// The skeleton (PackageManagementSkeleton, Step 8) calls into this class
        /// over the SOME/IP RPC layer.
        ///
        /// SW Package format (tar.gz):
        /// @code
        ///   <name>-<version>.swpkg.tar.gz
        ///   ├── metadata.json     {name, version, cluster_type, binary_name}
        ///   ├── bin/<executable>
        ///   └── manifest/process_entry.xml
        /// @endcode
        ///
        /// Phase 1 note:
        ///   VUCM sets PackageInfoType::name = absolute path to the .swpkg file on
        ///   a shared filesystem. TransferData blocks are empty (size=0). TransferExit
        ///   just verifies the file exists at the given path. Phase 2 will reassemble
        ///   the binary from TransferData chunks.
        ///
        /// @note Not thread-safe. Single-writer assumption applies.
        class PackageManager
        {
        public:
            /// @brief Constructor
            /// @param storageRoot Directory used for FileStorage and registry KVS.
            ///        Created automatically if absent.
            explicit PackageManager(const std::string &storageRoot) noexcept;

            // Non-copyable, movable.
            PackageManager(const PackageManager &) = delete;
            PackageManager &operator=(const PackageManager &) = delete;
            PackageManager(PackageManager &&) = default;

            // ── State machine ───────────────────────────────────────────────

            /// @brief Allocate a new transfer session.
            /// @param info Package metadata (name = path in Phase 1, version, size).
            /// @returns New TransferIdType on success, or kOperationInProgress if
            ///          UCM is not in kIdle state.
            ara::core::Result<TransferIdType> TransferStart(
                const PackageInfoType &info) noexcept;

            /// @brief Deliver one binary chunk for the given transfer session.
            /// @param id Transfer session handle from TransferStart.
            /// @param block Raw binary data chunk (may be empty in Phase 1).
            /// @param blockCounter Monotonically increasing counter starting at 0.
            /// @returns Empty Result on success, or kInvalidTransferId.
            ara::core::Result<void> TransferData(
                TransferIdType id,
                const std::vector<uint8_t> &block,
                uint32_t blockCounter) noexcept;

            /// @brief Finalise the transfer and verify completeness.
            /// @param id Transfer session handle.
            /// @returns Empty Result on success, or kInvalidTransferId /
            ///          kTransferNotComplete (Phase 2: size mismatch).
            ara::core::Result<void> TransferExit(TransferIdType id) noexcept;

            /// @brief Extract and stage the SW Package for activation.
            /// @param id Transfer session handle.
            /// @returns Empty Result on success, or kInvalidTransferId /
            ///          kPackageMalformed.
            ara::core::Result<void> ProcessSwPackage(
                TransferIdType id) noexcept;

            /// @brief Activate the staged SW Package.
            /// @param installRoot Directory where the binary will be placed.
            /// @param executionManifestPath Absolute path to the target machine's
            ///        execution_manifest.arxml that will be updated.
            /// @returns Empty Result on success, or kActivationFailed.
            ara::core::Result<void> Activate(
                const std::string &installRoot,
                const std::string &executionManifestPath) noexcept;

            /// @brief Return the current UCM state.
            UpdateStateType GetCurrentState() const noexcept;

            /// @brief Return the list of installed SW Clusters from the registry.
            ara::core::Result<std::vector<PackageInfoType>>
            GetSwClusterInfo() const noexcept;

            /// @brief Reset state to kIdle and clear all pending transfer data.
            ///
            /// Called by the skeleton when a client disconnects mid-transfer so
            /// that the next connection starts from a clean state.
            void Reset() noexcept;

        private:

            // ── Data members ─────────────────────────────────────────────────

            std::string mStorageRoot;

            /// @brief FileStorage for incoming SW Package binary data.
            ///        Only valid after a successful Open; nullptr otherwise.
            std::unique_ptr<ara::per::FileStorage> mStorage;

            /// @brief KeyValueStorage registry: { swClusterName → version }.
            ///        Located at <storageRoot>/ucm_registry.json.
            std::unique_ptr<ara::per::KeyValueStorage> mRegistry;

            UpdateStateType mState{UpdateStateType::kIdle};
            TransferIdType mNextId{1};

            // Active transfer session data
            std::map<TransferIdType, PackageInfoType> mPendingTransfers;

            // For Phase 2 block reassembly: accumulated bytes per transfer
            std::map<TransferIdType, std::vector<uint8_t>> mAccumulatedData;

            // Path to the staged .swpkg file (set during ProcessSwPackage)
            std::string mStagedPackagePath;
            std::string mStagedBinaryName;
        };

    } // namespace ucm
} // namespace ara

#endif
