#include "./vehicle_status_proxy.h"

#include <functional>
#include "arxml/arxml_reader.h"
#include "ara/com/someip/sd/sd_network_layer.h"
#include "ara/com/someip/pubsub/someip_pubsub_client.h"
#include "ara/com/someip/pubsub/pubsub_event_receiver.h"

namespace application
{
    namespace vehicle_status
    {
        namespace
        {
            const std::string cNicIpAddress{"127.0.0.1"};
        }

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

            mSdNetworkLayer =
                new ara::com::someip::sd::SdNetworkLayer(
                    mPoller, cNicIpAddress, cMulticastIp, cMulticastPort);

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

            mEventReceiver =
                new ara::com::someip::pubsub::PubSubEventReceiver(
                    mPoller, cNicIpAddress, cMulticastIp, cMulticastPort,
                    std::move(_callback));

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
