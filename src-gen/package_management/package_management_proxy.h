#ifndef PACKAGE_MANAGEMENT_PROXY_H
#define PACKAGE_MANAGEMENT_PROXY_H

#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <asyncbsdsocket/poller.h>
#include "ara/com/someip/rpc/rpc_client.h"
#include "ara/com/someip/rpc/socket_rpc_client.h"
#include "ara/ucm/package_manager.h"
#include "./package_management_types.h"

namespace package_management
{
    /// @brief Generated PackageManagement RPC proxy (client-side, runs in VUCM).
    ///
    /// Wraps a SocketRpcClient; exposes one blocking method per service method.
    /// Each call serialises the request payload, sends via SOME/IP RPC, and
    /// spin-polls the poller until the response arrives (or a timeout expires).
    ///
    /// Manifest ARXML element names expected (on the UCM server side):
    ///   networkEndpoint     "UcmServerEP"
    ///   applicationEndpoint "PackageManagementTcp"
    ///
    /// VUCM reads the target IP/port from vucm_manifest.arxml and passes them
    /// to the constructor directly (one proxy instance per target UCM).
    ///
    /// Hand-generated to simulate AUTOSAR code-generator output.
    class PackageManagementProxy
    {
    public:
        /// @brief Constructor — connects to a UCM instance.
        /// @param poller        Global async I/O poller (shared with the app loop).
        /// @param ipAddress     Target UCM IP address.
        /// @param portNumber    Target UCM TCP port (default 8090).
        /// @param protocolVersion SOME/IP protocol version.
        /// @throws std::runtime_error if the TCP connection cannot be established.
        PackageManagementProxy(
            AsyncBsdSocketLib::Poller *poller,
            const std::string &ipAddress,
            uint16_t portNumber,
            uint8_t protocolVersion = cProtocolVersion);

        ~PackageManagementProxy();

        // Non-copyable, non-movable.
        PackageManagementProxy(const PackageManagementProxy &) = delete;
        PackageManagementProxy &operator=(const PackageManagementProxy &) = delete;

        // ── Service methods (blocking) ────────────────────────────────────────

        ara::core::Result<ara::ucm::TransferIdType> TransferStart(
            const ara::ucm::PackageInfoType &info);

        ara::core::Result<void> TransferData(
            ara::ucm::TransferIdType id,
            const std::vector<uint8_t> &block,
            uint32_t blockCounter);

        ara::core::Result<void> TransferExit(ara::ucm::TransferIdType id);

        ara::core::Result<void> ProcessSwPackage(ara::ucm::TransferIdType id);

        ara::core::Result<void> Activate(
            const std::string &installRoot,
            const std::string &manifestPath);

        ara::core::Result<std::vector<ara::ucm::PackageInfoType>> GetSwClusterInfo();

        ara::core::Result<ara::ucm::UpdateStateType> GetCurrentStatus();

    private:
        static const std::chrono::seconds cCallTimeout;

        AsyncBsdSocketLib::Poller *mPoller;
        ara::com::someip::rpc::SocketRpcClient *mSocket{nullptr};
        ara::com::someip::rpc::RpcClient *mClient{nullptr};

        std::vector<uint8_t> mLastResponse;
        std::atomic<bool> mResponseReady{false};

        void registerHandlers();

        /// @brief Send a request and spin-poll until the response arrives.
        /// @param methodId     SOME/IP method ID.
        /// @param requestPayload  Serialised method arguments.
        /// @param responsePayload Filled with the raw response payload on success.
        /// @return true if a response was received before the timeout.
        bool sendAndWait(
            uint16_t methodId,
            const std::vector<uint8_t> &requestPayload,
            std::vector<uint8_t> &responsePayload);

        /// @brief Decode a void-return response; returns an error Result on wire
        ///        error or timeout.
        ara::core::Result<void> voidResult(
            uint16_t methodId,
            const std::vector<uint8_t> &requestPayload);

        // ── Serialisation helpers ────────────────────────────────────────────

        static void appendU8(std::vector<uint8_t> &v, uint8_t val);
        static void appendU16(std::vector<uint8_t> &v, uint16_t val);
        static void appendU32(std::vector<uint8_t> &v, uint32_t val);
        static void appendU64(std::vector<uint8_t> &v, uint64_t val);
        static void appendString(std::vector<uint8_t> &v, const std::string &s);
        static void appendBytes(
            std::vector<uint8_t> &v, const std::vector<uint8_t> &bytes);

        static uint8_t extractU8(
            const std::vector<uint8_t> &v, size_t &offset);
        static uint32_t extractU32(
            const std::vector<uint8_t> &v, size_t &offset);
        static uint64_t extractU64(
            const std::vector<uint8_t> &v, size_t &offset);
        static std::string extractString(
            const std::vector<uint8_t> &v, size_t &offset);
    };

} // namespace package_management

#endif
