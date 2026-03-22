#include "../ara/com/someip/sd/sd_network_layer.h"
#include "../ara/diag/conversation.h"
#include "../application/helper/argument_configuration.h"
#include "./extended_vehicle.h"

namespace application
{
    const std::string ExtendedVehicle::cAppId{"ExtendedVehicle"};
    const ara::core::InstanceSpecifier ExtendedVehicle::cSeInstance{"ExtendedVehicleSE"};

    ExtendedVehicle::ExtendedVehicle(
        AsyncBsdSocketLib::Poller *poller,
        ara::phm::CheckpointCommunicator *checkpointCommunicator) : ara::exec::helper::ModelledProcess(cAppId, poller),
                                                                    mSupervisedEntity{cSeInstance, checkpointCommunicator},
                                                                    mNetworkLayer{nullptr},
                                                                    mSdServer{nullptr},
                                                                    mPubSubServer{nullptr},
                                                                    mEventLayer{nullptr},
                                                                    mCurl{nullptr}
    {
    }

    void ExtendedVehicle::configureNetworkLayer(const arxml::ArxmlReader &reader)
    {
        const std::string cNicIpAddress{"127.0.0.1"};

        const arxml::ArxmlNode cSdPortNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "PROVIDED-EVENT-GROUPS",
                                "SOMEIP-PROVIDED-EVENT-GROUP",
                                "EVENT-MULTICAST-UDP-PORT"})};

        const arxml::ArxmlNode cSdIpNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "PROVIDED-EVENT-GROUPS",
                                "SOMEIP-PROVIDED-EVENT-GROUP",
                                "IPV-4-MULTICAST-IP-ADDRESS"})};

        const auto cSdPort{cSdPortNode.GetValue<uint16_t>()};
        const auto cSdIp{cSdIpNode.GetValue<std::string>()};
        mNetworkLayer =
            new ara::com::someip::sd::SdNetworkLayer(
                Poller, cNicIpAddress, cSdIp, cSdPort);
    }

    helper::NetworkConfiguration ExtendedVehicle::getNetworkConfiguration(
        const arxml::ArxmlReader &reader)
    {
        const std::string cNetworkEndpoint{"ExtendedVehicleEP"};
        const std::string cApplicationEndpoint{"ServerUnicastTcp"};
        const ara::com::option::Layer4ProtocolType cProtocol{
            ara::com::option::Layer4ProtocolType::Tcp};

        helper::NetworkConfiguration _result;
        bool _successful{
            helper::TryGetNetworkConfiguration(
                reader, cNetworkEndpoint, cApplicationEndpoint, cProtocol,
                _result)};

        if (_successful)
        {
            return _result;
        }
        else
        {
            throw std::runtime_error("Fetching network configuration failed.");
        }
    }

    void ExtendedVehicle::configureSdServer(const arxml::ArxmlReader &reader)
    {
        const arxml::ArxmlNode cServiceIdNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INTERFACE-DEPLOYMENT",
                                "SERVICE-INTERFACE-ID"})};

        const arxml::ArxmlNode cInstanceIdNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INSTANCE-ID"})};

        const arxml::ArxmlNode cMajorVersionNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INTERFACE-DEPLOYMENT",
                                "SERVICE-INTERFACE-VERSION",
                                "MAJOR-VERSION"})};

        const arxml::ArxmlNode cMinorVersionNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INTERFACE-DEPLOYMENT",
                                "SERVICE-INTERFACE-VERSION",
                                "MINOR-VERSION"})};

        const arxml::ArxmlNode cInitialDelayMinNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SD-SERVER-CONFIG",
                                "INITIAL-OFFER-BEHAVIOR",
                                "INITIAL-DELAY-MIN-VALUE"})};

        const arxml::ArxmlNode cInitialDelayMaxNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SD-SERVER-CONFIG",
                                "INITIAL-OFFER-BEHAVIOR",
                                "INITIAL-DELAY-MAX-VALUE"})};

        const auto cServiceId{cServiceIdNode.GetValue<uint16_t>()};
        const auto cInstanceId{cInstanceIdNode.GetValue<uint16_t>()};
        const auto cMajorVersion{cMajorVersionNode.GetValue<uint8_t>()};
        const auto cMinorVersion{cMinorVersionNode.GetValue<uint32_t>()};
        const auto cInitialDelayMin{cInitialDelayMinNode.GetValue<int>()};
        const auto cInitialDelayMax{cInitialDelayMaxNode.GetValue<int>()};

        helper::NetworkConfiguration _networkConfiguration{
            getNetworkConfiguration(reader)};

        mSdServer =
            new ara::com::someip::sd::SomeIpSdServer(
                mNetworkLayer,
                cServiceId,
                cInstanceId,
                cMajorVersion,
                cMinorVersion,
                _networkConfiguration.ipAddress,
                _networkConfiguration.portNumber,
                cInitialDelayMin,
                cInitialDelayMax);

        ara::log::LogStream _sdLogStream;
        _sdLogStream << "[SD-Server] Configured: service "
                     << static_cast<uint32_t>(cServiceId) << ":"
                     << static_cast<uint32_t>(cInstanceId)
                     << " v" << static_cast<uint32_t>(cMajorVersion) << "." << cMinorVersion
                     << " endpoint " << _networkConfiguration.ipAddress
                     << ":" << static_cast<uint32_t>(_networkConfiguration.portNumber);
        Log(cLogLevel, _sdLogStream);
    }

    void ExtendedVehicle::configurePubSubServer(const arxml::ArxmlReader &reader)
    {
        const std::string cNicIpAddress{"127.0.0.1"};

        const arxml::ArxmlNode cEventGroupIdNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "PROVIDED-EVENT-GROUPS",
                                "SOMEIP-PROVIDED-EVENT-GROUP",
                                "EVENT-GROUP-ID"})};

        const arxml::ArxmlNode cServiceIdNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INTERFACE-DEPLOYMENT",
                                "SERVICE-INTERFACE-ID"})};

        const arxml::ArxmlNode cInstanceIdNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INSTANCE-ID"})};

        const arxml::ArxmlNode cMajorVersionNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INTERFACE-DEPLOYMENT",
                                "SERVICE-INTERFACE-VERSION",
                                "MAJOR-VERSION"})};

        const arxml::ArxmlNode cMulticastIpNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "PROVIDED-EVENT-GROUPS",
                                "SOMEIP-PROVIDED-EVENT-GROUP",
                                "IPV-4-MULTICAST-IP-ADDRESS"})};

        const arxml::ArxmlNode cMulticastPortNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "PROVIDED-SOMEIP-SERVICE-INSTANCE",
                                "PROVIDED-EVENT-GROUPS",
                                "SOMEIP-PROVIDED-EVENT-GROUP",
                                "EVENT-MULTICAST-UDP-PORT"})};

        const auto cEventGroupId{cEventGroupIdNode.GetValue<uint16_t>()};
        const auto cServiceId{cServiceIdNode.GetValue<uint16_t>()};
        const auto cInstanceId{cInstanceIdNode.GetValue<uint16_t>()};
        mPublisherInstanceId = cInstanceId;
        const auto cMajorVersion{cMajorVersionNode.GetValue<uint8_t>()};
        const auto cMulticastIp{cMulticastIpNode.GetValue<std::string>()};
        const auto cMulticastPort{cMulticastPortNode.GetValue<uint16_t>()};

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
                Poller, cNicIpAddress, cMulticastIp, cMulticastPort);

        mPubSubServer->SetEventLayer(mEventLayer);
    }

    void ExtendedVehicle::publishVehicleStatus(
        const std::string &vin, bool connectedToCloud)
    {
        if (!mPubSubServer)
        {
            return;
        }

        // Serialize VehicleStatusType: VIN (17 bytes) + ConnectedToCloud (1 byte)
        const size_t cVinLength{17};
        std::vector<uint8_t> _payload;
        _payload.reserve(cVinLength + 1);

        for (size_t i = 0; i < cVinLength; ++i)
        {
            _payload.push_back(
                i < vin.size() ? static_cast<uint8_t>(vin[i]) : 0);
        }
        _payload.push_back(connectedToCloud ? 1 : 0);
        _payload.push_back(static_cast<uint8_t>(mPublisherInstanceId));

        // Event ID for TelematicControlModuleEvent per service interface ARXML
        const uint16_t cTelematicEventId{0x8001};
        mPubSubServer->Publish(cTelematicEventId, _payload);
    }

    bool ExtendedVehicle::tryConfigureRestCommunication(
        std::string apiKey, std::string bearerToken, std::string &vin)
    {
        mCurl = new helper::CurlWrapper(apiKey, bearerToken);

        ara::log::LogStream _logStream;
        vin = "DEMO0000000000001";
        mResourcesUrl = "demo://localhost/resources";
        _logStream << "Demo mode: VIN set to " << vin;
        Log(cLogLevel, _logStream);

        return true;
    }

    DoipLib::ControllerConfig ExtendedVehicle::getDoipConfiguration(
        const arxml::ArxmlReader &reader)
    {
        const arxml::ArxmlNode cAnnouncementTimeNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "DO-IP-INSTANTIATION",
                                "NETWORK-INTERFACES",
                                "DO-IP-NETWORK-CONFIGURATION",
                                "MAX-INITIAL-VEHICLE-ANNOUNCEMENT-TIME"})};

        const arxml::ArxmlNode cAnnouncementCountNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "DO-IP-INSTANTIATION",
                                "NETWORK-INTERFACES",
                                "DO-IP-NETWORK-CONFIGURATION",
                                "VEHICLE-ANNOUNCEMENT-COUNT"})};

        const arxml::ArxmlNode cAnnouncementIntervalNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "DO-IP-INSTANTIATION",
                                "NETWORK-INTERFACES",
                                "DO-IP-NETWORK-CONFIGURATION",
                                "VEHICLE-ANNOUNCEMENT-INTERVAL"})};

        DoipLib::ControllerConfig _result;

        const auto cAnnouncementTime{cAnnouncementTimeNode.GetValue<int>()};
        _result.doIPInitialVehicleAnnouncementTime =
            std::chrono::seconds(cAnnouncementTime);

        const auto cAnnouncementCount{cAnnouncementCountNode.GetValue<uint8_t>()};
        _result.doIPVehicleAnnouncementCount = cAnnouncementCount;

        const auto cAnnouncementInterval{cAnnouncementIntervalNode.GetValue<int>()};
        _result.doIPVehicleAnnouncementInterval =
            std::chrono::seconds(cAnnouncementInterval);

        const uint8_t cProtocolVersion{2};
        _result.protocolVersion = cProtocolVersion;

        _result.doipMaxRequestBytes = doip::DoipServer::cDoipPacketSize;

        return _result;
    }

    void ExtendedVehicle::configureDoipServer(
        const arxml::ArxmlReader &reader, std::string &&vin)
    {
        const arxml::ArxmlNode cLogicalAddressNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "DO-IP-INSTANTIATION",
                                "LOGICAL-ADDRESS"})};

        const arxml::ArxmlNode cEidNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "DO-IP-INSTANTIATION",
                                "EID"})};

        const arxml::ArxmlNode cGidNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "DO-IP-INSTANTIATION",
                                "GID"})};

        const auto cLogicalAddress{cLogicalAddressNode.GetValue<uint16_t>()};
        const auto cEid{cEidNode.GetValue<uint64_t>()};
        const auto cGid{cGidNode.GetValue<uint64_t>()};

        helper::NetworkConfiguration _networkConfiguration{
            getNetworkConfiguration(reader)};

        DoipLib::ControllerConfig _controllConfig{getDoipConfiguration(reader)};

        mDoipServer =
            new doip::DoipServer(
                Poller,
                mCurl,
                mResourcesUrl,
                _networkConfiguration.ipAddress,
                _networkConfiguration.portNumber,
                std::move(_controllConfig),
                std::move(vin),
                cLogicalAddress,
                cEid,
                cGid);
    }

    int ExtendedVehicle::Main(
        const std::atomic_bool *cancellationToken,
        const std::map<std::string, std::string> &arguments)
    {
        const std::string cEvConfigArgument{helper::ArgumentConfiguration::cEvConfigArgument};
        const std::string cEvConfigFilepath{arguments.at(cEvConfigArgument)};
        const arxml::ArxmlReader cReader(cEvConfigFilepath);

        ara::log::LogStream _logStream;

        try
        {
            bool _running{true};

            configureNetworkLayer(cReader);
            configureSdServer(cReader);
            configurePubSubServer(cReader);

            _logStream << "Extended Vehicle AA has been initialized.";
            Log(cLogLevel, _logStream);

            std::string _vin;
            bool cConfigured{tryConfigureRestCommunication(
                arguments.at(helper::ArgumentConfiguration::cApiKeyArgument),
                arguments.at(helper::ArgumentConfiguration::cBearerTokenArgument),
                _vin)};

            if (cConfigured)
            {
                configureDoipServer(cReader, std::move(_vin));
                mPubSubServer->Start();
                mSdServer->Start();

                ara::log::LogStream _startLogStream;
                _startLogStream << "[SD-Server] Service discovery started"
                                << " — advertising on multicast 239.0.0.1:5555";
                Log(cLogLevel, _startLogStream);
            }

            uint32_t _heartbeatCounter{0};

            while (!cancellationToken->load() && _running)
            {
                mSupervisedEntity.ReportCheckpoint(
                    PhmCheckpointType::AliveCheckpoint);
                mSupervisedEntity.ReportCheckpoint(
                    PhmCheckpointType::DeadlineSourceCheckpoint);

                _running = WaitForActivation();

                mSupervisedEntity.ReportCheckpoint(
                    PhmCheckpointType::DeadlineTargetCheckpoint);

                ++_heartbeatCounter;
                if (_heartbeatCounter % 100 == 0)
                {
                    ara::log::LogStream _hbStream;
                    _hbStream << "[Heartbeat] ExtendedVehicle alive"
                              << " | VIN="
                              << (_vin.empty() ? std::string{"DEMO"} : _vin)
                              << " | SD server "
                              << (cConfigured ? "active" : "inactive");
                    Log(cLogLevel, _hbStream);

                    publishVehicleStatus(_vin, cConfigured);
                }
            }

            _logStream.Flush();
            if (ara::diag::Conversation::GetCurrentActiveConversations().size() == 0)
            {
                _logStream << "There was no active diagnostic conversation at the termination.";
            }
            else
            {
                _logStream << "There were still some active diagnostic conversations at the termination.";
            }

            Log(cLogLevel, _logStream);

            if (mPubSubServer)
                mPubSubServer->Stop();
            delete mSdServer;
            mSdServer = nullptr;

            _logStream.Flush();
            _logStream << "Extended Vehicle AA has been terminated.";
            Log(cLogLevel, _logStream);

            return cSuccessfulExitCode;
        }
        catch (const std::runtime_error &ex)
        {
            _logStream.Flush();
            _logStream << ex.what();
            Log(cErrorLevel, _logStream);

            return cUnsuccessfulExitCode;
        }
    }

    ExtendedVehicle::~ExtendedVehicle()
    {
        if (mDoipServer)
            delete mDoipServer;

        if (mCurl)
            delete mCurl;

        if (mEventLayer)
            delete mEventLayer;

        if (mPubSubServer)
            delete mPubSubServer;

        if (mSdServer)
            delete mSdServer;

        if (mNetworkLayer)
            delete mNetworkLayer;
    }
}