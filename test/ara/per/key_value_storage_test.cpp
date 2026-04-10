#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include "../../../src/ara/per/key_value_storage.h"
#include "../../../src/ara/per/per_error_domain.h"

namespace ara
{
    namespace per
    {
        namespace
        {
            const std::string cTestStoragePath{"/tmp/ara_per_test.json"};

            void removeTestFile()
            {
                std::remove(cTestStoragePath.c_str());
            }
        }

        // ── PerErrorDomain tests ──────────────────────────────────────────────────

        TEST(PerErrorDomainTest, NameProperty)
        {
            ara::core::ErrorDomain *_domain{PerErrorDomain::GetPerDomain()};
            EXPECT_STRNE(_domain->Name(), "");
        }

        TEST(PerErrorDomainTest, MakeErrorCodeMethod)
        {
            const PerErrc cExpected{PerErrc::kKeyNotFound};

            auto *_domain{
                static_cast<PerErrorDomain *>(PerErrorDomain::GetPerDomain())};
            ara::core::ErrorCode _code{_domain->MakeErrorCode(cExpected)};
            auto _actual{static_cast<PerErrc>(_code.Value())};

            EXPECT_EQ(cExpected, _actual);
        }

        TEST(PerErrorDomainTest, FreeMakeErrorCode)
        {
            ara::core::ErrorCode _code{MakeErrorCode(PerErrc::kStorageNotFound)};
            auto _actual{static_cast<PerErrc>(_code.Value())};
            EXPECT_EQ(PerErrc::kStorageNotFound, _actual);
        }

        // ── KeyValueStorage: Open ─────────────────────────────────────────────────

        TEST(KeyValueStorageTest, OpenCreatesNewFileImplicitly)
        {
            removeTestFile();

            auto _result{KeyValueStorage::Open(cTestStoragePath)};
            EXPECT_TRUE(_result.HasValue());

            removeTestFile();
        }

        TEST(KeyValueStorageTest, OpenExistingValidFile)
        {
            removeTestFile();

            // First open/sync creates the file.
            {
                auto _r{KeyValueStorage::Open(cTestStoragePath)};
                ASSERT_TRUE(_r.HasValue());
                auto _kvs{std::move(_r).Value()};
                _kvs.SyncToStorage();
            }

            // Second open should succeed.
            auto _result{KeyValueStorage::Open(cTestStoragePath)};
            EXPECT_TRUE(_result.HasValue());

            removeTestFile();
        }

        TEST(KeyValueStorageTest, OpenCorruptFileReturnsError)
        {
            removeTestFile();

            // Write invalid JSON to the file.
            std::ofstream _out{cTestStoragePath};
            _out << "{ not valid json :::";
            _out.close();

            auto _result{KeyValueStorage::Open(cTestStoragePath)};
            EXPECT_FALSE(_result.HasValue());

            auto _errc{static_cast<PerErrc>(_result.Error().Value())};
            EXPECT_EQ(PerErrc::kIntegrityCorrupted, _errc);

            removeTestFile();
        }

        // ── KeyValueStorage: HasKey / SetValue / GetValue ─────────────────────────

        TEST(KeyValueStorageTest, HasKeyReturnsFalseForMissingKey)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            EXPECT_FALSE(_kvs.HasKey("missing"));

            removeTestFile();
        }

        TEST(KeyValueStorageTest, SetAndGetStringRoundTrip)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            _kvs.SetValue<std::string>("greeting", "hello");
            EXPECT_TRUE(_kvs.HasKey("greeting"));

            auto _getResult{_kvs.GetValue<std::string>("greeting")};
            ASSERT_TRUE(_getResult.HasValue());
            EXPECT_EQ(std::string{"hello"}, _getResult.Value());

