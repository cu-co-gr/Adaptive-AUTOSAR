#include <algorithm>
#include <sys/socket.h>
#include "./socket_rpc_client.h"
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
                const size_t SocketRpcClient::cBufferSize;

                SocketRpcClient::SocketRpcClient(
                    AsyncBsdSocketLib::Poller *poller,
                    std::string ipAddress,
                    uint16_t port,
                    uint8_t protocolVersion,
                    uint8_t interfaceVersion) : RpcClient(protocolVersion, interfaceVersion),
                                                mPoller{poller},
                                                mClient{AsyncBsdSocketLib::TcpClient(ipAddress, port)}
                {
                    bool _successful{mClient.TrySetup()};
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "TCP client socket setup failed.");
                    }

                    _successful = mClient.TryConnect();
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "TCP client socket connection failed.");
                    }

                    auto _sender{std::bind(&SocketRpcClient::onSend, this)};
                    _successful = mPoller->TryAddSender(&mClient, _sender);
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Adding TCP client socket sender failed.");
                    }

                    auto _receiver{std::bind(&SocketRpcClient::onReceive, this)};
                    _successful = mPoller->TryAddReceiver(&mClient, _receiver);
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Adding TCP client socket receiver failed.");
                    }
                }

                void SocketRpcClient::onSend()
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
                                mClient.Connection(),
                                _payload.data(),
                                _payload.size(),
                                MSG_NOSIGNAL);
                        }
                    }
                }

                void SocketRpcClient::onReceive()
                {
                    std::array<uint8_t, cBufferSize> _buffer;
                    ssize_t _receivedSize{mClient.Receive(_buffer)};

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
                            helper::ExtractInteger(
                                mReceiveBuffer, _lengthOffset)};
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

                        InvokeHandler(_message);
                    }
                }

                void SocketRpcClient::Send(const std::vector<uint8_t> &payload)
                {
                    mSendingQueue.TryEnqueue(payload);
                }

                SocketRpcClient::~SocketRpcClient()
                {
                    mPoller->TryRemoveReceiver(&mClient);
                    mPoller->TryRemoveSender(&mClient);
                }
            }
        }
    }
}