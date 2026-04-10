#include <fstream>
#include <sstream>
#include <cstdio>
#include "./key_value_storage.h"

namespace ara
{
    namespace per
    {
        // ── Private constructor ───────────────────────────────────────────────────

        KeyValueStorage::KeyValueStorage(
            std::string storagePath,
            Json::Value root) noexcept
            : mStoragePath{std::move(storagePath)},
              mRoot{std::move(root)}
        {
        }

        // ── Open ─────────────────────────────────────────────────────────────────

        ara::core::Result<KeyValueStorage> KeyValueStorage::Open(
            const std::string &storagePath) noexcept
        {
            Json::Value _root{Json::objectValue};

            std::ifstream _file{storagePath};
            if (_file.is_open())
            {
                Json::CharReaderBuilder _builder;
                std::string _errors;
                bool _ok = Json::parseFromStream(_builder, _file, &_root, &_errors);
                _file.close();

                if (!_ok)
                {
                    return ara::core::Result<KeyValueStorage>::FromError(
                        MakeErrorCode(PerErrc::kIntegrityCorrupted));
                }

                if (!_root.isObject())
                {
                    return ara::core::Result<KeyValueStorage>::FromError(
                        MakeErrorCode(PerErrc::kIntegrityCorrupted));
                }
            }
            // If the file does not exist we start with an empty object — that is fine.

            KeyValueStorage _kvs{storagePath, std::move(_root)};
            return ara::core::Result<KeyValueStorage>::FromValue(std::move(_kvs));
        }

        // ── HasKey ────────────────────────────────────────────────────────────────

        bool KeyValueStorage::HasKey(const std::string &key) const noexcept
        {
            return mRoot.isMember(key);
        }

        // ── GetAllKeys ────────────────────────────────────────────────────────────

        ara::core::Result<std::vector<std::string>> KeyValueStorage::GetAllKeys()
            const noexcept
        {
            std::vector<std::string> _keys = mRoot.getMemberNames();
            return ara::core::Result<std::vector<std::string>>::FromValue(
                std::move(_keys));
        }

        // ── RemoveKey ─────────────────────────────────────────────────────────────

        ara::core::Result<void> KeyValueStorage::RemoveKey(
            const std::string &key) noexcept
        {
            if (!mRoot.isMember(key))
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            mRoot.removeMember(key);
            return ara::core::Result<void>::FromValue();
        }

        // ── SyncToStorage ─────────────────────────────────────────────────────────

        ara::core::Result<void> KeyValueStorage::SyncToStorage() noexcept
        {
            std::string _tmp = mStoragePath + ".tmp";

            std::ofstream _out{_tmp};
            if (!_out.is_open())
            {
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            Json::StreamWriterBuilder _builder;
            _builder["indentation"] = "  ";
            std::unique_ptr<Json::StreamWriter> _writer(_builder.newStreamWriter());
            _writer->write(mRoot, &_out);
            _out.close();

            if (_out.fail())
            {
                std::remove(_tmp.c_str());
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kOutOfStorageSpace));
            }

            if (std::rename(_tmp.c_str(), mStoragePath.c_str()) != 0)
            {
                std::remove(_tmp.c_str());
                return ara::core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return ara::core::Result<void>::FromValue();
        }

        // ── GetValue specialisations ──────────────────────────────────────────────

        template <>
        ara::core::Result<std::string> KeyValueStorage::GetValue<std::string>(
            const std::string &key) const noexcept
        {
            if (!mRoot.isMember(key))
            {
                return ara::core::Result<std::string>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            const Json::Value &_val = mRoot[key];
            if (!_val.isString())
            {
                return ara::core::Result<std::string>::FromError(
                    MakeErrorCode(PerErrc::kIntegrityCorrupted));
            }

            return ara::core::Result<std::string>::FromValue(_val.asString());
        }

        template <>
        ara::core::Result<int> KeyValueStorage::GetValue<int>(
            const std::string &key) const noexcept
        {
            if (!mRoot.isMember(key))
            {
                return ara::core::Result<int>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            const Json::Value &_val = mRoot[key];
            if (!_val.isIntegral())
            {
                return ara::core::Result<int>::FromError(
                    MakeErrorCode(PerErrc::kIntegrityCorrupted));
            }

            return ara::core::Result<int>::FromValue(_val.asInt());
        }

        template <>
        ara::core::Result<bool> KeyValueStorage::GetValue<bool>(
            const std::string &key) const noexcept
        {
            if (!mRoot.isMember(key))
            {
                return ara::core::Result<bool>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            const Json::Value &_val = mRoot[key];
            if (!_val.isBool())
            {
                return ara::core::Result<bool>::FromError(
                    MakeErrorCode(PerErrc::kIntegrityCorrupted));
            }

            return ara::core::Result<bool>::FromValue(_val.asBool());
        }

        // ── SetValue specialisations ──────────────────────────────────────────────

        template <>
        ara::core::Result<void> KeyValueStorage::SetValue<std::string>(
            const std::string &key, const std::string &value) noexcept
        {
            mRoot[key] = value;
            return ara::core::Result<void>::FromValue();
        }

        template <>
        ara::core::Result<void> KeyValueStorage::SetValue<int>(
            const std::string &key, const int &value) noexcept
        {
            mRoot[key] = value;
            return ara::core::Result<void>::FromValue();
        }

        template <>
        ara::core::Result<void> KeyValueStorage::SetValue<bool>(
            const std::string &key, const bool &value) noexcept
        {
            mRoot[key] = value;
            return ara::core::Result<void>::FromValue();
        }

    } // namespace per
} // namespace ara
