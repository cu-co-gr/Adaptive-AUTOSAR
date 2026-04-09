#include "./vehicle_status_proxy.h"

#include <functional>
#include <string>
#include <vector>
#include "arxml/arxml_reader.h"
#include "ara/com/someip/sd/sd_network_layer.h"
#include "ara/com/someip/pubsub/someip_pubsub_client.h"
#include "ara/com/someip/pubsub/pubsub_event_receiver.h"
#include <cstdint>

namespace application
{
    namespace vehicle_status
    {
        VehicleStatusProxy::VehicleStatusProxy(
            AsyncBsdSocketLib::Poller *poller,
            const std::string &manifestPath)
            : ara::com::ServiceProxy{poller},
              mSdNetworkLayer{nullptr},
              mSdClient{nullptr},
              mEventReceiver{nullptr},
              mSelfInstanceId{0},
              mPeerInstanceId{0}
        {
            configureFromManifest(manifestPath);
        }

        void VehicleStatusProxy::configureFromManifest(
            const std::string &manifestPath)
        {
            const arxml::ArxmlReader cReader(manifestPath);

            const arxml::ArxmlNode cServiceIdNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                     "SERVICE-INTERFACE-DEPLOYMENT",
                                     "SERVICE-INTERFACE-ID"})};

            const arxml::ArxmlNode cInstanceIdNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                     "SERVICE-INSTANCE-ID"})};

            const arxml::ArxmlNode cMajorVersionNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                     "SERVICE-INTERFACE-DEPLOYMENT",
                                     "SERVICE-INTERFACE-VERSION",
                                     "MAJOR-VERSION"})};

            const arxml::ArxmlNode cEventGroupIdNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                     "REQUIRED-EVENT-GROUPS",
                                     "SOMEIP-REQUIRED-EVENT-GROUP",
                                     "EVENT-GROUP-ID"})};

            const arxml::ArxmlNode cMulticastIpNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                     "REQUIRED-EVENT-GROUPS",
                                     "SOMEIP-REQUIRED-EVENT-GROUP",
                                     "IPV-4-MULTICAST-IP-ADDRESS"})};

            const arxml::ArxmlNode cMulticastPortNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                     "REQUIRED-EVENT-GROUPS",
                                     "SOMEIP-REQUIRED-EVENT-GROUP",
                                     "EVENT-MULTICAST-UDP-PORT"})};

            const arxml::ArxmlNode cSelfInstanceIdNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                     "WATCHDOG-INSTANCE-ID"})};

            const auto cServiceId{cServiceIdNode.GetValue<uint16_t>()};
            const auto cInstanceId{cInstanceIdNode.GetValue<uint16_t>()};
            const auto cMajorVersion{cMajorVersionNode.GetValue<uint8_t>()};
            const auto cEventGroupId{cEventGroupIdNode.GetValue<uint16_t>()};
            const auto cMulticastIp{cMulticastIpNode.GetValue<std::string>()};
            const auto cMulticastPort{cMulticastPortNode.GetValue<uint16_t>()};

            mSelfInstanceId = cSelfInstanceIdNode.GetValue<uint16_t>();
            mPeerInstanceId = cInstanceId;

            // Read NIC IP so multicast binds to the correct network interface.
            const arxml::ArxmlNode cNicIpNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "COMMUNICATION-CLUSTER",
                                     "ETHERNET-PHYSICAL-CHANNEL",
                                     "NETWORK-ENDPOINTS",
                                     "NETWORK-ENDPOINT",
                                     "NETWORK-ENDPOINT-ADDRESSES",
                                     "IPV-4-CONFIGURATION",
                                     "IPV-4-ADDRESS"})};
            const auto cNicIp{cNicIpNode.GetValue<std::string>()};

            // ── Optional unicast SD peers ─────────────────────────────────────
            // When present, SD Finds are sent directly to the peer (unicast)
            // instead of the multicast group, enabling operation across routers
            // that do not forward multicast traffic.
            std::vector<std::string> cUnicastPeers;
            const arxml::ArxmlNodeRange cPeerNodes{
                cReader.GetNodes({"AUTOSAR",
                                  "AR-PACKAGES",
                                  "AR-PACKAGE",
                                  "ELEMENTS",
                                  "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                  "SD-CLIENT-CONFIG",
                                  "UNICAST-PEERS",
                                  "UNICAST-PEER"})};
            for (const arxml::ArxmlNode &_node : cPeerNodes)
            {
                std::string _ip{_node.GetValue<std::string>()};
                if (!_ip.empty())
                    cUnicastPeers.push_back(std::move(_ip));
            }

            mSdNetworkLayer =
                new ara::com::someip::sd::SdNetworkLayer(
                    mPoller, cNicIp, cMulticastIp, cMulticastPort,
                    cUnicastPeers);

            mSdClient =
                new ara::com::someip::pubsub::SomeIpPubSubClient(
                    mSdNetworkLayer, 1);

            auto _callback{
                [this](const std::vector<uint8_t> &raw)
                {
                    if (raw.size() < VehicleStatusData::cPayloadSize)
                        return;
                    VehicleStatusData _data{
                        VehicleStatusData::Deserialize(raw)};
                    if (_data.SenderInstanceId !=
                        static_cast<uint8_t>(mPeerInstanceId))
                        return;
                    if (mReceiveHandler)
                        mReceiveHandler(_data);
                }};

            // In unicast mode, bind the event receiver to 0.0.0.0 so it
            // accepts both unicast packets (from cross-machine sender) and
            // multicast packets (from same-host sender). In multicast-only
            // mode, bind to the multicast group address as before.
            const std::string cBindIp{
                cUnicastPeers.empty() ? cMulticastIp : std::string{"0.0.0.0"}};

            mEventReceiver =
                new ara::com::someip::pubsub::PubSubEventReceiver(
                    mPoller, cNicIp, cMulticastIp, cMulticastPort,
                    std::move(_callback), cBindIp);

            mSdClient->Subscribe(
                cServiceId, cInstanceId, cMajorVersion, cEventGroupId);
        }

        void VehicleStatusProxy::events_VehicleStatus_SetReceiveHandler(
            std::function<void(const VehicleStatusData &)> handler)
        {
            mReceiveHandler = std::move(handler);
        }

        void VehicleStatusProxy::events_VehicleStatus_UnsetReceiveHandler()
        {
            mReceiveHandler = nullptr;
        }

        uint16_t VehicleStatusProxy::GetSelfInstanceId() const noexcept
        {
            return mSelfInstanceId;
        }

        uint16_t VehicleStatusProxy::GetPeerInstanceId() const noexcept
        {
            return mPeerInstanceId;
        }

        VehicleStatusProxy::~VehicleStatusProxy()
        {
            delete mSdClient;
            delete mEventReceiver;
            delete mSdNetworkLayer;
        }
    }
}
