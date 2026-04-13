#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <json/json.h>
#include "./package_manager.h"

// pugixml is available via the arxml library dependency
#include "../../arxml/pugixml.hpp"

namespace ara
{
    namespace ucm
    {
        // ── Constructor ───────────────────────────────────────────────────────

        PackageManager::PackageManager(const std::string &storageRoot) noexcept
            : mStorageRoot{storageRoot}
        {
            // Open FileStorage (creates directory if absent)
            auto _fsResult{ara::per::FileStorage::Open(storageRoot)};
            if (_fsResult.HasValue())
            {
                mStorage = std::unique_ptr<ara::per::FileStorage>(
                    new ara::per::FileStorage(std::move(_fsResult).Value()));
            }

            // Open KeyValueStorage registry
            std::string _registryPath{storageRoot + "/ucm_registry.json"};
            auto _kvsResult{ara::per::KeyValueStorage::Open(_registryPath)};
            if (_kvsResult.HasValue())
            {
                mRegistry = std::unique_ptr<ara::per::KeyValueStorage>(
                    new ara::per::KeyValueStorage(std::move(_kvsResult).Value()));
            }
        }

        // ── reset ─────────────────────────────────────────────────────────────

        void PackageManager::reset() noexcept
        {
            mState = UpdateStateType::kIdle;
            mPendingTransfers.clear();
            mAccumulatedData.clear();
            mStagedPackagePath.clear();
            mStagedBinaryName.clear();
        }

        // ── TransferStart ─────────────────────────────────────────────────────

        ara::core::Result<TransferIdType> PackageManager::TransferStart(
            const PackageInfoType &info) noexcept
        {
            if (mState != UpdateStateType::kIdle)
            {
                return ara::core::Result<TransferIdType>::FromError(
                    MakeErrorCode(UcmErrc::kOperationInProgress));
            }

            TransferIdType _id{mNextId++};
            mPendingTransfers[_id] = info;
            mAccumulatedData[_id] = std::vector<uint8_t>{};
            mState = UpdateStateType::kTransferring;

            return ara::core::Result<TransferIdType>::FromValue(_id);
        }

        // ── TransferData ──────────────────────────────────────────────────────

