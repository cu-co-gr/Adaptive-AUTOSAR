#include <stdexcept>
#include <array>
#include <vector>
#include "./pubsub_event_receiver.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace pubsub
            {
                PubSubEventReceiver::PubSubEventReceiver(
                    AsyncBsdSocketLib::Poller *poller,
                    std::string nicIp,
                    std::string multicastIp,
                    uint16_t multicastPort,
                    std::function<void(const std::vector<uint8_t> &)> eventCallback) : mPoller{poller},
                                                           mUdpSocket{
                                                               multicastIp,
                                                               multicastPort,
                                                               nicIp,
                                                               multicastIp,
                                                               true},
                                                           mEventCallback{
                                                               std::move(
                                                                   eventCallback)}
                {
                    bool _successful{mUdpSocket.TrySetup()};
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Event receiver UDP socket setup failed.");
                    }

                    auto _receiver{
                        std::bind(&PubSubEventReceiver::onReceive, this)};
                    _successful = mPoller->TryAddReceiver(&mUdpSocket, _receiver);
                    if (!_successful)
                    {
                        throw std::runtime_error(
                            "Adding event receiver to poller failed.");
                    }
                }

                void PubSubEventReceiver::onReceive()
                {
                    std::array<uint8_t, cBufferSize> _buffer{};
                    std::string _ip;
                    uint16_t _port;

                    ssize_t _size{mUdpSocket.Receive(_buffer, _ip, _port)};
                    const ssize_t cSomeIpHeaderSize{16};
                    if (_size > cSomeIpHeaderSize)
                    {
                        // Drop SOME/IP-SD messages (Service ID = 0xFFFF).
                        // Both SD and event notifications share 239.0.0.1:5555.
                        const uint16_t cSdServiceId{0xFFFF};
                        uint16_t _serviceId{
                            static_cast<uint16_t>(
                                static_cast<uint16_t>(_buffer[0]) << 8 |
                                static_cast<uint16_t>(_buffer[1]))};
                        if (_serviceId == cSdServiceId)
                        {
                            return;
                        }

                        std::vector<uint8_t> _payload(
                            _buffer.begin() + cSomeIpHeaderSize,
                            _buffer.begin() + _size);
                        mEventCallback(_payload);
                    }
                }

                PubSubEventReceiver::~PubSubEventReceiver()
                {
                    mPoller->TryRemoveReceiver(&mUdpSocket);
                }
            }
        }
    }
}
