#include <algorithm>
#include <cerrno>
#include <sys/socket.h>
#include <iostream>
#include "./socket_rpc_server.h"
#include "../../helper/payload_helper.h"
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
                const size_t SocketRpcServer::cBufferSize;

                SocketRpcServer::SocketRpcServer(
                    AsyncBsdSocketLib::Poller *poller,
                    std::string ipAddress,
                    uint16_t port,
                    uint8_t protocolVersion,
                    uint8_t interfaceVersion) : RpcServer(protocolVersion, interfaceVersion),
                                                mPoller{poller},
                                                mServer{AsyncBsdSocketLib::TcpListener(ipAddress, port)}
                {
                    bool _successful{mServer.TrySetup()};
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "TCP server socket setup failed.");
                    }

                    auto _listener{std::bind(&SocketRpcServer::onAccept, this)};
                    _successful = mPoller->TryAddListener(&mServer, _listener);
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Adding TCP server socket listener failed.");
                    }
                }

                void SocketRpcServer::onAccept()
                {
                    std::cerr << "[RpcServer] onAccept called\n" << std::flush;
                    bool _successful{mServer.TryAccept()};
                    if (!_successful)
                    {
                        std::cerr << "[RpcServer] TryAccept failed\n" << std::flush;
                        return;
                    }
                    std::cerr << "[RpcServer] TryAccept ok, conn=" << mServer.Connection() << "\n" << std::flush;

                    bool _nb{mServer.TryMakeConnectionNonblock()};
                    std::cerr << "[RpcServer] TryMakeConnectionNonblock=" << _nb << "\n" << std::flush;

                    auto _receiver{std::bind(&SocketRpcServer::onReceive, this)};
                    _successful = mPoller->TryAddReceiver(&mServer, _receiver);
                    if (!_successful)
                    {
                        std::cerr << "[RpcServer] TryAddReceiver failed\n" << std::flush;
                        return;
                    }

                    auto _sender{std::bind(&SocketRpcServer::onSend, this)};
                    _successful = mPoller->TryAddSender(&mServer, _sender);
                    std::cerr << "[RpcServer] TryAddSender=" << _successful << "\n" << std::flush;

                    // Eagerly drain any data that arrived before poll() registration.
                    // On QNX io-sock, POLLIN does not fire reliably for accepted sockets;
                    // data may already be in the recv buffer before we call poll().
                    onReceive();
                }

                void SocketRpcServer::SetDisconnectHandler(
                    std::function<void()> handler)
                {
                    mDisconnectHandler = std::move(handler);
                }

                void SocketRpcServer::onReceive()
                {
                    std::array<uint8_t, cBufferSize> _buffer;
                    ssize_t _receivedSize = ::recv(
                        mServer.Connection(),
                        _buffer.data(),
                        _buffer.size(),
                        0);

                    if (_receivedSize < 0)
                    {
                        // EAGAIN/EWOULDBLOCK: non-blocking socket, no data available.
                        // This is not a disconnect — return silently.
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                        {
                            if (mDisconnectHandler)
                                mDisconnectHandler();
                        }
                        return;
                    }
                    if (_receivedSize == 0)
                    {
                        if (mDisconnectHandler)
                            mDisconnectHandler();
                        return;
                    }

                    std::cerr << "[RpcServer] onReceive: " << _receivedSize
                              << " bytes\n" << std::flush;

                    // Accumulate into the reassembly buffer
                    mReceiveBuffer.insert(
                        mReceiveBuffer.end(),
                        _buffer.begin(),
                        _buffer.begin() + _receivedSize);

                    // Process all complete SOME/IP messages in the buffer.
                    // SOME/IP header: bytes [0-3] MessageId, [4-7] Length (BE
                    // uint32). Total frame = 8 + Length bytes.
                    while (true)
                    {
                        const size_t cMinHeader{8};
                        if (mReceiveBuffer.size() < cMinHeader)
                        {
                            break;
                        }

                        size_t _lengthOffset{4};
                        uint32_t _length{
                            helper::ExtractInteger(mReceiveBuffer, _lengthOffset)};
                        size_t _totalSize{
                            cMinHeader + static_cast<size_t>(_length)};

                        if (mReceiveBuffer.size() < _totalSize)
                        {
                            break;
                        }

                        const std::vector<uint8_t> _message(
                            mReceiveBuffer.begin(),
                            mReceiveBuffer.begin() +
                                static_cast<std::ptrdiff_t>(_totalSize));

                        mReceiveBuffer.erase(
                            mReceiveBuffer.begin(),
                            mReceiveBuffer.begin() +
                                static_cast<std::ptrdiff_t>(_totalSize));

                        std::vector<uint8_t> _responsePayload;
                        bool _handled{
                            TryInvokeHandler(_message, _responsePayload)};
                        if (_handled)
                        {
                            mSendingQueue.TryEnqueue(std::move(_responsePayload));
                        }
                    }
                }

                void SocketRpcServer::onSend()
                {
                    // QNX io-sock: POLLIN does not fire reliably for accepted sockets
                    // even when data is in the recv buffer. POLLOUT fires correctly.
                    // Piggyback a non-blocking receive on every send opportunity so
                    // requests are never stuck waiting for a POLLIN that never arrives.
                    onReceive();

                    while (!mSendingQueue.Empty())
                    {
                        std::vector<uint8_t> _payload;
                        bool _dequeued{mSendingQueue.TryDequeue(_payload)};
                        if (_dequeued && !_payload.empty())
                        {
                            ::send(
                                mServer.Connection(),
                                _payload.data(),
                                _payload.size(),
                                MSG_NOSIGNAL);
                        }
                    }
                }

                SocketRpcServer::~SocketRpcServer()
                {
                    std::cerr << "[RpcServer] ~SocketRpcServer, conn="
                              << mServer.Connection() << "\n" << std::flush;
                    mPoller->TryRemoveSender(&mServer);
                    mPoller->TryRemoveReceiver(&mServer);
                    mPoller->TryRemoveListener(&mServer);
                }
            }
        }
    }
}