        ara::core::Result<void> PackageManager::TransferData(
            TransferIdType id,
            const std::vector<uint8_t> &block,
            uint32_t /*blockCounter*/) noexcept
        {
            if (mPendingTransfers.find(id) == mPendingTransfers.end())
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidTransferId));
            }

            // Phase 1: blocks are empty (file path already known from TransferStart)
            // Phase 2: accumulate binary chunks
            auto &_acc{mAccumulatedData[id]};
            _acc.insert(_acc.end(), block.begin(), block.end());

            return ara::core::Result<void>::FromValue();
        }

        // ── TransferExit ──────────────────────────────────────────────────────

        ara::core::Result<void> PackageManager::TransferExit(
            TransferIdType id) noexcept
        {
            auto _it{mPendingTransfers.find(id)};
            if (_it == mPendingTransfers.end())
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidTransferId));
            }

            const PackageInfoType &_info{_it->second};

            if (_info.size == 0)
            {
                // Phase 1: verify the referenced file exists
                struct stat _st;
                if (::stat(_info.name.c_str(), &_st) != 0)
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kTransferNotComplete));
                }
                // Store the path for ProcessSwPackage
                mStagedPackagePath = _info.name;
            }
            else
            {
                // Phase 2: verify accumulated byte count matches declared size
                const auto &_acc{mAccumulatedData[id]};
                if (_acc.size() != static_cast<size_t>(_info.size))
                {
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kTransferNotComplete));
                }

                // Write assembled package to FileStorage
                std::string _pkgFileName{_info.name + "-" + _info.version + ".swpkg.tar.gz"};
                if (mStorage)
                {
                    auto _wr{mStorage->WriteFile(_pkgFileName, _acc)};
                    if (!_wr.HasValue())
                    {
                        return ara::core::Result<void>::FromError(
                            MakeErrorCode(UcmErrc::kInsufficientSpace));
                    }
                }
                mStagedPackagePath = mStorageRoot + "/" + _pkgFileName;
            }

            return ara::core::Result<void>::FromValue();
        }

        // ── ProcessSwPackage ──────────────────────────────────────────────────

        ara::core::Result<void> PackageManager::ProcessSwPackage(
            TransferIdType id) noexcept
        {
            if (mPendingTransfers.find(id) == mPendingTransfers.end())
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kInvalidTransferId));
            }

            if (mStagedPackagePath.empty())
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kPackageMalformed));
            }

            mState = UpdateStateType::kProcessing;

            // Extract the tar.gz to the storage root
            std::string _extractDir{mStorageRoot + "/staging"};
            ::mkdir(_extractDir.c_str(), 0755);

            std::string _cmd{"tar xzf \""};
            _cmd += mStagedPackagePath;
            _cmd += "\" -C \"";
            _cmd += _extractDir;
            _cmd += "\"";

            std::printf("[UCM] Extracting package...\n");
            int _tarRet{::system(_cmd.c_str())};
            if (_tarRet != 0)
            {
                mState = UpdateStateType::kIdle;
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kPackageMalformed));
            }

            // Read metadata.json to get the binary name
            std::string _metaPath{_extractDir + "/metadata.json"};
            std::ifstream _metaFile{_metaPath};
            if (!_metaFile.is_open())
            {
                mState = UpdateStateType::kIdle;
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kPackageMalformed));
            }

            Json::Value _meta;
            Json::CharReaderBuilder _builder;
            std::string _errors;
            if (!Json::parseFromStream(_builder, _metaFile, &_meta, &_errors))
            {
                mState = UpdateStateType::kIdle;
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kPackageMalformed));
            }

            mStagedBinaryName = _meta.get("binary_name", "").asString();
            if (mStagedBinaryName.empty())
            {
                mState = UpdateStateType::kIdle;
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kPackageMalformed));
            }

            return ara::core::Result<void>::FromValue();
        }

        // ── Activate ──────────────────────────────────────────────────────────

        ara::core::Result<void> PackageManager::Activate(
            const std::string &installRoot,
            const std::string &executionManifestPath) noexcept
        {
            if (mStagedBinaryName.empty())
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kActivationFailed));
            }

            mState = UpdateStateType::kActivating;

            std::string _extractDir{mStorageRoot + "/staging"};

            // Step 1: copy binary to installRoot/bin/
            std::string _srcBin{_extractDir + "/bin/" + mStagedBinaryName};
            std::string _dstBin{installRoot + "/" + mStagedBinaryName};
            std::printf("[UCM] Installing binary: %s\n", _dstBin.c_str());

            // Step 1: copy binary via tmp-file + rename (atomic; works even if
            // the destination binary is currently executing — avoids ETXTBSY).
            {
                const std::string _tmpBin{_dstBin + ".ucm_tmp"};
                int _rfd{::open(_srcBin.c_str(), O_RDONLY)};
                if (_rfd < 0)
                {
                    std::printf("[UCM] ERROR: open src failed: %s\n",
                        std::strerror(errno));
                    mState = UpdateStateType::kIdle;
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kActivationFailed));
                }
                int _wfd{::open(_tmpBin.c_str(),
                    O_WRONLY | O_CREAT | O_TRUNC, 0755)};
                if (_wfd < 0)
                {
                    std::printf("[UCM] ERROR: open tmp failed: %s\n",
                        std::strerror(errno));
                    ::close(_rfd);
                    mState = UpdateStateType::kIdle;
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kActivationFailed));
                }
                char _buf[65536];
                ssize_t _n;
                bool _copyOk{true};
                while ((_n = ::read(_rfd, _buf, sizeof(_buf))) > 0)
                {
                    if (::write(_wfd, _buf, static_cast<size_t>(_n)) != _n)
                    {
                        _copyOk = false;
                        break;
                    }
                }
                ::close(_rfd);
                ::close(_wfd);
                if (!_copyOk || _n < 0 || ::rename(_tmpBin.c_str(),
                                                    _dstBin.c_str()) != 0)
                {
                    ::unlink(_tmpBin.c_str());
                    mState = UpdateStateType::kIdle;
                    return ara::core::Result<void>::FromError(
                        MakeErrorCode(UcmErrc::kActivationFailed));
                }
            }

            // Step 2: mode 0755 already set via O_CREAT above
            std::printf("[UCM] Binary installed\n");

            // Step 3: inject <PROCESS> fragment into execution_manifest.arxml
            std::string _procEntryPath{
                _extractDir + "/manifest/process_entry.xml"};
            pugi::xml_document _procDoc;
            if (_procDoc.load_file(_procEntryPath.c_str()).status !=
                pugi::status_ok)
            {
                mState = UpdateStateType::kIdle;
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kActivationFailed));
            }

            pugi::xml_document _manifestDoc;
            if (_manifestDoc.load_file(executionManifestPath.c_str()).status !=
                pugi::status_ok)
            {
                mState = UpdateStateType::kIdle;
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kActivationFailed));
            }

            // Find <PROCESSES> node and append the new <PROCESS> child
            auto _processes{
                _manifestDoc.select_node("//PROCESSES")};
            if (!_processes)
            {
                mState = UpdateStateType::kIdle;
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kActivationFailed));
            }

            auto _newProcess{_procDoc.document_element()};

            // Idempotent: skip if a PROCESS with this SHORT-NAME already exists
            const std::string _newName{
                _newProcess.child("SHORT-NAME").text().as_string()};
            bool _alreadyPresent{false};
            for (const auto &_child : _processes.node().children("PROCESS"))
            {
                if (std::string{_child.child("SHORT-NAME").text().as_string()}
                    == _newName)
                {
                    _alreadyPresent = true;
                    break;
                }
            }

            if (!_alreadyPresent)
            {
                _processes.node().append_copy(_newProcess);
            }
            else
            {
                std::printf("[UCM] Process '%s' already present — skipping inject\n",
                    _newName.c_str());
            }

            std::printf("[UCM] Updating manifest: %s\n",
                executionManifestPath.c_str());
            if (!_manifestDoc.save_file(executionManifestPath.c_str()))
            {
                mState = UpdateStateType::kIdle;
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(UcmErrc::kActivationFailed));
            }

            // Step 4: update installed-cluster registry
            if (mRegistry && !mPendingTransfers.empty())
            {
                const auto &_info{mPendingTransfers.begin()->second};
                mRegistry->SetValue(_info.name, _info.version);
                mRegistry->SyncToStorage();
            }

            mState = UpdateStateType::kActivated;
            return ara::core::Result<void>::FromValue();
        }

        // ── GetCurrentState ───────────────────────────────────────────────────

        UpdateStateType PackageManager::GetCurrentState() const noexcept
        {
            return mState;
        }

        // ── GetSwClusterInfo ──────────────────────────────────────────────────

        ara::core::Result<std::vector<PackageInfoType>>
        PackageManager::GetSwClusterInfo() const noexcept
        {
            std::vector<PackageInfoType> _clusters;

            if (!mRegistry)
            {
                return ara::core::Result<std::vector<PackageInfoType>>::FromValue(
                    std::move(_clusters));
            }

            auto _keysResult{mRegistry->GetAllKeys()};
            if (!_keysResult.HasValue())
            {
                return ara::core::Result<std::vector<PackageInfoType>>::FromValue(
                    std::move(_clusters));
            }

            for (const auto &_key : std::move(_keysResult).Value())
            {
                auto _verResult{
                    mRegistry->GetValue<std::string>(_key)};
                PackageInfoType _info;
                _info.name = _key;
                _info.version = _verResult.HasValue()
                                    ? std::move(_verResult).Value()
                                    : "";
                _clusters.push_back(std::move(_info));
            }

            return ara::core::Result<std::vector<PackageInfoType>>::FromValue(
                std::move(_clusters));
        }

    } // namespace ucm
} // namespace ara
