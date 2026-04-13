#ifndef FILE_STORAGE_H
#define FILE_STORAGE_H

#include <string>
#include <vector>
#include "../core/result.h"
#include "./per_error_domain.h"

namespace ara
{
    namespace per
    {
        /// @brief File-based storage: each logical file is a named binary blob
        ///        stored under a filesystem directory.
        ///
        /// Usage pattern:
        /// @code
        ///   auto _result = FileStorage::Open("/var/per/ucm_packages");
        ///   if (_result.HasValue())
        ///   {
        ///       auto _fs{std::move(_result).Value()};
        ///       std::vector<uint8_t> _data = ...;
        ///       _fs.WriteFile("pkg.tar.gz", _data);
        ///   }
        /// @endcode
        ///
        /// @note Not thread-safe. Single-writer assumption applies.
        class FileStorage
        {
        public:
            FileStorage(FileStorage &&other) noexcept = default;
            FileStorage &operator=(FileStorage &&other) noexcept = default;

            FileStorage(const FileStorage &) = delete;
            FileStorage &operator=(const FileStorage &) = delete;

            /// @brief Open (or create) a file storage rooted at the given directory.
            /// @param storageRoot Absolute path to the directory used as storage root.
            ///        The directory is created if it does not exist.
            /// @returns Result containing the FileStorage on success, or an error code
            ///          on failure (kPhysicalStorageFailure if the directory cannot be
            ///          created or is not accessible).
            static ara::core::Result<FileStorage> Open(
                const std::string &storageRoot) noexcept;

            /// @brief Write (or overwrite) a named file with the given data.
            /// @details The write is atomic: data is written to a temporary file
            ///          in the same directory, then renamed into place.
            /// @param name File name (must not contain path separators).
            /// @param data Raw byte content to store.
            /// @returns Empty Result on success, or kPhysicalStorageFailure /
            ///          kOutOfStorageSpace on I/O error.
            ara::core::Result<void> WriteFile(
                const std::string &name,
                const std::vector<uint8_t> &data) noexcept;

            /// @brief Read the full content of a named file.
            /// @param name File name.
            /// @returns Result with the raw bytes, or kFileNotFound if the file
            ///          does not exist, or kPhysicalStorageFailure on I/O error.
            ara::core::Result<std::vector<uint8_t>> ReadFile(
                const std::string &name) const noexcept;

            /// @brief Delete a named file from the storage directory.
            /// @param name File name.
            /// @returns Empty Result on success, or kFileNotFound if absent.
            ara::core::Result<void> DeleteFile(
                const std::string &name) noexcept;

            /// @brief Check whether a named file exists.
            bool FileExists(const std::string &name) const noexcept;

            /// @brief List all file names present in the storage directory.
            /// @returns Result with a vector of file names (no path prefix).
            ara::core::Result<std::vector<std::string>> GetAllFileNames()
                const noexcept;

        private:
            explicit FileStorage(std::string storageRoot) noexcept;

            /// @brief Construct the full filesystem path for a named file.
            std::string filePath(const std::string &name) const noexcept;

            std::string mStorageRoot;
        };

    } // namespace per
} // namespace ara

#endif
