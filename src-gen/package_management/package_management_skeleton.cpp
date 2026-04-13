#include <functional>
#include <stdexcept>
#include "arxml/arxml_reader.h"
#include "application/helper/network_configuration.h"
#include "ara/com/option/option.h"
#include "./package_management_skeleton.h"

namespace package_management
{
    // ── Constructor ───────────────────────────────────────────────────────────

    PackageManagementSkeleton::PackageManagementSkeleton(
        AsyncBsdSocketLib::Poller *poller,
        const std::string &manifestPath,
        ara::ucm::PackageManager *packageManager)
        : mPackageManager{packageManager}
    {
        application::helper::NetworkConfiguration _cfg;
        bool _ok{application::helper::TryGetNetworkConfiguration(
            manifestPath,
            "UcmServerEP",
            "PackageManagementTcp",
            ara::com::option::Layer4ProtocolType::Tcp,
            _cfg)};

        if (!_ok)
        {
            throw std::runtime_error(
                "PackageManagementSkeleton: failed to read manifest");
        }

        const arxml::ArxmlReader _reader{manifestPath};
        const arxml::ArxmlNode _protoNode{
            _reader.GetRootNode({"AUTOSAR",
                                  "AR-PACKAGES",
                                  "AR-PACKAGE",
                                  "ELEMENTS",
                                  "COMMUNICATION-CLUSTER",
                                  "PROTOCOL-VERSION"})};
        const auto _protocolVersion{_protoNode.GetValue<uint8_t>()};

        mServer = new ara::com::someip::rpc::SocketRpcServer(
            poller,
            _cfg.ipAddress,
            _cfg.portNumber,
            _protocolVersion,
            cInterfaceVersion);

        registerHandlers();
    }

    PackageManagementSkeleton::~PackageManagementSkeleton()
    {
        delete mServer;
    }

    // ── registerHandlers ──────────────────────────────────────────────────────

    void PackageManagementSkeleton::registerHandlers()
    {
        using namespace std::placeholders;

        mServer->SetHandler(
            cServiceId, cMethodTransferStart,
            std::bind(&PackageManagementSkeleton::handleTransferStart,
                      this, _1, _2));

        mServer->SetHandler(
            cServiceId, cMethodTransferData,
            std::bind(&PackageManagementSkeleton::handleTransferData,
                      this, _1, _2));

        mServer->SetHandler(
            cServiceId, cMethodTransferExit,
            std::bind(&PackageManagementSkeleton::handleTransferExit,
                      this, _1, _2));

        mServer->SetHandler(
            cServiceId, cMethodProcessSwPackage,
            std::bind(&PackageManagementSkeleton::handleProcessSwPackage,
                      this, _1, _2));

        mServer->SetHandler(
            cServiceId, cMethodActivate,
            std::bind(&PackageManagementSkeleton::handleActivate,
                      this, _1, _2));

        mServer->SetHandler(
            cServiceId, cMethodGetSwClusterInfo,
            std::bind(&PackageManagementSkeleton::handleGetSwClusterInfo,
                      this, _1, _2));

        mServer->SetHandler(
            cServiceId, cMethodGetCurrentStatus,
            std::bind(&PackageManagementSkeleton::handleGetCurrentStatus,
                      this, _1, _2));
    }

    // ── Method handlers ───────────────────────────────────────────────────────

    bool PackageManagementSkeleton::handleTransferStart(
        const std::vector<uint8_t> &request,
        std::vector<uint8_t> &response)
    {
        try
        {
            size_t _off{0};
            ara::ucm::PackageInfoType _info;
            _info.name = extractString(request, _off);
            _info.version = extractString(request, _off);
            _info.size = extractU64(request, _off);

            auto _r{mPackageManager->TransferStart(_info)};
            if (_r.HasValue())
            {
                appendU8(response, cWireOk);
                appendU32(response, std::move(_r).Value());
            }
            else
            {
                appendU8(response,
                    static_cast<WireErrorCode>(_r.Error().Value()));
            }
        }
        catch (...)
        {
            response = errorResponse(
                static_cast<WireErrorCode>(
                    ara::ucm::UcmErrc::kPackageMalformed));
        }
        return true;
    }

    bool PackageManagementSkeleton::handleTransferData(
        const std::vector<uint8_t> &request,
        std::vector<uint8_t> &response)
    {
        try
        {
            size_t _off{0};
            ara::ucm::TransferIdType _id{extractU32(request, _off)};
            std::vector<uint8_t> _block{extractBytes(request, _off)};
            uint32_t _blockCounter{extractU32(request, _off)};

            auto _r{mPackageManager->TransferData(_id, _block, _blockCounter)};
            appendU8(response,
                _r.HasValue()
                    ? cWireOk
                    : static_cast<WireErrorCode>(_r.Error().Value()));
        }
        catch (...)
        {
            response = errorResponse(
                static_cast<WireErrorCode>(
                    ara::ucm::UcmErrc::kPackageMalformed));
        }
        return true;
    }

    bool PackageManagementSkeleton::handleTransferExit(
        const std::vector<uint8_t> &request,
        std::vector<uint8_t> &response)
    {
        try
        {
            size_t _off{0};
            ara::ucm::TransferIdType _id{extractU32(request, _off)};
            auto _r{mPackageManager->TransferExit(_id)};
            appendU8(response,
                _r.HasValue()
                    ? cWireOk
                    : static_cast<WireErrorCode>(_r.Error().Value()));
        }
        catch (...)
        {
            response = errorResponse(
                static_cast<WireErrorCode>(
                    ara::ucm::UcmErrc::kPackageMalformed));
        }
        return true;
    }

