#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
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
            // ── Phase 2 workflow ──────────────────────────────────────────────
            // Read the SW package from disk and stream it to UCM in 64 KB chunks
            // via TransferData RPCs. UCM reassembles and stores the file in its
            // FileStorage before extracting and activating.

            // Derive the package name (basename without extension) from the path
            // so UCM can use it as the FileStorage filename prefix.
            std::string _basename{mPackagePath};
            const auto _slash{_basename.rfind('/')};
            if (_slash != std::string::npos)
            {
                _basename = _basename.substr(_slash + 1);
            }
            // Strip .swpkg.tar.gz (or just .tar.gz) if present
            for (const auto *_sfx : {".swpkg.tar.gz", ".tar.gz", ".gz"})
            {
                const size_t _sfxLen{std::string{_sfx}.size()};
                if (_basename.size() > _sfxLen &&
                    _basename.compare(_basename.size() - _sfxLen,
                                      _sfxLen, _sfx) == 0)
                {
                    _basename.erase(_basename.size() - _sfxLen);
                    break;
                }
            }

            // Get file size
            struct stat _st;
            if (::stat(mPackagePath.c_str(), &_st) != 0)
            {
                std::printf("[VUCM] ERROR: cannot stat package: %s\n",
                    mPackagePath.c_str());
                return 1;
            }
            const uint64_t _fileSize{static_cast<uint64_t>(_st.st_size)};

            ara::ucm::PackageInfoType _info;
            _info.name    = _basename;
            _info.version = "1.0.0";
            _info.size    = _fileSize;

            // TransferStart
            std::printf("[VUCM] TransferStart: %s (%llu bytes)\n",
                mPackagePath.c_str(),
                static_cast<unsigned long long>(_fileSize));
            auto _start{mProxy->TransferStart(_info)};
            if (!_start.HasValue())
            {
                std::printf("[VUCM] TransferStart FAILED (error %llu)\n",
                    static_cast<unsigned long long>(_start.Error().Value()));
                return 1;
            }
            ara::ucm::TransferIdType _id{std::move(_start).Value()};
            std::printf("[VUCM] TransferStart OK, id=%u\n", _id);

            // TransferData — stream file in 64 KB chunks
            static const size_t cChunkSize{65536};
            int _fd{::open(mPackagePath.c_str(), O_RDONLY)};
            if (_fd < 0)
            {
                std::printf("[VUCM] ERROR: cannot open package file\n");
                return 1;
            }

            const uint32_t _totalChunks{
                static_cast<uint32_t>((_fileSize + cChunkSize - 1) / cChunkSize)};
            std::printf("[VUCM] Sending %u chunk(s) (%llu bytes)\n",
                _totalChunks,
                static_cast<unsigned long long>(_fileSize));

            uint32_t _blockCounter{1};
            std::vector<uint8_t> _chunk;
            _chunk.reserve(cChunkSize);

            bool _readError{false};
            while (true)
            {
                _chunk.resize(cChunkSize);
                ssize_t _n{::read(_fd, _chunk.data(), cChunkSize)};
                if (_n < 0)
                {
                    std::printf("[VUCM] ERROR: read failed on package file\n");
                    _readError = true;
                    break;
                }
                if (_n == 0)
                {
                    break; // EOF
                }
                _chunk.resize(static_cast<size_t>(_n));

                auto _data{mProxy->TransferData(_id, _chunk, _blockCounter)};
                if (!_data.HasValue())
                {
                    std::printf("[VUCM] TransferData FAILED on chunk %u"
                                " (error %llu)\n",
                        _blockCounter,
                        static_cast<unsigned long long>(_data.Error().Value()));
                    _readError = true;
                    break;
                }

                if (_blockCounter % 10 == 0 || _blockCounter == _totalChunks)
                {
                    std::printf("[VUCM] Chunk %u/%u\n",
                        _blockCounter, _totalChunks);
                }
                ++_blockCounter;
            }
            ::close(_fd);

            if (_readError)
            {
                return 1;
            }
            std::printf("[VUCM] TransferData OK (%u chunks)\n",
                _blockCounter - 1);

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
