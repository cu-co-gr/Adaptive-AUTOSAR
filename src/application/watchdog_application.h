#ifndef WATCHDOG_APPLICATION_H
#define WATCHDOG_APPLICATION_H

#include <chrono>
#include <cstdint>
#include <vector>
#include "../ara/exec/helper/modelled_process.h"
#include "../ara/com/someip/sd/sd_network_layer.h"
#include "../ara/com/someip/pubsub/someip_pubsub_client.h"
#include "../ara/com/someip/pubsub/pubsub_event_receiver.h"
#include "../arxml/arxml_reader.h"

namespace application
{
    /// @brief Watchdog application: subscribes to VehicleStatus events and
    ///        logs a warning when no event is received within the timeout window.
    class WatchdogApplication : public ara::exec::helper::ModelledProcess
    {
    private:
        static const std::string cAppId;
        static const std::string cNicIp;
        static const std::chrono::milliseconds cEventTimeout;

        ara::com::someip::sd::SdNetworkLayer *mSdNetworkLayer{nullptr};
        ara::com::someip::pubsub::SomeIpPubSubClient *mSdClient{nullptr};
        ara::com::someip::pubsub::PubSubEventReceiver *mEventReceiver{nullptr};

        uint16_t mSelfInstanceId{0};
        uint16_t mPeerInstanceId{0};
        std::chrono::steady_clock::time_point mLastEventTime;
        bool mExpiredLogged{false};

        void configureFromManifest(const arxml::ArxmlReader &reader);
        void onEventReceived(const std::vector<uint8_t> &payload);

    protected:
        int Main(
            const std::atomic_bool *cancellationToken,
            const std::map<std::string, std::string> &arguments) override;

    public:
        /// @brief Constructor
        /// @param poller Global poller for network communication
        explicit WatchdogApplication(AsyncBsdSocketLib::Poller *poller);

        ~WatchdogApplication() override;
    };
}

#endif
