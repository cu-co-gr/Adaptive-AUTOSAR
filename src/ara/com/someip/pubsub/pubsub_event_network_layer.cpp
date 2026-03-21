#include <algorithm>
#include "./pubsub_event_network_layer.h"

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
                    std::string eventIp,
                    uint16_t eventPort) : mEventIp{std::move(eventIp)},
                                          mEventPort{eventPort},
                                          mPoller{poller},
                                          mUdpSocket{"0.0.0.0", 0}
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

                            mUdpSocket.Send(_buffer, mEventIp, mEventPort);
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
