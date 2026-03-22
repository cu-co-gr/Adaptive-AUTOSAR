#include <stdexcept>
#include <vector>
#include "../ara/log/log_stream.h"
#include "../arxml/arxml_reader.h"
#include "./helper/argument_configuration.h"
#include "./watchdog_application.h"

namespace application
{
    const std::string WatchdogApplication::cAppId{"WatchdogApplication"};
    const std::string WatchdogApplication::cNicIp{"127.0.0.1"};
    const std::chrono::milliseconds WatchdogApplication::cEventTimeout{2000};

    WatchdogApplication::WatchdogApplication(
        AsyncBsdSocketLib::Poller *poller) : ModelledProcess(cAppId, poller)
    {
    }

    void WatchdogApplication::configureFromManifest(
        const arxml::ArxmlReader &reader)
    {
        const arxml::ArxmlNode cServiceIdNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INTERFACE-DEPLOYMENT",
                                "SERVICE-INTERFACE-ID"})};

        const arxml::ArxmlNode cInstanceIdNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INSTANCE-ID"})};

        const arxml::ArxmlNode cMajorVersionNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                "SERVICE-INTERFACE-DEPLOYMENT",
                                "SERVICE-INTERFACE-VERSION",
                                "MAJOR-VERSION"})};

        const arxml::ArxmlNode cEventGroupIdNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                "REQUIRED-EVENT-GROUPS",
                                "SOMEIP-REQUIRED-EVENT-GROUP",
                                "EVENT-GROUP-ID"})};

        const arxml::ArxmlNode cMulticastIpNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                "REQUIRED-EVENT-GROUPS",
                                "SOMEIP-REQUIRED-EVENT-GROUP",
                                "IPV-4-MULTICAST-IP-ADDRESS"})};

        const arxml::ArxmlNode cMulticastPortNode{
            reader.GetRootNode({"AUTOSAR",
                                "AR-PACKAGES",
                                "AR-PACKAGE",
                                "ELEMENTS",
                                "REQUIRED-SOMEIP-SERVICE-INSTANCE",
                                "REQUIRED-EVENT-GROUPS",
                                "SOMEIP-REQUIRED-EVENT-GROUP",
                                "EVENT-MULTICAST-UDP-PORT"})};

        const arxml::ArxmlNode cSelfInstanceIdNode{
            reader.GetRootNode({"AUTOSAR",
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
                Poller, cNicIp, cMulticastIp, cMulticastPort);

        mSdClient =
            new ara::com::someip::pubsub::SomeIpPubSubClient(
                mSdNetworkLayer, 1);

        auto _callback{
            std::bind(&WatchdogApplication::onEventReceived, this,
                      std::placeholders::_1)};
        mEventReceiver =
            new ara::com::someip::pubsub::PubSubEventReceiver(
                Poller, cNicIp, cMulticastIp, cMulticastPort,
                std::move(_callback));

        mSdClient->Subscribe(cServiceId, cInstanceId, cMajorVersion, cEventGroupId);
    }

    void WatchdogApplication::onEventReceived(
        const std::vector<uint8_t> &payload)
    {
        if (payload.empty())
        {
            return;
        }

        uint16_t _senderId{payload.back()};

        // Only accept events from the monitored peer; ignore own loopback events.
        if (_senderId != mPeerInstanceId)
        {
            return;
        }

        mLastEventTime = std::chrono::steady_clock::now();
        mExpiredLogged = false;

        ara::log::LogStream _stream;
        _stream << "[Watchdog] "
                << static_cast<uint32_t>(mSelfInstanceId)
                << " restarted by ExtendedVehicle "
                << static_cast<uint32_t>(_senderId);
        Log(cLogLevel, _stream);
    }

    int WatchdogApplication::Main(
        const std::atomic_bool *cancellationToken,
        const std::map<std::string, std::string> &arguments)
    {
        const std::string &_manifestPath{
            arguments.at(
                helper::ArgumentConfiguration::cWatchdogConfigArgument)};

        arxml::ArxmlReader _reader(_manifestPath);
        configureFromManifest(_reader);

        mLastEventTime = std::chrono::steady_clock::now();

        ara::log::LogStream _startStream;
        _startStream << "[Watchdog] Started - timeout "
                     << static_cast<uint32_t>(cEventTimeout.count()) << " ms";
        Log(cLogLevel, _startStream);

        bool _running{true};
        while (_running)
        {
            _running = WaitForActivation();

            auto _now{std::chrono::steady_clock::now()};
            auto _elapsed{
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    _now - mLastEventTime)};

            if (_elapsed > cEventTimeout && !mExpiredLogged)
            {
                ara::log::LogStream _warnStream;
                _warnStream << "[Watchdog] Event expired: no VehicleStatus"
                            << " received for >"
                            << static_cast<uint32_t>(cEventTimeout.count())
                            << " ms";
                Log(cLogLevel, _warnStream);
                mExpiredLogged = true;
            }
        }

        delete mSdClient;
        mSdClient = nullptr;

        delete mEventReceiver;
        mEventReceiver = nullptr;

        delete mSdNetworkLayer;
        mSdNetworkLayer = nullptr;

        return cSuccessfulExitCode;
    }

    WatchdogApplication::~WatchdogApplication()
    {
        delete mSdClient;
        delete mEventReceiver;
        delete mSdNetworkLayer;
    }
}
