#include <algorithm>
#include <sys/socket.h>
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
                    bool _successful{mServer.TryAccept()};

                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Accepting RPC client TCP connection failed.");
                    }

                    _successful = mServer.TryMakeConnectionNonblock();
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Making non-blocking TCP connection failed.");
                    }

                    auto _receiver{std::bind(&SocketRpcServer::onReceive, this)};
                    _successful = mPoller->TryAddReceiver(&mServer, _receiver);
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Adding TCP server socket receiver failed.");
                    }

                    auto _sender{std::bind(&SocketRpcServer::onSend, this)};
                    _successful = mPoller->TryAddSender(&mServer, _sender);
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Adding TCP server socket sender failed.");
                    }
                }

                void SocketRpcServer::onReceive()
                {
                    std::array<uint8_t, cBufferSize> _buffer;
                    ssize_t _receivedSize{mServer.Receive(_buffer)};
                    if (_receivedSize <= 0)
                    {
                        return;
                    }

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
                    while (!mSendingQueue.Empty())
                    {
                        std::vector<uint8_t> _payload;
                        bool _dequeued{mSendingQueue.TryDequeue(_payload)};
                        if (_dequeued && !_payload.empty())
                        {
                            // Send entire payload in one call using the
                            // connection fd directly (avoids fixed-array size
                            // limit of the template Send() method).
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
                    mPoller->TryRemoveSender(&mServer);
                    mPoller->TryRemoveReceiver(&mServer);
                    mPoller->TryRemoveListener(&mServer);
                }
            }
        }
    }
}