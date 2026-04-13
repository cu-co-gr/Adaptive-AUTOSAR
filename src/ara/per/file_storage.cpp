#include <fstream>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#include "./file_storage.h"

namespace ara
{
    namespace per
    {
        // ── Private constructor ───────────────────────────────────────────────────

        FileStorage::FileStorage(std::string storageRoot) noexcept
            : mStorageRoot{std::move(storageRoot)}
        {
        }

        // ── filePath helper ───────────────────────────────────────────────────────

        std::string FileStorage::filePath(const std::string &name) const noexcept
        {
            return mStorageRoot + "/" + name;
        }

        // ── Open ──────────────────────────────────────────────────────────────────

        ara::core::Result<FileStorage> FileStorage::Open(
            const std::string &storageRoot) noexcept
        {
            if (::mkdir(storageRoot.c_str(), 0755) != 0 && errno != EEXIST)
            {
                return ara::core::Result<FileStorage>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            FileStorage _fs{storageRoot};
            return ara::core::Result<FileStorage>::FromValue(std::move(_fs));
        }

        // ── WriteFile ─────────────────────────────────────────────────────────────

        ara::core::Result<void> FileStorage::WriteFile(
            const std::string &name,
            const std::vector<uint8_t> &data) noexcept
        {
            std::string _tmp = filePath(name) + ".tmp";

            std::ofstream _out{_tmp, std::ios::binary | std::ios::trunc};
            if (!_out.is_open())
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            if (!data.empty())
            {
                _out.write(
                    reinterpret_cast<const char *>(data.data()),
                    static_cast<std::streamsize>(data.size()));
            }

            _out.close();

            if (_out.fail())
            {
                std::remove(_tmp.c_str());
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kOutOfStorageSpace));
            }

            std::string _dest = filePath(name);
            if (std::rename(_tmp.c_str(), _dest.c_str()) != 0)
            {
                std::remove(_tmp.c_str());
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return ara::core::Result<void>::FromValue();
        }

        // ── ReadFile ──────────────────────────────────────────────────────────────

        ara::core::Result<std::vector<uint8_t>> FileStorage::ReadFile(
            const std::string &name) const noexcept
        {
            std::string _path = filePath(name);
            std::ifstream _in{_path, std::ios::binary};

            if (!_in.is_open())
            {
                return ara::core::Result<std::vector<uint8_t>>::FromError(
                    MakeErrorCode(PerErrc::kFileNotFound));
            }

            std::vector<uint8_t> _data{
                std::istreambuf_iterator<char>(_in),
                std::istreambuf_iterator<char>()};

            if (_in.fail() && !_in.eof())
            {
                return ara::core::Result<std::vector<uint8_t>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return ara::core::Result<std::vector<uint8_t>>::FromValue(
                std::move(_data));
        }

        // ── DeleteFile ────────────────────────────────────────────────────────────

        ara::core::Result<void> FileStorage::DeleteFile(
            const std::string &name) noexcept
        {
            std::string _path = filePath(name);

            if (std::remove(_path.c_str()) != 0)
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kFileNotFound));
            }

            return ara::core::Result<void>::FromValue();
        }

        // ── FileExists ────────────────────────────────────────────────────────────

        bool FileStorage::FileExists(const std::string &name) const noexcept
        {
            struct stat _st;
            return ::stat(filePath(name).c_str(), &_st) == 0;
        }

        // ── GetAllFileNames ───────────────────────────────────────────────────────

        ara::core::Result<std::vector<std::string>> FileStorage::GetAllFileNames()
            const noexcept
        {
            DIR *_dir = ::opendir(mStorageRoot.c_str());
            if (_dir == nullptr)
            {
                return ara::core::Result<std::vector<std::string>>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            std::vector<std::string> _names;
            struct dirent *_entry;
            while ((_entry = ::readdir(_dir)) != nullptr)
            {
                std::string _name{_entry->d_name};
                // Skip . and .. and any .tmp files (partial writes)
                if (_name == "." || _name == "..")
                    continue;
                // Skip .tmp sentinel files left by interrupted writes
                if (_name.size() > 4 &&
                    _name.substr(_name.size() - 4) == ".tmp")
                    continue;
                _names.push_back(_name);
            }

            ::closedir(_dir);

            return ara::core::Result<std::vector<std::string>>::FromValue(
                std::move(_names));
        }

    } // namespace per
} // namespace ara
