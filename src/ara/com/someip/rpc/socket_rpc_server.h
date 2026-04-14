#ifndef SOCKET_RPC_SERVER_H
#define SOCKET_RPC_SERVER_H

#include <asyncbsdsocket/poller.h>
#include "../../helper/concurrent_queue.h"
#include "./rpc_server.h"
#include <functional>
#include <stdexcept>
#include <cstdint>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace rpc
            {
                /// @brief TCP socket-based RPC server
                class SocketRpcServer : public RpcServer
                {
                private:
                    static const size_t cBufferSize{65536};

                    // Accumulates partial TCP reads until a complete SOME/IP
                    // message (identified by the Length header field) is ready.
                    std::vector<uint8_t> mReceiveBuffer;

                    helper::ConcurrentQueue<std::vector<uint8_t>> mSendingQueue;
                    AsyncBsdSocketLib::Poller *const mPoller;
                    AsyncBsdSocketLib::TcpListener mServer;

                    std::function<void()> mDisconnectHandler;

                    void onAccept();
                    void onReceive();
                    void onSend();

                public:
                    /// @brief Constructor
                    /// @param poller BSD sockets poller
                    /// @param ipAddress RPC server IP address
                    /// @param port RPC server listening TCP port number
                    /// @param protocolVersion SOME/IP protocol header version
                    /// @param interfaceVersion Service interface version
                    /// @throws std::runtime_error Throws when the TCP server socket configuration failed
                    SocketRpcServer(
                        AsyncBsdSocketLib::Poller *poller,
                        std::string ipAddress,
                        uint16_t port,
                        uint8_t protocolVersion,
                        uint8_t interfaceVersion = 1);

                    virtual ~SocketRpcServer() override;

                    /// @brief Register a callback invoked when the client disconnects.
                    ///
                    /// Called once when `recv()` returns 0 or a negative value,
                    /// indicating the peer closed the connection.  Use this to reset
                    /// any state that depends on a live client (e.g. UCM in-progress
                    /// transfer) so the next connection starts clean.
                    ///
                    /// @param handler Callable with signature `void()`.
                    void SetDisconnectHandler(std::function<void()> handler);
                };
            }
        }
    }
}

#endif