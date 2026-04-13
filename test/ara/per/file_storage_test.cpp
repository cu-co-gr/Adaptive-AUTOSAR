#include <gtest/gtest.h>
#include <cstdio>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include "../../../src/ara/per/file_storage.h"
#include "../../../src/ara/per/per_error_domain.h"

namespace ara
{
    namespace per
    {
        namespace
        {
            const std::string cTestRoot{"/tmp/ara_file_storage_test"};

            void cleanTestDir()
            {
                // Remove all regular files then the directory itself (best-effort).
                DIR *_dir = ::opendir(cTestRoot.c_str());
                if (_dir)
                {
                    struct dirent *_entry;
                    while ((_entry = ::readdir(_dir)) != nullptr)
                    {
                        std::string _name{_entry->d_name};
                        if (_name == "." || _name == "..")
                            continue;
                        std::remove((cTestRoot + "/" + _name).c_str());
                    }
                    ::closedir(_dir);
                }
                ::rmdir(cTestRoot.c_str());
            }
        }

        // ── PerErrc new codes ─────────────────────────────────────────────────────

        TEST(PerErrcExtendedTest, ResourceBusyMessage)
        {
            ara::core::ErrorCode _code{MakeErrorCode(PerErrc::kResourceBusy)};
            auto *_domain{PerErrorDomain::GetPerDomain()};
            EXPECT_STRNE(_domain->Message(_code.Value()), "Not supported");
        }

        TEST(PerErrcExtendedTest, FileNotFoundMessage)
        {
            ara::core::ErrorCode _code{MakeErrorCode(PerErrc::kFileNotFound)};
            auto *_domain{PerErrorDomain::GetPerDomain()};
            EXPECT_STRNE(_domain->Message(_code.Value()), "Not supported");
        }

        // ── FileStorage::Open ─────────────────────────────────────────────────────

        TEST(FileStorageTest, OpenCreatesDirectory)
        {
            cleanTestDir();

            auto _result{FileStorage::Open(cTestRoot)};
            EXPECT_TRUE(_result.HasValue());

            // Verify directory exists
            struct stat _st;
            EXPECT_EQ(::stat(cTestRoot.c_str(), &_st), 0);

            cleanTestDir();
        }

        TEST(FileStorageTest, OpenExistingDirectory)
        {
            cleanTestDir();
            ::mkdir(cTestRoot.c_str(), 0755);

            auto _result{FileStorage::Open(cTestRoot)};
            EXPECT_TRUE(_result.HasValue());

            cleanTestDir();
        }

        // ── FileStorage::WriteFile / ReadFile ─────────────────────────────────────

        TEST(FileStorageTest, WriteAndReadRoundTrip)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            const std::vector<uint8_t> cData{0x01, 0x02, 0x03, 0xFF};
            auto _wr{_fs.WriteFile("blob.bin", cData)};
            ASSERT_TRUE(_wr.HasValue());

            auto _rr{_fs.ReadFile("blob.bin")};
            ASSERT_TRUE(_rr.HasValue());
            EXPECT_EQ(std::move(_rr).Value(), cData);

            cleanTestDir();
        }

        TEST(FileStorageTest, WriteEmptyFile)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            std::vector<uint8_t> _empty;
            EXPECT_TRUE(_fs.WriteFile("empty.bin", _empty).HasValue());

            auto _rr{_fs.ReadFile("empty.bin")};
            ASSERT_TRUE(_rr.HasValue());
            EXPECT_TRUE(std::move(_rr).Value().empty());

