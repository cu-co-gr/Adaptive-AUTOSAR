#ifndef PUBSUB_EVENT_RECEIVER_H
#define PUBSUB_EVENT_RECEIVER_H

#include <functional>
#include <vector>
#include <cstdint>
#include <asyncbsdsocket/poller.h>
#include <asyncbsdsocket/udp_client.h>
#include <stdexcept>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace pubsub
            {
                /// @brief UDP multicast receiver for SOME/IP event notification messages
                /// @note Vendor implementation: joins the event multicast group and
                ///       fires a callback whenever any bytes arrive on that channel.
                class PubSubEventReceiver
                {
                private:
                    static const size_t cBufferSize{256};

                    AsyncBsdSocketLib::Poller *const mPoller;
                    AsyncBsdSocketLib::UdpClient mUdpSocket;
                    std::function<void(const std::vector<uint8_t> &)> mEventCallback;

                    void onReceive();

                public:
                    PubSubEventReceiver() = delete;

                    /// @brief Constructor
                    /// @param poller Global async poller
                    /// @param nicIp Network interface IP address (e.g. "127.0.0.1")
                    /// @param multicastIp Multicast group IP (e.g. "239.0.0.1")
                    /// @param multicastPort Multicast UDP port
                    /// @param eventCallback Callback invoked on every received packet
                    /// @throws std::runtime_error if socket setup or poller registration fails
                    PubSubEventReceiver(
                        AsyncBsdSocketLib::Poller *poller,
                        std::string nicIp,
                        std::string multicastIp,
                        uint16_t multicastPort,
                        std::function<void(const std::vector<uint8_t> &)> eventCallback);

                    ~PubSubEventReceiver();
                };
            }
        }
    }
}

#endif
