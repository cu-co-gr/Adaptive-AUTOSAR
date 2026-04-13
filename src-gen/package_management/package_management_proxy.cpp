#include <functional>
#include <stdexcept>
#include <thread>
#include "ara/ucm/ucm_error_domain.h"
#include "./package_management_proxy.h"

namespace package_management
{
    const std::chrono::seconds PackageManagementProxy::cCallTimeout{5};

    // ── Constructor / Destructor ──────────────────────────────────────────────

    PackageManagementProxy::PackageManagementProxy(
        AsyncBsdSocketLib::Poller *poller,
        const std::string &ipAddress,
        uint16_t portNumber,
        uint8_t protocolVersion)
        : mPoller{poller}
    {
        mSocket = new ara::com::someip::rpc::SocketRpcClient(
            poller,
            ipAddress,
            portNumber,
            protocolVersion,
            cInterfaceVersion);

        mClient = mSocket;

        registerHandlers();
    }

    PackageManagementProxy::~PackageManagementProxy()
    {
        delete mSocket;
    }

    // ── registerHandlers ─────────────────────────────────────────────────────
    //
    // One handler per method ID; each handler stores the RPC payload and sets
    // mResponseReady so that sendAndWait() can stop polling.

    void PackageManagementProxy::registerHandlers()
    {
        using namespace std::placeholders;
        using ara::com::someip::rpc::SomeIpRpcMessage;

        auto makeHandler = [this](uint16_t /*methodId*/)
            -> ara::com::someip::rpc::RpcClient::HandlerType
        {
            return [this](const SomeIpRpcMessage &msg)
            {
                const auto &_rpc{msg.RpcPayload()};
                mLastResponse.assign(_rpc.begin(), _rpc.end());
                mResponseReady.store(true);
            };
        };

        mClient->SetHandler(cServiceId, cMethodTransferStart,
            makeHandler(cMethodTransferStart));
        mClient->SetHandler(cServiceId, cMethodTransferData,
            makeHandler(cMethodTransferData));
        mClient->SetHandler(cServiceId, cMethodTransferExit,
            makeHandler(cMethodTransferExit));
        mClient->SetHandler(cServiceId, cMethodProcessSwPackage,
            makeHandler(cMethodProcessSwPackage));
        mClient->SetHandler(cServiceId, cMethodActivate,
            makeHandler(cMethodActivate));
        mClient->SetHandler(cServiceId, cMethodGetSwClusterInfo,
            makeHandler(cMethodGetSwClusterInfo));
        mClient->SetHandler(cServiceId, cMethodGetCurrentStatus,
            makeHandler(cMethodGetCurrentStatus));
    }

    // ── sendAndWait ───────────────────────────────────────────────────────────

