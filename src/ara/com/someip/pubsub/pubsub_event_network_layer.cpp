#include <algorithm>
#include "./pubsub_event_network_layer.h"
#include <stdexcept>
#include <cstdint>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace pubsub
            {
                PubSubEventNetworkLayer::PubSubEventNetworkLayer(
                    AsyncBsdSocketLib::Poller *poller,
                    std::string nicIp,
                    std::string eventIp,
                    uint16_t eventPort,
                    std::vector<std::string> unicastPeers) : mEventIp{eventIp},
                                          mEventPort{eventPort},
                                          mUnicastPeers{std::move(unicastPeers)},
                                          mPoller{poller},
                                          mUdpSocket{
                                              eventIp,
                                              eventPort,
                                              std::move(nicIp),
                                              std::move(eventIp),
                                              true}
                {
                    bool _successful{mUdpSocket.TrySetup()};
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Event notification UDP socket setup failed.");
                    }

                    auto _sender{
                        std::bind(&PubSubEventNetworkLayer::onSend, this)};
                    _successful = mPoller->TryAddSender(&mUdpSocket, _sender);
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Adding event notification UDP sender failed.");
                    }
                }

                void PubSubEventNetworkLayer::onSend()
                {
                    while (!mSendingQueue.Empty())
                    {
                        std::vector<uint8_t> _payload;
                        bool _dequeued{mSendingQueue.TryDequeue(_payload)};
                        if (_dequeued)
                        {
                            std::array<uint8_t, cBufferSize> _buffer{};
                            std::copy_n(
                                _payload.cbegin(),
                                _payload.size(),
                                _buffer.begin());

                            if (mUnicastPeers.empty())
                            {
                                mUdpSocket.Send(_buffer, mEventIp, mEventPort);
                            }
                            else
                            {
                                for (const auto &_peer : mUnicastPeers)
                                {
                                    mUdpSocket.Send(_buffer, _peer, mEventPort);
                                }
                            }
                        }
                    }
                }

                void PubSubEventNetworkLayer::SendRaw(
                    const std::vector<uint8_t> &bytes)
                {
                    mSendingQueue.TryEnqueue(bytes);
                }

                PubSubEventNetworkLayer::~PubSubEventNetworkLayer()
                {
                    mPoller->TryRemoveSender(&mUdpSocket);
                }
            }
        }
    }
}
