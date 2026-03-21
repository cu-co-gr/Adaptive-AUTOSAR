#ifndef PUBSUB_EVENT_NETWORK_LAYER_H
#define PUBSUB_EVENT_NETWORK_LAYER_H

#include <asyncbsdsocket/poller.h>
#include <asyncbsdsocket/udp_client.h>
#include "../../helper/concurrent_queue.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace pubsub
            {
                /// @brief UDP sender for SOME/IP event notification messages
                /// @note Vendor implementation: binds to an ephemeral local port
                ///       and sends raw notification bytes to the event multicast
                ///       endpoint. Clients join that multicast group to receive.
                class PubSubEventNetworkLayer
                {
                private:
                    static const size_t cBufferSize{256};

                    const std::string mEventIp;
                    const uint16_t mEventPort;
                    helper::ConcurrentQueue<std::vector<uint8_t>> mSendingQueue;
                    AsyncBsdSocketLib::Poller *const mPoller;
                    AsyncBsdSocketLib::UdpClient mUdpSocket;

                    void onSend();

                public:
                    PubSubEventNetworkLayer() = delete;

                    /// @brief Constructor
                    /// @param poller Global async poller
                    /// @param eventIp Event multicast IP address
                    /// @param eventPort Event multicast UDP port
                    /// @throws std::runtime_error if socket setup or poller registration fails
                    PubSubEventNetworkLayer(
                        AsyncBsdSocketLib::Poller *poller,
                        std::string eventIp,
                        uint16_t eventPort);

                    ~PubSubEventNetworkLayer();

                    /// @brief Enqueue raw bytes for delivery to the event multicast endpoint
                    /// @param bytes Fully serialized SOME/IP notification message
                    void SendRaw(const std::vector<uint8_t> &bytes);
                };
            }
        }
    }
}

#endif
