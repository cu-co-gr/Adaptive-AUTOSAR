#ifndef SOMEIP_PUBSUB_SERVER
#define SOMEIP_PUBSUB_SERVER

#include "../../helper/finite_state_machine.h"
#include "../../helper/network_layer.h"
#include "../sd/someip_sd_message.h"
#include "../../entry/eventgroup_entry.h"
#include "../../option/ipv4_endpoint_option.h"
#include "./fsm/service_down_state.h"
#include "./fsm/notsubscribed_state.h"
#include "./fsm/subscribed_state.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace pubsub
            {
                class PubSubEventNetworkLayer;

                /// @brief SOME/IP publish/subscribe server
                class SomeIpPubSubServer
                {
                private:
                    helper::FiniteStateMachine<helper::PubSubState> mStateMachine;
                    helper::NetworkLayer<sd::SomeIpSdMessage> *mCommunicationLayer;
                    PubSubEventNetworkLayer *mEventLayer;
                    uint16_t mSessionId;
                    const uint16_t mServiceId;
                    const uint16_t mInstanceId;
                    const uint8_t mMajorVersion;
                    const uint16_t mEventgroupId;
                    const helper::Ipv4Address mEndpointIp;
                    const uint16_t mEndpointPort;
                    fsm::ServiceDownState mServiceDownState;
                    fsm::NotSubscribedState mNotSubscribedState;
                    fsm::SubscribedState mSubscribedState;

                    void onMessageReceived(sd::SomeIpSdMessage &&message);
                    void processEntry(const entry::EventgroupEntry *entry);

                public:
                    SomeIpPubSubServer() = delete;
                    ~SomeIpPubSubServer();

                    /// @brief Constructor
                    /// @param networkLayer Network communication abstraction layer
                    /// @param serviceId Service ID
                    /// @param instanceId Service instance ID
                    /// @param majorVersion Service major version
                    /// @param eventgroupId Service event-group ID
                    /// @param ipAddress Multicast IP address that clients should listen to for receiving events
                    /// @param port Multicast port number that clients should listen to for receiving events
                    SomeIpPubSubServer(
                        helper::NetworkLayer<sd::SomeIpSdMessage> *networkLayer,
                        uint16_t serviceId,
                        uint16_t instanceId,
                        uint8_t majorVersion,
                        uint16_t eventgroupId,
                        helper::Ipv4Address ipAddress,
                        uint16_t port);

                    /// @brief Set the event notification network layer
                    /// @param eventLayer UDP sender for event notifications (ownership not transferred)
                    void SetEventLayer(PubSubEventNetworkLayer *eventLayer) noexcept;

                    /// @brief Publish an event notification to all subscribers
                    /// @param eventId SOME/IP event ID (e.g., 0x8001 for the first event)
                    /// @param payload Serialized event data
                    /// @note No-op when not in Subscribed state or no event layer is set
                    void Publish(
                        uint16_t eventId,
                        const std::vector<uint8_t> &payload);

                    /// @brief Start the server
                    void Start();

                    /// @brief Get the current server state
                    /// @returns Server machine state
                    helper::PubSubState GetState() const noexcept;

                    /// @brief Stop the server
                    void Stop();
                };
            }
        }
    }
}

#endif