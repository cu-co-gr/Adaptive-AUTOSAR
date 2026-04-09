#ifndef SD_NETWORK_LAYER_H
#define SD_NETWORK_LAYER_H

#include <asyncbsdsocket/poller.h>
#include <asyncbsdsocket/udp_client.h>
#include "../../helper/concurrent_queue.h"
#include "../../helper/network_layer.h"
#include "./someip_sd_message.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            /// @brief SOME/IP service discovery namespace
            /// @note The namespace is not part of the ARA standard.
            namespace sd
            {
                /// @brief SOME/IP service discovery multicast network layer
                class SdNetworkLayer : public helper::NetworkLayer<SomeIpSdMessage>
                {
                private:
                    static const size_t cBufferSize;
                    static const std::string cAnyIpAddress;

                    const std::string cNicIpAddress;
                    const std::string cMulticastGroup;
                    const uint16_t cPort;
                    const std::vector<std::string> mUnicastPeers;

                    helper::ConcurrentQueue<std::vector<uint8_t>> mSendingQueue;
                    AsyncBsdSocketLib::Poller *const mPoller;
                    AsyncBsdSocketLib::UdpClient mUdpSocket;

                    void onReceive();
                    void onSend();

                public:
                    /// @brief Constructor
                    /// @param poller BSD sockets poller
                    /// @param nicIpAddress Network interface controller IPv4 address
                    /// @param multicastGroup Multicast group IPv4 address
                    /// @param port Multicast UDP port number
                    /// @param unicastPeers Optional list of unicast peer IP addresses.
                    ///        When non-empty, SD messages are sent directly to each
                    ///        peer instead of the multicast group. Useful when the
                    ///        network does not forward multicast between hosts.
                    /// @throws std::runtime_error Throws when the UDP socket configuration failed
                    SdNetworkLayer(
                        AsyncBsdSocketLib::Poller *poller,
                        std::string nicIpAddress,
                        std::string multicastGroup,
                        uint16_t port,
                        std::vector<std::string> unicastPeers = {});

                    ~SdNetworkLayer() override;

                    void Send(const SomeIpSdMessage &message) override;
                };
            }
        }
    }
}

#endif