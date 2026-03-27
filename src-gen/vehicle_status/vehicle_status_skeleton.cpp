#include "./vehicle_status_skeleton.h"

#include <stdexcept>
#include "arxml/arxml_reader.h"
#include "ara/com/someip/sd/sd_network_layer.h"
#include "ara/com/someip/sd/someip_sd_server.h"
#include "ara/com/someip/pubsub/someip_pubsub_server.h"
#include "ara/com/someip/pubsub/pubsub_event_network_layer.h"

namespace application
{
    namespace vehicle_status
    {
        namespace
        {
            const std::string cNicIpAddress{"127.0.0.1"};
            const uint16_t cTelematicEventId{0x8001};
        }

        VehicleStatusSkeleton::VehicleStatusSkeleton(
            AsyncBsdSocketLib::Poller *poller,
            const std::string &manifestPath)
            : ara::com::ServiceSkeleton{poller},
              mNetworkLayer{nullptr},
              mSdServer{nullptr},
              mPubSubServer{nullptr},
              mEventLayer{nullptr},
              mInstanceId{0}
        {
            configureFromManifest(manifestPath);
        }

        void VehicleStatusSkeleton::configureFromManifest(
            const std::string &manifestPath)
        {
            const arxml::ArxmlReader cReader(manifestPath);

            // ── Multicast group (SD + event publish) ────────────────────────
            const arxml::ArxmlNode cMulticastPortNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "PROVIDED-EVENT-GROUPS",
                                     "SOMEIP-PROVIDED-EVENT-GROUP",
                                     "EVENT-MULTICAST-UDP-PORT"})};

            const arxml::ArxmlNode cMulticastIpNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "PROVIDED-EVENT-GROUPS",
                                     "SOMEIP-PROVIDED-EVENT-GROUP",
                                     "IPV-4-MULTICAST-IP-ADDRESS"})};

            const auto cMulticastPort{cMulticastPortNode.GetValue<uint16_t>()};
            const auto cMulticastIp{cMulticastIpNode.GetValue<std::string>()};

            mNetworkLayer =
                new ara::com::someip::sd::SdNetworkLayer(
                    mPoller, cNicIpAddress, cMulticastIp, cMulticastPort);

            // ── Service / instance identifiers ───────────────────────────────
            const arxml::ArxmlNode cServiceIdNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "SERVICE-INTERFACE-DEPLOYMENT",
                                     "SERVICE-INTERFACE-ID"})};

            const arxml::ArxmlNode cInstanceIdNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "SERVICE-INSTANCE-ID"})};

            const arxml::ArxmlNode cMajorVersionNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "SERVICE-INTERFACE-DEPLOYMENT",
                                     "SERVICE-INTERFACE-VERSION",
                                     "MAJOR-VERSION"})};

            const arxml::ArxmlNode cMinorVersionNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "SERVICE-INTERFACE-DEPLOYMENT",
                                     "SERVICE-INTERFACE-VERSION",
                                     "MINOR-VERSION"})};

            const auto cServiceId{cServiceIdNode.GetValue<uint16_t>()};
            const auto cInstanceId{cInstanceIdNode.GetValue<uint16_t>()};
            mInstanceId = cInstanceId;
            const auto cMajorVersion{cMajorVersionNode.GetValue<uint8_t>()};
            const auto cMinorVersion{cMinorVersionNode.GetValue<uint32_t>()};

            // ── SD initial-offer timing ───────────────────────────────────────
            const arxml::ArxmlNode cInitialDelayMinNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "SD-SERVER-CONFIG",
                                     "INITIAL-OFFER-BEHAVIOR",
                                     "INITIAL-DELAY-MIN-VALUE"})};

            const arxml::ArxmlNode cInitialDelayMaxNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "SD-SERVER-CONFIG",
                                     "INITIAL-OFFER-BEHAVIOR",
                                     "INITIAL-DELAY-MAX-VALUE"})};

            const auto cInitialDelayMin{cInitialDelayMinNode.GetValue<int>()};
            const auto cInitialDelayMax{cInitialDelayMaxNode.GetValue<int>()};

            // ── Unicast TCP endpoint (SD OFFER announcement) ─────────────────
            const arxml::ArxmlNode cUnicastIpNode{
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

            const arxml::ArxmlNode cUnicastPortNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "ETHERNET-COMMUNICATION-CONNECTOR",
                                     "AP-APPLICATION-ENDPOINTS",
                                     "AP-APPLICATION-ENDPOINT",
                                     "TP-CONFIGURATION",
                                     "TCP-TP",
                                     "TCP-TP-PORT",
                                     "PORT-NUMBER"})};

            const auto cUnicastIp{cUnicastIpNode.GetValue<std::string>()};
            const auto cUnicastPort{cUnicastPortNode.GetValue<uint16_t>()};

            // ── SD server ────────────────────────────────────────────────────
            mSdServer =
                new ara::com::someip::sd::SomeIpSdServer(
                    mNetworkLayer,
                    cServiceId,
                    cInstanceId,
                    cMajorVersion,
                    cMinorVersion,
                    cUnicastIp,
                    cUnicastPort,
                    cInitialDelayMin,
                    cInitialDelayMax);

            // ── Pub/sub server ───────────────────────────────────────────────
            const arxml::ArxmlNode cEventGroupIdNode{
                cReader.GetRootNode({"AUTOSAR",
                                     "AR-PACKAGES",
                                     "AR-PACKAGE",
                                     "ELEMENTS",
                                     "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                     "PROVIDED-EVENT-GROUPS",
                                     "SOMEIP-PROVIDED-EVENT-GROUP",
                                     "EVENT-GROUP-ID"})};

            const auto cEventGroupId{cEventGroupIdNode.GetValue<uint16_t>()};

            mPubSubServer =
                new ara::com::someip::pubsub::SomeIpPubSubServer(
                    mNetworkLayer,
                    cServiceId,
                    cInstanceId,
                    cMajorVersion,
                    cEventGroupId,
                    cMulticastIp,
                    cMulticastPort);

            mEventLayer =
                new ara::com::someip::pubsub::PubSubEventNetworkLayer(
                    mPoller, cNicIpAddress, cMulticastIp, cMulticastPort);

            mPubSubServer->SetEventLayer(mEventLayer);
        }

        void VehicleStatusSkeleton::OfferService()
        {
            if (mPubSubServer)
                mPubSubServer->Start();
            if (mSdServer)
                mSdServer->Start();
        }

        void VehicleStatusSkeleton::StopOfferService()
        {
            if (mPubSubServer)
                mPubSubServer->Stop();
            delete mSdServer;
            mSdServer = nullptr;
        }

        void VehicleStatusSkeleton::events_VehicleStatus_Send(
            const VehicleStatusData &data)
        {
            if (!mPubSubServer)
                return;

            VehicleStatusData _injected{data};
            _injected.SenderInstanceId = static_cast<uint8_t>(mInstanceId);
            mPubSubServer->Publish(cTelematicEventId, _injected.Serialize());
        }

        VehicleStatusSkeleton::~VehicleStatusSkeleton()
        {
            delete mEventLayer;
            delete mPubSubServer;
            delete mSdServer;
            delete mNetworkLayer;
        }
    }
}
