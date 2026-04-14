#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
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
            const std::string cTestRoot{"/tmp/ara_ucm_phase2_test"};
            const std::string cInstallRoot{"/tmp/ara_ucm_phase2_install"};

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

            /// @brief Build a minimal SW package tar.gz at @p outPath.
            ///
            /// Contents:
            ///   metadata.json  (name, version, binary_name)
            ///   bin/<binaryName>  (tiny ELF-less placeholder script)
            ///   manifest/process_entry.xml  (<PROCESS> fragment)
            ///
            /// @return true on success.
            bool buildMinimalPackage(
                const std::string &outPath,
                const std::string &name,
                const std::string &version,
                const std::string &binaryName)
            {
                const std::string _staging{"/tmp/ucm_phase2_pkg_stage"};
                std::string _cmd;

                _cmd = "rm -rf '" + _staging + "' && "
                       "mkdir -p '" + _staging + "/bin' '"
                                    + _staging + "/manifest'";
                if (::system(_cmd.c_str()) != 0) return false;

                // metadata.json
                std::ofstream _meta{_staging + "/metadata.json"};
                if (!_meta.is_open()) return false;
                _meta << "{\n"
                      << "  \"name\": \"" << name << "\",\n"
                      << "  \"version\": \"" << version << "\",\n"
                      << "  \"cluster_type\": \"application\",\n"
                      << "  \"binary_name\": \"" << binaryName << "\"\n"
                      << "}\n";
                _meta.close();

                // Placeholder binary (shell script — just needs to exist and
                // be chmod 755 so the Activate copy succeeds)
                std::ofstream _bin{_staging + "/bin/" + binaryName};
                if (!_bin.is_open()) return false;
                _bin << "#!/bin/sh\necho hello\n";
                _bin.close();
                ::chmod((_staging + "/bin/" + binaryName).c_str(), 0755);

                // process_entry.xml
                std::ofstream _xml{_staging + "/manifest/process_entry.xml"};
                if (!_xml.is_open()) return false;
                _xml << "<PROCESS>\n"
                     << "  <SHORT-NAME>Phase2App</SHORT-NAME>\n"
                     << "  <EXECUTABLE-PATH>./build/bin/"
                     << binaryName << "</EXECUTABLE-PATH>\n"
                     << "  <BOOTSTRAP>false</BOOTSTRAP>\n"
                     << "  <FUNCTION-GROUP-STATE-IREFS>\n"
                     << "    <FUNCTION-GROUP-STATE-IREF>\n"
                     << "      <FUNCTION-GROUP-REF>MachineFG</FUNCTION-GROUP-REF>\n"
                     << "      <FUNCTION-GROUP-STATE-REF>StartUp"
                     << "</FUNCTION-GROUP-STATE-REF>\n"
                     << "    </FUNCTION-GROUP-STATE-IREF>\n"
                     << "  </FUNCTION-GROUP-STATE-IREFS>\n"
                     << "</PROCESS>\n";
                _xml.close();

                // Create tar.gz
                _cmd = "tar czf '" + outPath + "' -C '" + _staging + "' .";
                int _ret{::system(_cmd.c_str())};
                ::system(("rm -rf '" + _staging + "'").c_str());
                return _ret == 0;
            }

            /// @brief Read a file into a byte vector.
            std::vector<uint8_t> readFile(const std::string &path)
            {
                std::ifstream _f{path, std::ios::binary};
                return std::vector<uint8_t>{
                    std::istreambuf_iterator<char>(_f),
                    std::istreambuf_iterator<char>{}};
            }

            /// @brief Write a minimal execution manifest ARXML at @p path.
            void writeMinimalManifest(const std::string &path)
            {
                std::ofstream _f{path};
                _f << "<?xml version=\"1.0\"?>\n"
                   << "<AUTOSAR>\n"
                   << "  <AR-PACKAGES>\n"
                   << "    <AR-PACKAGE>\n"
                   << "      <SHORT-NAME>ExecutionManifest</SHORT-NAME>\n"
                   << "      <ELEMENTS>\n"
                   << "        <PROCESSES>\n"
                   << "          <PROCESS>\n"
                   << "            <SHORT-NAME>Dummy</SHORT-NAME>\n"
                   << "          </PROCESS>\n"
                   << "        </PROCESSES>\n"
                   << "      </ELEMENTS>\n"
                   << "    </AR-PACKAGE>\n"
                   << "  </AR-PACKAGES>\n"
                   << "</AUTOSAR>\n";
            }

        } // anonymous namespace

        // ── Reset ─────────────────────────────────────────────────────────────

        TEST(PackageManagerPhase2Test, ResetFromTransferringReturnsToIdle)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};

            PackageInfoType _info;
            _info.name = "mypkg-1.0.0";
            _info.version = "1.0.0";
            _info.size = 1024;
            auto _sr{_pm.TransferStart(_info)};
            ASSERT_TRUE(_sr.HasValue());
            EXPECT_EQ(_pm.GetCurrentState(), UpdateStateType::kTransferring);

            _pm.Reset();
            EXPECT_EQ(_pm.GetCurrentState(), UpdateStateType::kIdle);

            // A new transfer should succeed after Reset
            auto _sr2{_pm.TransferStart(_info)};
            EXPECT_TRUE(_sr2.HasValue());

            cleanDir(cTestRoot);
        }

        TEST(PackageManagerPhase2Test, ResetFromIdleStaysIdle)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};
            _pm.Reset(); // Should be a no-op
            EXPECT_EQ(_pm.GetCurrentState(), UpdateStateType::kIdle);
            cleanDir(cTestRoot);
        }

        // ── Phase 2: multi-chunk transfer ─────────────────────────────────────

        TEST(PackageManagerPhase2Test, ChunkedTransferExitSucceeds)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};

            // Synthesise a 3-chunk payload (3 × 32 KB = 96 KB)
            const size_t cChunk{32768};
            const size_t cTotalBytes{3 * cChunk};
            std::vector<uint8_t> _fullData(cTotalBytes, 0xAB);

            PackageInfoType _info;
            _info.name = "synth-1.0.0";
            _info.version = "1.0.0";
            _info.size = static_cast<uint64_t>(cTotalBytes);

            auto _sr{_pm.TransferStart(_info)};
            ASSERT_TRUE(_sr.HasValue());
            TransferIdType _id{std::move(_sr).Value()};

            // Send in three chunks
            for (uint32_t _i = 0; _i < 3; ++_i)
            {
                std::vector<uint8_t> _chunk(
                    _fullData.begin() +
                        static_cast<std::ptrdiff_t>(_i * cChunk),
                    _fullData.begin() +
                        static_cast<std::ptrdiff_t>((_i + 1) * cChunk));
                auto _dr{_pm.TransferData(_id, _chunk, _i + 1)};
                EXPECT_TRUE(_dr.HasValue()) << "chunk " << _i;
            }

            auto _er{_pm.TransferExit(_id)};
            EXPECT_TRUE(_er.HasValue());

            // TransferExit should have written the file to FileStorage
            const std::string _expected{
                cTestRoot + "/synth-1.0.0.swpkg.tar.gz"};
            struct stat _st;
            EXPECT_EQ(::stat(_expected.c_str(), &_st), 0)
                << "FileStorage file not found: " << _expected;
            EXPECT_EQ(static_cast<size_t>(_st.st_size), cTotalBytes);

            cleanDir(cTestRoot);
        }

        TEST(PackageManagerPhase2Test, TransferExitPhase2SizeMismatchFails)
        {
            cleanDir(cTestRoot);
            PackageManager _pm{cTestRoot};

            PackageInfoType _info;
            _info.name = "partial-1.0.0";
            _info.version = "1.0.0";
            _info.size = 1000; // claim 1000 bytes

            auto _sr{_pm.TransferStart(_info)};
            ASSERT_TRUE(_sr.HasValue());
            TransferIdType _id{std::move(_sr).Value()};

            // Only send 500 bytes
            _pm.TransferData(_id, std::vector<uint8_t>(500, 0), 1);

            auto _er{_pm.TransferExit(_id)};
            EXPECT_FALSE(_er.HasValue());
            EXPECT_EQ(
                static_cast<UcmErrc>(_er.Error().Value()),
                UcmErrc::kTransferNotComplete);

            cleanDir(cTestRoot);
        }

        // ── Phase 2: full workflow (TransferStart→...→Activate) ───────────────

        TEST(PackageManagerPhase2Test, FullPhase2WorkflowInstallsBinary)
        {
            cleanDir(cTestRoot);
            cleanDir(cInstallRoot);
            ::mkdir(cInstallRoot.c_str(), 0755);

            // Build a minimal SW package
            const std::string _pkgFile{"/tmp/phase2_test_pkg-1.0.0.swpkg.tar.gz"};
            ASSERT_TRUE(buildMinimalPackage(
                _pkgFile, "phase2_test_pkg", "1.0.0", "phase2_bin"));

            // Read the package into memory
            std::vector<uint8_t> _pkgData{readFile(_pkgFile)};
            ASSERT_FALSE(_pkgData.empty());

            // Write a minimal execution manifest to patch
            const std::string _manifestPath{
                cInstallRoot + "/test_execution_manifest.arxml"};
            writeMinimalManifest(_manifestPath);

            PackageManager _pm{cTestRoot};

            PackageInfoType _info;
            _info.name = "phase2_test_pkg-1.0.0"; // basename without extension
            _info.version = "1.0.0";
            _info.size = static_cast<uint64_t>(_pkgData.size());

            auto _sr{_pm.TransferStart(_info)};
            ASSERT_TRUE(_sr.HasValue());
            TransferIdType _id{std::move(_sr).Value()};

            // Stream in 64 KB chunks
            const size_t cChunkSize{65536};
            uint32_t _counter{1};
            for (size_t _off = 0; _off < _pkgData.size(); _off += cChunkSize)
            {
                size_t _end{std::min(_off + cChunkSize, _pkgData.size())};
                std::vector<uint8_t> _chunk{
                    _pkgData.begin() +
                        static_cast<std::ptrdiff_t>(_off),
                    _pkgData.begin() +
                        static_cast<std::ptrdiff_t>(_end)};
                auto _dr{_pm.TransferData(_id, _chunk, _counter++)};
                ASSERT_TRUE(_dr.HasValue()) << "TransferData failed at offset " << _off;
            }

            auto _er{_pm.TransferExit(_id)};
            ASSERT_TRUE(_er.HasValue()) << "TransferExit failed";

            auto _pr{_pm.ProcessSwPackage(_id)};
            ASSERT_TRUE(_pr.HasValue()) << "ProcessSwPackage failed";

            auto _ar{_pm.Activate(cInstallRoot, _manifestPath)};
            ASSERT_TRUE(_ar.HasValue()) << "Activate failed";

            EXPECT_EQ(_pm.GetCurrentState(), UpdateStateType::kActivated);

            // Binary should be installed
            struct stat _st;
            EXPECT_EQ(::stat((cInstallRoot + "/phase2_bin").c_str(), &_st), 0)
                << "Installed binary not found";

            // Manifest should now contain the new PROCESS entry
            std::ifstream _mf{_manifestPath};
            std::string _content{
                std::istreambuf_iterator<char>(_mf),
                std::istreambuf_iterator<char>{}};
            EXPECT_NE(_content.find("Phase2App"), std::string::npos)
                << "Manifest not updated with new process";

            // Cleanup
            std::remove(_pkgFile.c_str());
            cleanDir(cTestRoot);
            cleanDir(cInstallRoot);
        }

    } // namespace ucm
} // namespace ara
