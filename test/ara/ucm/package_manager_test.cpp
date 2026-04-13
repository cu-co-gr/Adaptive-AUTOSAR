#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include "../../../src/ara/ucm/package_manager.h"

namespace ara
{
    namespace ucm
    {
        namespace
        {
            const std::string cTestRoot{"/tmp/ara_ucm_test"};

            void cleanDir(const std::string &path)
            {
                DIR *_dir = ::opendir(path.c_str());
                if (_dir)
                {
                    struct dirent *_entry;
                    while ((_entry = ::readdir(_dir)) != nullptr)
                    {
                        std::string _n{_entry->d_name};
                        if (_n == "." || _n == "..") continue;
                        std::string _full{path + "/" + _n};
                        struct stat _st;
                        ::stat(_full.c_str(), &_st);
                        if (S_ISDIR(_st.st_mode))
                            cleanDir(_full);
                        else
                            std::remove(_full.c_str());
                    }
                    ::closedir(_dir);
                }
                ::rmdir(path.c_str());
            }

            PackageInfoType makeInfo(
                const std::string &name, const std::string &ver,
                uint64_t size = 0)
            {
                PackageInfoType _info;
                _info.name = name;
                _info.version = ver;
                _info.size = size;
                return _info;
            }
        }

        // ── UcmErrorDomain (smoke-test in this file too) ──────────────────────

        TEST(UcmErrorTest, MakeErrorCode)
        {
            auto _code{MakeErrorCode(UcmErrc::kInvalidTransferId)};
            EXPECT_EQ(
                static_cast<UcmErrc>(_code.Value()),
                UcmErrc::kInvalidTransferId);
        }

        // ── PackageManager: initial state ─────────────────────────────────────

        TEST(PackageManagerTest, InitialStateIsIdle)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            EXPECT_EQ(_pm.GetCurrentState(), UpdateStateType::kIdle);
            cleanDir(cTestRoot);
        }

        // ── TransferStart ─────────────────────────────────────────────────────

        TEST(PackageManagerTest, TransferStartTransitionsToTransferring)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};

            auto _r{_pm.TransferStart(makeInfo("/tmp/dummy.swpkg.tar.gz", "1.0.0"))};
            ASSERT_TRUE(_r.HasValue());
            EXPECT_EQ(_pm.GetCurrentState(), UpdateStateType::kTransferring);
            cleanDir(cTestRoot);
        }

        TEST(PackageManagerTest, TransferStartReturnsDifferentIds)
        {
            cleanDir(cTestRoot);
            {
                PackageManager _pm{cTestRoot};
                auto _r1{_pm.TransferStart(makeInfo("pkg1", "1.0"))};
                ASSERT_TRUE(_r1.HasValue());
                TransferIdType _id1{std::move(_r1).Value()};

                // Second call should fail — UCM is not in kIdle
                auto _r2{_pm.TransferStart(makeInfo("pkg2", "2.0"))};
                EXPECT_FALSE(_r2.HasValue());
                EXPECT_EQ(
                    static_cast<UcmErrc>(_r2.Error().Value()),
                    UcmErrc::kOperationInProgress);
                (void)_id1;
            }
            cleanDir(cTestRoot);
        }

        // ── TransferData ──────────────────────────────────────────────────────

        TEST(PackageManagerTest, TransferDataWithInvalidIdReturnsError)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            auto _r{_pm.TransferData(999, {}, 0)};
            EXPECT_FALSE(_r.HasValue());
            EXPECT_EQ(
                static_cast<UcmErrc>(_r.Error().Value()),
                UcmErrc::kInvalidTransferId);
            cleanDir(cTestRoot);
        }

        TEST(PackageManagerTest, TransferDataAcceptsEmptyBlocksPhase1)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            auto _sr{_pm.TransferStart(makeInfo("path/pkg", "1.0", 0))};
            ASSERT_TRUE(_sr.HasValue());
            TransferIdType _id{std::move(_sr).Value()};

            // Phase 1: empty block is fine
            auto _dr{_pm.TransferData(_id, {}, 0)};
            EXPECT_TRUE(_dr.HasValue());
            cleanDir(cTestRoot);
        }

        // ── TransferExit ──────────────────────────────────────────────────────

        TEST(PackageManagerTest, TransferExitWithInvalidIdReturnsError)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            auto _r{_pm.TransferExit(42)};
            EXPECT_FALSE(_r.HasValue());
            EXPECT_EQ(
                static_cast<UcmErrc>(_r.Error().Value()),
                UcmErrc::kInvalidTransferId);
            cleanDir(cTestRoot);
        }

        TEST(PackageManagerTest, TransferExitPhase1MissingFileReturnsError)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            // size=0 → Phase 1 path; the file does not exist
            auto _sr{_pm.TransferStart(
                makeInfo("/nonexistent/file.swpkg.tar.gz", "1.0", 0))};
            ASSERT_TRUE(_sr.HasValue());
            TransferIdType _id{std::move(_sr).Value()};

            auto _er{_pm.TransferExit(_id)};
            EXPECT_FALSE(_er.HasValue());
            EXPECT_EQ(
                static_cast<UcmErrc>(_er.Error().Value()),
                UcmErrc::kTransferNotComplete);
            cleanDir(cTestRoot);
        }

        TEST(PackageManagerTest, TransferExitPhase2SizeMismatchReturnsError)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            // size=100 → Phase 2; but we only send 3 bytes
            auto _sr{_pm.TransferStart(makeInfo("mypkg", "1.0", 100))};
            ASSERT_TRUE(_sr.HasValue());
            TransferIdType _id{std::move(_sr).Value()};

            _pm.TransferData(_id, {0x01, 0x02, 0x03}, 0);

            auto _er{_pm.TransferExit(_id)};
            EXPECT_FALSE(_er.HasValue());
            EXPECT_EQ(
                static_cast<UcmErrc>(_er.Error().Value()),
                UcmErrc::kTransferNotComplete);
            cleanDir(cTestRoot);
        }

        // ── GetCurrentState ───────────────────────────────────────────────────

        TEST(PackageManagerTest, StateSequenceIdleToTransferring)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            EXPECT_EQ(_pm.GetCurrentState(), UpdateStateType::kIdle);

            _pm.TransferStart(makeInfo("pkg", "1.0"));
            EXPECT_EQ(_pm.GetCurrentState(), UpdateStateType::kTransferring);
            cleanDir(cTestRoot);
        }

        // ── GetSwClusterInfo ──────────────────────────────────────────────────

        TEST(PackageManagerTest, GetSwClusterInfoEmptyOnFreshInstance)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            auto _r{_pm.GetSwClusterInfo()};
            ASSERT_TRUE(_r.HasValue());
            EXPECT_TRUE(std::move(_r).Value().empty());
            cleanDir(cTestRoot);
        }

    } // namespace ucm
} // namespace ara
