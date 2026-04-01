#include "../../entry/eventgroup_entry.h"
#include "../../helper/payload_helper.h"
#include "./pubsub_event_network_layer.h"
#include "./someip_pubsub_server.h"
#include <cstdint>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace pubsub
            {
                SomeIpPubSubServer::SomeIpPubSubServer(
                    helper::NetworkLayer<sd::SomeIpSdMessage> *networkLayer,
                    uint16_t serviceId,
                    uint16_t instanceId,
                    uint8_t majorVersion,
                    uint16_t eventgroupId,
                    helper::Ipv4Address ipAddress,
                    uint16_t port) : mCommunicationLayer{networkLayer},
                                     mEventLayer{nullptr},
                                     mSessionId{1},
                                     mServiceId{serviceId},
                                     mInstanceId{instanceId},
                                     mMajorVersion{majorVersion},
                                     mEventgroupId{eventgroupId},
                                     mEndpointIp{ipAddress},
                                     mEndpointPort{port}
                {
                    mStateMachine.Initialize({&mServiceDownState,
                                              &mNotSubscribedState,
                                              &mSubscribedState},
                                             helper::PubSubState::ServiceDown);

                    auto _receiver =
                        std::bind(
                            &SomeIpPubSubServer::onMessageReceived,
                            this,
                            std::placeholders::_1);
                    mCommunicationLayer->SetReceiver(this, _receiver);
                }

                void SomeIpPubSubServer::onMessageReceived(sd::SomeIpSdMessage &&message)
                {
                    // Iterate over all the message entry to search for the first Event-group Subscribing entry
                    for (auto &_entry : message.Entries())
                    {
                        if (_entry->Type() == entry::EntryType::Subscribing)
                        {
                            if (auto _eventgroupEntry = dynamic_cast<entry::EventgroupEntry *>(_entry.get()))
                            {
                                // Compare service ID, instance ID, major version, and event-group ID
                                if ((_eventgroupEntry->ServiceId() == mServiceId) &&
                                    (_eventgroupEntry->InstanceId() == entry::Entry::cAnyInstanceId ||
                                     _eventgroupEntry->InstanceId() == mInstanceId) &&
                                    (_eventgroupEntry->MajorVersion() == entry::Entry::cAnyMajorVersion ||
                                     _eventgroupEntry->MajorVersion() == mMajorVersion) &&
                                    (_eventgroupEntry->EventgroupId() == mEventgroupId))
                                {
                                    if (_eventgroupEntry->TTL() > 0)
                                    {
                                        // Subscription
                                        processEntry(_eventgroupEntry);
                                    }
                                    else
                                    {
                                        // Unsubscription
                                        helper::PubSubState _state = GetState();
                                        if (_state == helper::PubSubState::Subscribed)
                                        {
                                            mSubscribedState.Unsubscribed();
                                        }
                                    }

                                    return;
                                }
                            }
                        }
                    }
                }

                void SomeIpPubSubServer::processEntry(const entry::EventgroupEntry *entry)
                {
                    const bool cDiscardableEndpoint{true};

                    sd::SomeIpSdMessage _acknowledgeMessage;

                    helper::PubSubState _state = GetState();
                    if (_state == helper::PubSubState::NotSubscribed)
                    {
                        mNotSubscribedState.Subscribed();
                    }
                    else if (_state == helper::PubSubState::Subscribed)
                    {
                        mSubscribedState.Subscribed();
                    }

                    // Acknowledge the subscription if the service is up
                    auto _acknowledgeEntry =
                        _state == helper::PubSubState::ServiceDown ? entry::EventgroupEntry::CreateNegativeAcknowledgeEntry(
                                                                         entry)
                                                                   : entry::EventgroupEntry::CreateAcknowledgeEntry(
                                                                         entry);

                    // If the service is not down, add a multicast endpoint option to the acknowledgement entry
                    if (_state != helper::PubSubState::ServiceDown)
                    {
                        auto _multicastEndpoint =
                            option::Ipv4EndpointOption::CreateMulticastEndpoint(
                                cDiscardableEndpoint, mEndpointIp, mEndpointPort);
                        _acknowledgeEntry->AddFirstOption(std::move(_multicastEndpoint));
                    }

                    _acknowledgeMessage.AddEntry(std::move(_acknowledgeEntry));
                    mCommunicationLayer->Send(_acknowledgeMessage);
                }

                void SomeIpPubSubServer::SetEventLayer(
                    PubSubEventNetworkLayer *eventLayer) noexcept
                {
                    mEventLayer = eventLayer;
                }

                void SomeIpPubSubServer::Publish(
                    uint16_t eventId,
                    const std::vector<uint8_t> &payload)
                {
                    if (mEventLayer == nullptr)
                    {
                        return;
                    }

                    if (GetState() != helper::PubSubState::Subscribed)
                    {
                        return;
                    }

                    const uint8_t cProtocolVersion{1};
                    // SomeIpMessageType::Notification = 0x02
                    const uint8_t cNotificationType{0x02};
                    // SomeIpReturnCode::eOK = 0x00
                    const uint8_t cReturnCode{0x00};

                    auto _messageId =
                        static_cast<uint32_t>(mServiceId) << 16 |
                        static_cast<uint32_t>(eventId);
                    // Length covers the 8-byte sub-header + payload
                    auto _length =
                        static_cast<uint32_t>(8 + payload.size());

                    std::vector<uint8_t> _notification;
                    helper::Inject(_notification, _messageId);
                    helper::Inject(_notification, _length);
                    // Client ID = 0 (server-initiated notification)
                    helper::Inject(_notification, static_cast<uint16_t>(0));
                    helper::Inject(_notification, mSessionId);

                    _notification.push_back(cProtocolVersion);
                    _notification.push_back(mMajorVersion);
                    _notification.push_back(cNotificationType);
                    _notification.push_back(cReturnCode);

                    _notification.insert(
                        _notification.end(),
                        payload.cbegin(),
                        payload.cend());

                    if (++mSessionId == 0)
                    {
                        mSessionId = 1;
                    }

                    mEventLayer->SendRaw(_notification);
                }

                void SomeIpPubSubServer::Start()
                {
                    helper::PubSubState _state = GetState();
                    if (_state == helper::PubSubState::ServiceDown)
                    {
                        mServiceDownState.Started();
                    }
                }

                helper::PubSubState SomeIpPubSubServer::GetState() const noexcept
                {
                    return mStateMachine.GetState();
                }

                void SomeIpPubSubServer::Stop()
                {
                    helper::PubSubState _state = GetState();
                    if (_state == helper::PubSubState::NotSubscribed)
                    {
                        mNotSubscribedState.Stopped();
                    }
                    else if (_state == helper::PubSubState::Subscribed)
                    {
                        mSubscribedState.Stopped();
                    }
                }

                SomeIpPubSubServer::~SomeIpPubSubServer()
                {
                    Stop();
                }
            }
        }
    }
}