    bool PackageManagementProxy::sendAndWait(
        uint16_t methodId,
        const std::vector<uint8_t> &requestPayload,
        std::vector<uint8_t> &responsePayload)
    {
        mResponseReady.store(false);
        mLastResponse.clear();

        mClient->Send(cServiceId, methodId, cClientId, requestPayload);

        const auto _deadline{
            std::chrono::steady_clock::now() + cCallTimeout};

        while (!mResponseReady.load())
        {
            mPoller->TryPoll();
            if (std::chrono::steady_clock::now() >= _deadline)
            {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        responsePayload = std::move(mLastResponse);
        return true;
    }

    // ── voidResult helper ─────────────────────────────────────────────────────

    ara::core::Result<void> PackageManagementProxy::voidResult(
        uint16_t methodId,
        const std::vector<uint8_t> &requestPayload)
    {
        std::vector<uint8_t> _resp;
        if (!sendAndWait(methodId, requestPayload, _resp) || _resp.empty())
        {
            return ara::core::Result<void>::FromError(
                ara::ucm::MakeErrorCode(ara::ucm::UcmErrc::kActivationFailed));
        }

        const uint8_t _status{_resp.at(0)};
        if (_status != cWireOk)
        {
            return ara::core::Result<void>::FromError(
                ara::ucm::MakeErrorCode(
                    static_cast<ara::ucm::UcmErrc>(_status)));
        }

        return ara::core::Result<void>::FromValue();
    }

    // ── Service methods ───────────────────────────────────────────────────────

    ara::core::Result<ara::ucm::TransferIdType>
    PackageManagementProxy::TransferStart(
        const ara::ucm::PackageInfoType &info)
    {
        std::vector<uint8_t> _req;
        appendString(_req, info.name);
        appendString(_req, info.version);
        appendU64(_req, info.size);

        std::vector<uint8_t> _resp;
        if (!sendAndWait(cMethodTransferStart, _req, _resp) || _resp.empty())
        {
            return ara::core::Result<ara::ucm::TransferIdType>::FromError(
                ara::ucm::MakeErrorCode(ara::ucm::UcmErrc::kActivationFailed));
        }

        const uint8_t _status{_resp.at(0)};
        if (_status != cWireOk)
        {
            return ara::core::Result<ara::ucm::TransferIdType>::FromError(
                ara::ucm::MakeErrorCode(
                    static_cast<ara::ucm::UcmErrc>(_status)));
        }

        size_t _off{1};
        ara::ucm::TransferIdType _id{extractU32(_resp, _off)};
        return ara::core::Result<ara::ucm::TransferIdType>::FromValue(_id);
    }

    ara::core::Result<void> PackageManagementProxy::TransferData(
        ara::ucm::TransferIdType id,
        const std::vector<uint8_t> &block,
        uint32_t blockCounter)
    {
        std::vector<uint8_t> _req;
        appendU32(_req, id);
        appendBytes(_req, block);
        appendU32(_req, blockCounter);

        return voidResult(cMethodTransferData, _req);
    }

    ara::core::Result<void> PackageManagementProxy::TransferExit(
        ara::ucm::TransferIdType id)
    {
        std::vector<uint8_t> _req;
        appendU32(_req, id);

        return voidResult(cMethodTransferExit, _req);
    }

    ara::core::Result<void> PackageManagementProxy::ProcessSwPackage(
        ara::ucm::TransferIdType id)
    {
        std::vector<uint8_t> _req;
        appendU32(_req, id);

        return voidResult(cMethodProcessSwPackage, _req);
    }

    ara::core::Result<void> PackageManagementProxy::Activate(
        const std::string &installRoot,
        const std::string &manifestPath)
    {
        std::vector<uint8_t> _req;
        appendString(_req, installRoot);
        appendString(_req, manifestPath);

        return voidResult(cMethodActivate, _req);
    }

    ara::core::Result<std::vector<ara::ucm::PackageInfoType>>
    PackageManagementProxy::GetSwClusterInfo()
    {
        std::vector<uint8_t> _req;
        std::vector<uint8_t> _resp;

        if (!sendAndWait(cMethodGetSwClusterInfo, _req, _resp) || _resp.empty())
        {
            return ara::core::Result<std::vector<ara::ucm::PackageInfoType>>
                ::FromError(ara::ucm::MakeErrorCode(
                    ara::ucm::UcmErrc::kActivationFailed));
        }

        const uint8_t _status{_resp.at(0)};
        if (_status != cWireOk)
        {
            return ara::core::Result<std::vector<ara::ucm::PackageInfoType>>
                ::FromError(ara::ucm::MakeErrorCode(
                    static_cast<ara::ucm::UcmErrc>(_status)));
        }

        size_t _off{1};
        uint32_t _count{extractU32(_resp, _off)};
        std::vector<ara::ucm::PackageInfoType> _clusters;
        _clusters.reserve(_count);
        for (uint32_t _i = 0; _i < _count; ++_i)
        {
            ara::ucm::PackageInfoType _c;
            _c.name    = extractString(_resp, _off);
            _c.version = extractString(_resp, _off);
            _c.size    = extractU64(_resp, _off);
            _clusters.push_back(std::move(_c));
        }

        return ara::core::Result<std::vector<ara::ucm::PackageInfoType>>
            ::FromValue(std::move(_clusters));
    }

    ara::core::Result<ara::ucm::UpdateStateType>
    PackageManagementProxy::GetCurrentStatus()
    {
        std::vector<uint8_t> _req;
        std::vector<uint8_t> _resp;

        if (!sendAndWait(cMethodGetCurrentStatus, _req, _resp)
            || _resp.size() < 2)
        {
            return ara::core::Result<ara::ucm::UpdateStateType>::FromError(
                ara::ucm::MakeErrorCode(ara::ucm::UcmErrc::kActivationFailed));
        }

        const uint8_t _status{_resp.at(0)};
        if (_status != cWireOk)
        {
            return ara::core::Result<ara::ucm::UpdateStateType>::FromError(
                ara::ucm::MakeErrorCode(
                    static_cast<ara::ucm::UcmErrc>(_status)));
        }

        return ara::core::Result<ara::ucm::UpdateStateType>::FromValue(
            static_cast<ara::ucm::UpdateStateType>(_resp.at(1)));
    }

    // ── Serialisation helpers ─────────────────────────────────────────────────

    void PackageManagementProxy::appendU8(
        std::vector<uint8_t> &v, uint8_t val)
    {
        v.push_back(val);
    }

    void PackageManagementProxy::appendU16(
        std::vector<uint8_t> &v, uint16_t val)
    {
        v.push_back(static_cast<uint8_t>(val >> 8));
        v.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    void PackageManagementProxy::appendU32(
        std::vector<uint8_t> &v, uint32_t val)
    {
        v.push_back(static_cast<uint8_t>(val >> 24));
        v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        v.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    void PackageManagementProxy::appendU64(
        std::vector<uint8_t> &v, uint64_t val)
    {
        for (int i = 7; i >= 0; --i)
        {
            v.push_back(
                static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    }

    void PackageManagementProxy::appendString(
        std::vector<uint8_t> &v, const std::string &s)
    {
        appendU16(v, static_cast<uint16_t>(s.size()));
        v.insert(v.end(), s.begin(), s.end());
    }

    void PackageManagementProxy::appendBytes(
        std::vector<uint8_t> &v, const std::vector<uint8_t> &bytes)
    {
        appendU32(v, static_cast<uint32_t>(bytes.size()));
        v.insert(v.end(), bytes.begin(), bytes.end());
    }

    uint8_t PackageManagementProxy::extractU8(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        return v.at(offset++);
    }

    uint32_t PackageManagementProxy::extractU32(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        uint32_t _val{0};
        for (int i = 3; i >= 0; --i)
        {
            _val |= static_cast<uint32_t>(v.at(offset++)) << (i * 8);
        }
        return _val;
    }

    uint64_t PackageManagementProxy::extractU64(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        uint64_t _val{0};
        for (int i = 7; i >= 0; --i)
        {
            _val |= static_cast<uint64_t>(v.at(offset++)) << (i * 8);
        }
        return _val;
    }

    std::string PackageManagementProxy::extractString(
        const std::vector<uint8_t> &v, size_t &offset)
    {
        uint16_t _len{0};
        _len |= static_cast<uint16_t>(v.at(offset++) << 8);
        _len |= static_cast<uint16_t>(v.at(offset++));
        std::string _s(
            reinterpret_cast<const char *>(v.data() + offset), _len);
        offset += _len;
        return _s;
    }

} // namespace package_management
