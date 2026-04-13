#include <gtest/gtest.h>
#include <chrono>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <thread>
#include <asyncbsdsocket/poller.h>
#include "package_management_skeleton.h"
#include "package_management_proxy.h"
#include "ara/ucm/package_manager.h"

namespace package_management
{
    namespace
    {
        // Use a dedicated port for skeleton tests to avoid collisions.
        static constexpr uint16_t cTestPort{8085};
        static const std::string cTestIp{"127.0.0.1"};
        static const std::string cTestRoot{"/tmp/ara_ucm_skeleton_test"};

        // Minimal ARXML manifest understood by the skeleton constructor.
        // Written to a temp file before each test.
        static const char *cManifestXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<AUTOSAR>
  <AR-PACKAGES>
    <AR-PACKAGE>
      <ELEMENTS>
        <COMMUNICATION-CLUSTER>
          <ETHERNET-PHYSICAL-CHANNEL>
            <NETWORK-ENDPOINTS>
              <NETWORK-ENDPOINT>
                <SHORT-NAME>UcmServerEP</SHORT-NAME>
                <NETWORK-ENDPOINT-ADDRESSES>
                  <IPV-4-CONFIGURATION>
                    <IPV-4-ADDRESS>127.0.0.1</IPV-4-ADDRESS>
                  </IPV-4-CONFIGURATION>
                </NETWORK-ENDPOINT-ADDRESSES>
              </NETWORK-ENDPOINT>
            </NETWORK-ENDPOINTS>
          </ETHERNET-PHYSICAL-CHANNEL>
          <PROTOCOL-VERSION>1</PROTOCOL-VERSION>
        </COMMUNICATION-CLUSTER>
        <ETHERNET-COMMUNICATION-CONNECTOR>
          <AP-APPLICATION-ENDPOINTS>
            <AP-APPLICATION-ENDPOINT>
              <SHORT-NAME>PackageManagementTcp</SHORT-NAME>
              <TP-CONFIGURATION>
                <TCP-TP>
                  <TCP-TP-PORT>
                    <PORT-NUMBER>8085</PORT-NUMBER>
                  </TCP-TP-PORT>
                </TCP-TP>
              </TP-CONFIGURATION>
            </AP-APPLICATION-ENDPOINT>
          </AP-APPLICATION-ENDPOINTS>
        </ETHERNET-COMMUNICATION-CONNECTOR>
      </ELEMENTS>
    </AR-PACKAGE>
  </AR-PACKAGES>
</AUTOSAR>
)";

        static const std::string cManifestPath{"/tmp/ucm_skeleton_test_manifest.arxml"};

        void writeManifest()
        {
            std::ofstream _f{cManifestPath};
            _f << cManifestXml;
        }

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

    } // anonymous namespace

    // ── Fixture ───────────────────────────────────────────────────────────────

    class SkeletonProxyTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            cleanDir(cTestRoot);
            writeManifest();

            // Fresh poller per test: avoids stale fd callbacks from previous
            // test's accepted TCP connection leaking into the next test.
            mPoller = std::make_unique<AsyncBsdSocketLib::Poller>();

            mPackageManager =
                std::make_unique<ara::ucm::PackageManager>(cTestRoot);

            mSkeleton = std::make_unique<PackageManagementSkeleton>(
                mPoller.get(), cManifestPath, mPackageManager.get());

            // Give the server socket a moment to start listening.
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            mProxy = std::make_unique<PackageManagementProxy>(
                mPoller.get(), cTestIp, cTestPort, cProtocolVersion);
        }

        void TearDown() override
        {
            mProxy.reset();
            mSkeleton.reset();
            mPackageManager.reset();
            mPoller.reset();
            cleanDir(cTestRoot);
        }

        std::unique_ptr<AsyncBsdSocketLib::Poller> mPoller;
        std::unique_ptr<ara::ucm::PackageManager> mPackageManager;
        std::unique_ptr<PackageManagementSkeleton> mSkeleton;
        std::unique_ptr<PackageManagementProxy> mProxy;
    };

    // ── Tests ─────────────────────────────────────────────────────────────────

    TEST_F(SkeletonProxyTest, TransferStartReturnsIdAndEntersTransferring)
    {
        ara::ucm::PackageInfoType _info;
        _info.name    = "test_package";
        _info.version = "1.0.0";
        _info.size    = 0;

        auto _r{mProxy->TransferStart(_info)};

        ASSERT_TRUE(_r.HasValue());
        EXPECT_GE(std::move(_r).Value(), 1u);
        EXPECT_EQ(mPackageManager->GetCurrentState(),
                  ara::ucm::UpdateStateType::kTransferring);
    }

    TEST_F(SkeletonProxyTest, TransferStartTwiceReturnsOperationInProgress)
    {
        ara::ucm::PackageInfoType _info;
        _info.name    = "pkg";
        _info.version = "1.0";
        _info.size    = 0;

        auto _r1{mProxy->TransferStart(_info)};
        ASSERT_TRUE(_r1.HasValue());

        // Second call while already transferring must fail.
        auto _r2{mProxy->TransferStart(_info)};
        EXPECT_FALSE(_r2.HasValue());
        EXPECT_EQ(
            static_cast<ara::ucm::UcmErrc>(_r2.Error().Value()),
            ara::ucm::UcmErrc::kOperationInProgress);
    }

    TEST_F(SkeletonProxyTest, TransferDataWithInvalidIdReturnsError)
    {
        auto _r{mProxy->TransferData(999, {}, 0)};
        EXPECT_FALSE(_r.HasValue());
        EXPECT_EQ(
            static_cast<ara::ucm::UcmErrc>(_r.Error().Value()),
            ara::ucm::UcmErrc::kInvalidTransferId);
    }

    TEST_F(SkeletonProxyTest, TransferExitWithInvalidIdReturnsError)
    {
        auto _r{mProxy->TransferExit(42)};
        EXPECT_FALSE(_r.HasValue());
        EXPECT_EQ(
            static_cast<ara::ucm::UcmErrc>(_r.Error().Value()),
            ara::ucm::UcmErrc::kInvalidTransferId);
    }

    TEST_F(SkeletonProxyTest, GetSwClusterInfoEmptyOnFreshInstance)
    {
        auto _r{mProxy->GetSwClusterInfo()};
        ASSERT_TRUE(_r.HasValue());
        EXPECT_TRUE(std::move(_r).Value().empty());
    }

    TEST_F(SkeletonProxyTest, GetCurrentStatusReturnsIdle)
    {
        auto _r{mProxy->GetCurrentStatus()};
        ASSERT_TRUE(_r.HasValue());
        EXPECT_EQ(std::move(_r).Value(), ara::ucm::UpdateStateType::kIdle);
    }

} // namespace package_management