            removeTestFile();
        }

        TEST(KeyValueStorageTest, SetAndGetIntRoundTrip)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            _kvs.SetValue<int>("count", 42);
            auto _getResult{_kvs.GetValue<int>("count")};
            ASSERT_TRUE(_getResult.HasValue());
            EXPECT_EQ(42, _getResult.Value());

            removeTestFile();
        }

        TEST(KeyValueStorageTest, SetAndGetBoolRoundTrip)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            _kvs.SetValue<bool>("flag", true);
            auto _getResult{_kvs.GetValue<bool>("flag")};
            ASSERT_TRUE(_getResult.HasValue());
            EXPECT_TRUE(_getResult.Value());

            removeTestFile();
        }

        TEST(KeyValueStorageTest, GetValueMissingKeyReturnsError)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            auto _getResult{_kvs.GetValue<std::string>("no_such_key")};
            EXPECT_FALSE(_getResult.HasValue());

            auto _errc{static_cast<PerErrc>(_getResult.Error().Value())};
            EXPECT_EQ(PerErrc::kKeyNotFound, _errc);

            removeTestFile();
        }

        // ── KeyValueStorage: RemoveKey ────────────────────────────────────────────

        TEST(KeyValueStorageTest, RemoveExistingKey)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            _kvs.SetValue<std::string>("k", "v");
            ASSERT_TRUE(_kvs.HasKey("k"));

            auto _removeResult{_kvs.RemoveKey("k")};
            EXPECT_TRUE(_removeResult.HasValue());
            EXPECT_FALSE(_kvs.HasKey("k"));

            removeTestFile();
        }

        TEST(KeyValueStorageTest, RemoveMissingKeyReturnsError)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            auto _removeResult{_kvs.RemoveKey("absent")};
            EXPECT_FALSE(_removeResult.HasValue());

            auto _errc{static_cast<PerErrc>(_removeResult.Error().Value())};
            EXPECT_EQ(PerErrc::kKeyNotFound, _errc);

            removeTestFile();
        }

        // ── KeyValueStorage: GetAllKeys ───────────────────────────────────────────

        TEST(KeyValueStorageTest, GetAllKeysEmpty)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            auto _keysResult{_kvs.GetAllKeys()};
            ASSERT_TRUE(_keysResult.HasValue());
            EXPECT_TRUE(_keysResult.Value().empty());

            removeTestFile();
        }

        TEST(KeyValueStorageTest, GetAllKeysAfterInserts)
        {
            removeTestFile();

            auto _r{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r.HasValue());
            auto _kvs{std::move(_r).Value()};

            _kvs.SetValue<std::string>("a", "1");
            _kvs.SetValue<std::string>("b", "2");
            _kvs.SetValue<std::string>("c", "3");

            auto _keysResult{_kvs.GetAllKeys()};
            ASSERT_TRUE(_keysResult.HasValue());
            EXPECT_EQ(3u, _keysResult.Value().size());

            removeTestFile();
        }

        // ── KeyValueStorage: SyncToStorage / persistence ─────────────────────────

        TEST(KeyValueStorageTest, SyncAndReopenRestoresData)
        {
            removeTestFile();

            // Write and sync.
            {
                auto _r{KeyValueStorage::Open(cTestStoragePath)};
                ASSERT_TRUE(_r.HasValue());
                auto _kvs{std::move(_r).Value()};
                _kvs.SetValue<std::string>(
                    "0", "Extended Vehicle AA has been initialized.");
                _kvs.SetValue<std::string>(
                    "1", "[Heartbeat] ExtendedVehicle alive | VIN=DEMO");
                auto _syncResult{_kvs.SyncToStorage()};
                EXPECT_TRUE(_syncResult.HasValue());
            }

            // Re-open and verify data survived.
            {
                auto _r{KeyValueStorage::Open(cTestStoragePath)};
                ASSERT_TRUE(_r.HasValue());
                auto _kvs{std::move(_r).Value()};

                auto _v0{_kvs.GetValue<std::string>("0")};
                ASSERT_TRUE(_v0.HasValue());
                EXPECT_EQ(
                    std::string{"Extended Vehicle AA has been initialized."},
                    _v0.Value());

                auto _v1{_kvs.GetValue<std::string>("1")};
                ASSERT_TRUE(_v1.HasValue());
                EXPECT_EQ(
                    std::string{"[Heartbeat] ExtendedVehicle alive | VIN=DEMO"},
                    _v1.Value());
            }

            removeTestFile();
        }

        TEST(KeyValueStorageTest, InMemoryChangesNotVisibleBeforeSync)
        {
            removeTestFile();

            // Write but do NOT sync — file is never created.
            {
                auto _r{KeyValueStorage::Open(cTestStoragePath)};
                ASSERT_TRUE(_r.HasValue());
                auto _kvs{std::move(_r).Value()};
                _kvs.SetValue<std::string>("key", "value");
                // No SyncToStorage() call.
            }

            // File should not exist yet; re-open gets an empty store.
            auto _r2{KeyValueStorage::Open(cTestStoragePath)};
            ASSERT_TRUE(_r2.HasValue());
            auto _kvs2{std::move(_r2).Value()};
            EXPECT_FALSE(_kvs2.HasKey("key"));

            removeTestFile();
        }

    } // namespace per
} // namespace ara
