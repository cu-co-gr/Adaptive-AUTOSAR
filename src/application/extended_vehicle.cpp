#include "../ara/diag/conversation.h"
#include "../application/helper/argument_configuration.h"
#include "../../src-gen/vehicle_status/vehicle_status_skeleton.h"
#include "../../src-gen/vehicle_status/vehicle_status_data.h"
#include "./extended_vehicle.h"
#include <stdexcept>
#include <cstdint>
#include <sys/stat.h>

namespace application
{
    const std::string ExtendedVehicle::cAppId{"ExtendedVehicle"};
    const ara::core::InstanceSpecifier ExtendedVehicle::cSeInstance{"ExtendedVehicleSE"};

    ExtendedVehicle::ExtendedVehicle(
        AsyncBsdSocketLib::Poller *poller,
        ara::phm::CheckpointCommunicator *checkpointCommunicator)
        : ara::exec::helper::ModelledProcess(cAppId, poller),
          mSupervisedEntity{cSeInstance, checkpointCommunicator},
          mSkeleton{nullptr},
          mCurl{nullptr},
          mDoipServer{nullptr},
          mStorage{nullptr},
          mLogCounter{0}
    {
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

        // Open (or create) the persistency storage for this execution session.
        // Try /var/per first; fall back to /tmp/per if not writable.
        {
            const std::string cPreferred{"/var/per"};
            const std::string cFallback{"/tmp/per"};
            if (::mkdir(cPreferred.c_str(), 0755) == 0 || errno == EEXIST)
                mStoragePath = cPreferred + "/extended_vehicle.json";
            else
            {
                ::mkdir(cFallback.c_str(), 0755);
                mStoragePath = cFallback + "/extended_vehicle.json";
            }
        }
        auto _openResult{ara::per::KeyValueStorage::Open(mStoragePath)};
        {
            ara::log::LogStream _perStream;
            if (_openResult.HasValue())
            {
                auto _kvs{std::move(_openResult).Value()};
                mStorage = std::unique_ptr<ara::per::KeyValueStorage>(
                    new ara::per::KeyValueStorage(std::move(_kvs)));
                _perStream << "[Per] Storage opened at " << mStoragePath;
            }
            else
            {
                _perStream << "[Per] Storage unavailable at " << mStoragePath;
            }
            Log(cLogLevel, _perStream);
        }

        // Helper: persist a log message and advance the entry counter.
        auto persistLog = [this](const std::string &message)
        {
            if (mStorage)
            {
                mStorage->SetValue(std::to_string(mLogCounter), message);
                ++mLogCounter;
            }
        };

        try
        {
            bool _running{true};

            mSkeleton = new vehicle_status::VehicleStatusSkeleton(
                Poller, cEvConfigFilepath);

            _logStream << "Extended Vehicle AA has been initialized.";
            Log(cLogLevel, _logStream);
            persistLog("Extended Vehicle AA has been initialized.");

            std::string _vin;
            bool cConfigured{tryConfigureRestCommunication(
                arguments.at(helper::ArgumentConfiguration::cApiKeyArgument),
                arguments.at(helper::ArgumentConfiguration::cBearerTokenArgument),
                _vin)};

            if (cConfigured)
            {
                configureDoipServer(cReader, std::move(_vin));
                mSkeleton->OfferService();
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
                    std::string _hbMsg =
                        std::string{"[Heartbeat] ExtendedVehicle alive"} +
                        " | VIN=" +
                        (_vin.empty() ? std::string{"DEMO"} : _vin) +
                        " | SD server " +
                        (cConfigured ? "active" : "inactive");

                    ara::log::LogStream _hbStream;
                    _hbStream << _hbMsg;
                    Log(cLogLevel, _hbStream);
                    persistLog(_hbMsg);

                    vehicle_status::VehicleStatusData _data;
                    _data.Vin = _vin;
                    _data.ConnectedToCloud = cConfigured;
                    mSkeleton->events_VehicleStatus_Send(_data);
                }
            }

            _logStream.Flush();
            std::string _termConvMsg;
            if (ara::diag::Conversation::GetCurrentActiveConversations().size() == 0)
            {
                _termConvMsg =
                    "There was no active diagnostic conversation at the termination.";
            }
            else
            {
                _termConvMsg =
                    "There were still some active diagnostic conversations at the termination.";
            }
            _logStream << _termConvMsg;
            Log(cLogLevel, _logStream);
            persistLog(_termConvMsg);

            if (mSkeleton)
                mSkeleton->StopOfferService();

            _logStream.Flush();
            _logStream << "Extended Vehicle AA has been terminated.";
            Log(cLogLevel, _logStream);
            persistLog("Extended Vehicle AA has been terminated.");

            if (mStorage)
            {
                mStorage->SyncToStorage();
                ara::log::LogStream _perStream;
                _perStream << "[Per] Synced " << mLogCounter
                           << " entries to " << mStoragePath;
                Log(cLogLevel, _perStream);
            }

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

        if (mSkeleton)
            delete mSkeleton;
    }
}