            cleanTestDir();
        }

        TEST(FileStorageTest, WriteOverwritesExistingFile)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            _fs.WriteFile("file.bin", {0xAA, 0xBB});
            _fs.WriteFile("file.bin", {0xCC, 0xDD, 0xEE});

            auto _rr{_fs.ReadFile("file.bin")};
            ASSERT_TRUE(_rr.HasValue());
            std::vector<uint8_t> _expected{0xCC, 0xDD, 0xEE};
            EXPECT_EQ(std::move(_rr).Value(), _expected);

            cleanTestDir();
        }

        TEST(FileStorageTest, ReadMissingFileReturnsError)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            auto _rr{_fs.ReadFile("nonexistent.bin")};
            ASSERT_FALSE(_rr.HasValue());
            EXPECT_EQ(
                static_cast<PerErrc>(_rr.Error().Value()),
                PerErrc::kFileNotFound);

            cleanTestDir();
        }

        // ── FileStorage::DeleteFile ───────────────────────────────────────────────

        TEST(FileStorageTest, DeleteExistingFile)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            _fs.WriteFile("del.bin", {0x01});
            EXPECT_TRUE(_fs.DeleteFile("del.bin").HasValue());
            EXPECT_FALSE(_fs.FileExists("del.bin"));

            cleanTestDir();
        }

        TEST(FileStorageTest, DeleteMissingFileReturnsError)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            auto _dr{_fs.DeleteFile("ghost.bin")};
            ASSERT_FALSE(_dr.HasValue());
            EXPECT_EQ(
                static_cast<PerErrc>(_dr.Error().Value()),
                PerErrc::kFileNotFound);

            cleanTestDir();
        }

        // ── FileStorage::FileExists ───────────────────────────────────────────────

        TEST(FileStorageTest, FileExistsReturnsTrueAfterWrite)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            EXPECT_FALSE(_fs.FileExists("x.bin"));
            _fs.WriteFile("x.bin", {0x42});
            EXPECT_TRUE(_fs.FileExists("x.bin"));

            cleanTestDir();
        }

        // ── FileStorage::GetAllFileNames ──────────────────────────────────────────

        TEST(FileStorageTest, GetAllFileNamesEmpty)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            auto _nr{_fs.GetAllFileNames()};
            ASSERT_TRUE(_nr.HasValue());
            EXPECT_TRUE(std::move(_nr).Value().empty());

            cleanTestDir();
        }

        TEST(FileStorageTest, GetAllFileNamesReturnsWrittenFiles)
        {
            cleanTestDir();
            auto _r{FileStorage::Open(cTestRoot)};
            ASSERT_TRUE(_r.HasValue());
            auto _fs{std::move(_r).Value()};

            _fs.WriteFile("a.bin", {0x01});
            _fs.WriteFile("b.bin", {0x02});

            auto _nr{_fs.GetAllFileNames()};
            ASSERT_TRUE(_nr.HasValue());
            auto _names{std::move(_nr).Value()};
            EXPECT_EQ(_names.size(), 2u);

            bool _hasA = std::find(_names.begin(), _names.end(), "a.bin")
                         != _names.end();
            bool _hasB = std::find(_names.begin(), _names.end(), "b.bin")
                         != _names.end();
            EXPECT_TRUE(_hasA);
            EXPECT_TRUE(_hasB);

            cleanTestDir();
        }

        // ── Persistence across re-Open ────────────────────────────────────────────

        TEST(FileStorageTest, ReOpenReadsFilesFromPreviousSession)
        {
            cleanTestDir();

            {
                auto _r{FileStorage::Open(cTestRoot)};
                ASSERT_TRUE(_r.HasValue());
                auto _fs{std::move(_r).Value()};
                _fs.WriteFile("session.bin", {0xDE, 0xAD, 0xBE, 0xEF});
            }

            // Re-open in a new scope simulating next session
            {
                auto _r2{FileStorage::Open(cTestRoot)};
                ASSERT_TRUE(_r2.HasValue());
                auto _fs2{std::move(_r2).Value()};

                EXPECT_TRUE(_fs2.FileExists("session.bin"));
                auto _rr{_fs2.ReadFile("session.bin")};
                ASSERT_TRUE(_rr.HasValue());
                std::vector<uint8_t> _expected{0xDE, 0xAD, 0xBE, 0xEF};
                EXPECT_EQ(std::move(_rr).Value(), _expected);
            }

            cleanTestDir();
        }

    } // namespace per
} // namespace ara
