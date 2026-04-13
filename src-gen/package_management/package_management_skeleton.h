#ifndef PACKAGE_MANAGEMENT_SKELETON_H
#define PACKAGE_MANAGEMENT_SKELETON_H

#include <string>
#include <memory>
#include "ara/com/someip/rpc/socket_rpc_server.h"
#include "ara/ucm/package_manager.h"
#include "./package_management_types.h"

namespace package_management
{
    /// @brief Generated PackageManagement RPC skeleton (server-side, runs in UCM).
    ///
    /// Wraps a SocketRpcServer; registers handlers for all 7 method IDs.
    /// Each handler deserialises the request payload, calls PackageManager,
    /// and serialises the response.
    ///
    /// Manifest ARXML element names expected:
    ///   networkEndpoint     "UcmServerEP"
    ///   applicationEndpoint "PackageManagementTcp"
    ///
    /// Hand-generated to simulate AUTOSAR code-generator output.
    class PackageManagementSkeleton
    {
    public:
        /// @brief Constructor — reads IP/port from manifest, creates RPC server.
        /// @param poller   Global async I/O poller.
        /// @param manifestPath  Path to ucm_manifest.arxml for this machine.
        /// @param packageManager  PackageManager instance to dispatch calls to.
        /// @throws std::runtime_error if manifest cannot be parsed or server setup fails.
        PackageManagementSkeleton(
            AsyncBsdSocketLib::Poller *poller,
            const std::string &manifestPath,
            ara::ucm::PackageManager *packageManager);

        ~PackageManagementSkeleton();

        // Non-copyable, non-movable (owns the RPC server).
        PackageManagementSkeleton(const PackageManagementSkeleton &) = delete;
        PackageManagementSkeleton &operator=(
            const PackageManagementSkeleton &) = delete;

    private:
        ara::com::someip::rpc::SocketRpcServer *mServer{nullptr};
        ara::ucm::PackageManager *mPackageManager{nullptr};

        void registerHandlers();

        // ── Per-method handlers ──────────────────────────────────────────────

        bool handleTransferStart(
            const std::vector<uint8_t> &request,
            std::vector<uint8_t> &response);

        bool handleTransferData(
            const std::vector<uint8_t> &request,
            std::vector<uint8_t> &response);

        bool handleTransferExit(
            const std::vector<uint8_t> &request,
            std::vector<uint8_t> &response);

        bool handleProcessSwPackage(
            const std::vector<uint8_t> &request,
            std::vector<uint8_t> &response);

        bool handleActivate(
            const std::vector<uint8_t> &request,
            std::vector<uint8_t> &response);

        bool handleGetSwClusterInfo(
            const std::vector<uint8_t> &request,
            std::vector<uint8_t> &response);

        bool handleGetCurrentStatus(
            const std::vector<uint8_t> &request,
            std::vector<uint8_t> &response);

        // ── Serialisation helpers ────────────────────────────────────────────

        /// @brief Append a uint8 to a byte vector.
        static void appendU8(std::vector<uint8_t> &v, uint8_t val);

        /// @brief Append a uint16 big-endian to a byte vector.
        static void appendU16(std::vector<uint8_t> &v, uint16_t val);

        /// @brief Append a uint32 big-endian to a byte vector.
        static void appendU32(std::vector<uint8_t> &v, uint32_t val);

        /// @brief Append a uint64 big-endian to a byte vector.
        static void appendU64(std::vector<uint8_t> &v, uint64_t val);

        /// @brief Append a string (uint16 length prefix + bytes).
        static void appendString(std::vector<uint8_t> &v, const std::string &s);

        /// @brief Extract a uint8 from payload at offset; advance offset.
        static uint8_t extractU8(
            const std::vector<uint8_t> &v, size_t &offset);

        /// @brief Extract a uint32 big-endian from payload at offset.
        static uint32_t extractU32(
            const std::vector<uint8_t> &v, size_t &offset);

        /// @brief Extract a uint64 big-endian from payload at offset.
        static uint64_t extractU64(
            const std::vector<uint8_t> &v, size_t &offset);

        /// @brief Extract a string (uint16 length prefix + bytes).
        static std::string extractString(
            const std::vector<uint8_t> &v, size_t &offset);

        /// @brief Extract a byte vector (uint32 length prefix + bytes).
        static std::vector<uint8_t> extractBytes(
            const std::vector<uint8_t> &v, size_t &offset);

        /// @brief Build a minimal error-only response.
        static std::vector<uint8_t> errorResponse(WireErrorCode code);
    };

} // namespace package_management

#endif
