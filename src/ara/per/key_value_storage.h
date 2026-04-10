#ifndef KEY_VALUE_STORAGE_H
#define KEY_VALUE_STORAGE_H

#include <string>
#include <vector>
#include <json/json.h>
#include "../core/result.h"
#include "./per_error_domain.h"

namespace ara
{
    namespace per
    {
        /// @brief In-memory key/value store backed by a JSON file on disk.
        ///
        /// Usage pattern:
        /// @code
        ///   auto _result = KeyValueStorage::Open("/var/per/app.json");
        ///   if (_result.HasValue())
        ///   {
        ///       auto &_kvs = _result.Value();
        ///       _kvs.SetValue("0", std::string{"hello"});
        ///       _kvs.SyncToStorage();
        ///   }
        /// @endcode
        ///
        /// @note Not thread-safe. The single-writer assumption from CONTEXT.md applies.
        class KeyValueStorage
        {
        public:
            KeyValueStorage(KeyValueStorage &&other) noexcept = default;
            KeyValueStorage &operator=(KeyValueStorage &&other) noexcept = default;

            KeyValueStorage(const KeyValueStorage &) = delete;
            KeyValueStorage &operator=(const KeyValueStorage &) = delete;

            /// @brief Open (or create) a key/value storage at the given file path.
            /// @param storagePath Absolute or relative path to the backing JSON file.
            /// @returns Result containing the storage on success, or an error code on
            ///          failure (kIntegrityCorrupted if the file exists but is not valid
            ///          JSON, kPhysicalStorageFailure for I/O errors).
            static ara::core::Result<KeyValueStorage> Open(
                const std::string &storagePath) noexcept;

            /// @brief Read a value for the given key.
            /// @tparam T Value type — supported: std::string, int, bool.
            /// @param key Storage key.
            /// @returns Result with the value, or kKeyNotFound if absent.
            template <typename T>
            ara::core::Result<T> GetValue(const std::string &key) const noexcept;

            /// @brief Write a value for the given key (in memory only).
            /// @tparam T Value type — supported: std::string, int, bool.
            /// @param key Storage key.
            /// @param value Value to store.
            /// @returns Empty Result on success.
            template <typename T>
            ara::core::Result<void> SetValue(
                const std::string &key, const T &value) noexcept;

            /// @brief Remove a key and its value.
            /// @returns Empty Result on success, or kKeyNotFound if absent.
            ara::core::Result<void> RemoveKey(const std::string &key) noexcept;

            /// @brief Return all keys present in the storage.
            ara::core::Result<std::vector<std::string>> GetAllKeys() const noexcept;

            /// @brief Check whether a key exists.
            bool HasKey(const std::string &key) const noexcept;

            /// @brief Persist all in-memory changes atomically to the backing file.
            /// @details Writes to a temporary file adjacent to the target, then renames
            ///          it so the update is atomic on POSIX systems.
            /// @returns Empty Result on success, or kPhysicalStorageFailure / kOutOfStorageSpace.
            ara::core::Result<void> SyncToStorage() noexcept;

        private:
            explicit KeyValueStorage(
                std::string storagePath,
                Json::Value root) noexcept;

            std::string mStoragePath;
            Json::Value mRoot;
        };

        // ── Explicit template specialisation declarations ─────────────────────────

        template <>
        ara::core::Result<std::string> KeyValueStorage::GetValue<std::string>(
            const std::string &key) const noexcept;

        template <>
        ara::core::Result<int> KeyValueStorage::GetValue<int>(
            const std::string &key) const noexcept;

        template <>
        ara::core::Result<bool> KeyValueStorage::GetValue<bool>(
            const std::string &key) const noexcept;

        template <>
        ara::core::Result<void> KeyValueStorage::SetValue<std::string>(
            const std::string &key, const std::string &value) noexcept;

        template <>
        ara::core::Result<void> KeyValueStorage::SetValue<int>(
            const std::string &key, const int &value) noexcept;

        template <>
        ara::core::Result<void> KeyValueStorage::SetValue<bool>(
            const std::string &key, const bool &value) noexcept;

    } // namespace per
} // namespace ara

#endif