    bool PackageManagementSkeleton::handleProcessSwPackage(
        const std::vector<uint8_t> &request,
        std::vector<uint8_t> &response)
    {
        try
        {
            size_t _off{0};
            ara::ucm::TransferIdType _id{extractU32(request, _off)};
            auto _r{mPackageManager->ProcessSwPackage(_id)};
            appendU8(response,
                _r.HasValue()
                    ? cWireOk
                    : static_cast<WireErrorCode>(_r.Error().Value()));
        }
        catch (...)
        {
            response = errorResponse(
                static_cast<WireErrorCode>(
                    ara::ucm::UcmErrc::kPackageMalformed));
        }
        return true;
    }

    bool PackageManagementSkeleton::handleActivate(
        const std::vector<uint8_t> &request,
        std::vector<uint8_t> &response)
    {
        try
        {
            size_t _off{0};
            std::string _installRoot{extractString(request, _off)};
            std::string _manifestPath{extractString(request, _off)};
            std::printf("[UCM] Activating: install '%s' into '%s'\n",
                _installRoot.c_str(), _manifestPath.c_str());
            auto _r{mPackageManager->Activate(_installRoot, _manifestPath)};
            appendU8(response,
                _r.HasValue()
                    ? cWireOk
                    : static_cast<WireErrorCode>(_r.Error().Value()));
        }
        catch (...)
        {
            response = errorResponse(
                static_cast<WireErrorCode>(
                    ara::ucm::UcmErrc::kActivationFailed));
        }
        return true;
    }

    bool PackageManagementSkeleton::handleGetSwClusterInfo(
        const std::vector<uint8_t> &/*request*/,
        std::vector<uint8_t> &response)
    {
        auto _r{mPackageManager->GetSwClusterInfo()};
        if (!_r.HasValue())
        {
            response = errorResponse(cWireOk);
            return true;
        }

        appendU8(response, cWireOk);
        auto _clusters{std::move(_r).Value()};
        appendU32(response, static_cast<uint32_t>(_clusters.size()));
        for (const auto &_c : _clusters)
        {
            appendString(response, _c.name);
            appendString(response, _c.version);
            appendU64(response, _c.size);
        }
        return true;
    }

    bool PackageManagementSkeleton::handleGetCurrentStatus(
        const std::vector<uint8_t> &/*request*/,
        std::vector<uint8_t> &response)
    {
        appendU8(response, cWireOk);
        appendU8(response,
            static_cast<uint8_t>(mPackageManager->GetCurrentState()));
        return true;
    }

    // ── Serialisation helpers ─────────────────────────────────────────────────

    void PackageManagementSkeleton::appendU8(
        std::vector<uint8_t> &v, uint8_t val)
    {
        v.push_back(val);
    }

    void PackageManagementSkeleton::appendU16(
        std::vector<uint8_t> &v, uint16_t val)
    {
        v.push_back(static_cast<uint8_t>(val >> 8));
        v.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    void PackageManagementSkeleton::appendU32(
        std::vector<uint8_t> &v, uint32_t val)
    {
        v.push_back(static_cast<uint8_t>(val >> 24));
        v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        v.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    void PackageManagementSkeleton::appendU64(
        std::vector<uint8_t> &v, uint64_t val)
    {
        for (int i = 7; i >= 0; --i)
        {
            v.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    }

    void PackageManagementSkeleton::appendString(
        std::vector<uint8_t> &v, const std::string &s)
    {
        appendU16(v, static_cast<uint16_t>(s.size()));
        v.insert(v.end(), s.begin(), s.end());
    }

    uint8_t PackageManagementSkeleton::extractU8(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        return v.at(offset++);
    }

    uint32_t PackageManagementSkeleton::extractU32(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        uint32_t _val{0};
        for (int i = 3; i >= 0; --i)
        {
            _val |= static_cast<uint32_t>(v.at(offset++)) << (i * 8);
        }
        return _val;
    }

    uint64_t PackageManagementSkeleton::extractU64(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        uint64_t _val{0};
        for (int i = 7; i >= 0; --i)
        {
            _val |= static_cast<uint64_t>(v.at(offset++)) << (i * 8);
        }
        return _val;
    }

    std::string PackageManagementSkeleton::extractString(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        uint16_t _len{0};
        _len |= static_cast<uint16_t>(v.at(offset++) << 8);
        _len |= static_cast<uint16_t>(v.at(offset++));
        std::string _s(
            reinterpret_cast<const char *>(v.data() + offset),
            _len);
        offset += _len;
        return _s;
    }

    std::vector<uint8_t> PackageManagementSkeleton::extractBytes(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        uint32_t _len{extractU32(v, offset)};
        std::vector<uint8_t> _bytes(
            v.begin() + static_cast<std::ptrdiff_t>(offset),
            v.begin() + static_cast<std::ptrdiff_t>(offset + _len));
        offset += _len;
        return _bytes;
    }

    std::vector<uint8_t> PackageManagementSkeleton::errorResponse(
        WireErrorCode code)
    {
        return {code};
    }

} // namespace package_management
