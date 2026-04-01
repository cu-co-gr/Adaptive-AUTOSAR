#ifndef VEHICLE_STATUS_PROXY_H
#define VEHICLE_STATUS_PROXY_H

#include <cstdint>
#include <functional>
#include <string>
#include "ara/com/service_proxy.h"
#include "./vehicle_status_data.h"
#include <stdexcept>

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                class SdNetworkLayer;
                class SomeIpPubSubClient;
            }
            namespace pubsub
            {
                class SomeIpPubSubClient;
                class PubSubEventReceiver;
            }
        }
    }
}

namespace application
{
    namespace vehicle_status
    {
        /// @brief Generated proxy for the VehicleStatus SOME/IP service.
        ///
        /// Simulates what an AUTOSAR code generator would produce from
        /// extended_vehicle_service_interface.arxml (SWS_CM §8.4).
        /// Applications interact only with this class — SOME/IP internals are
        /// entirely hidden.
        class VehicleStatusProxy : public ara::com::ServiceProxy
        {
        public:
            /// @brief Construct, subscribe, and start listening for events.
            /// @param poller  Global async I/O poller.
            /// @param manifestPath  Path to the required service instance manifest.
            /// @throws std::runtime_error if the manifest cannot be parsed.
            VehicleStatusProxy(
                AsyncBsdSocketLib::Poller *poller,
                const std::string &manifestPath);

            ~VehicleStatusProxy() override;

            /// Register a typed receive handler for VehicleStatus events.
            /// Only events from the peer declared in the manifest are forwarded.
            void events_VehicleStatus_SetReceiveHandler(
                std::function<void(const VehicleStatusData &)> handler);

            /// Remove the registered receive handler.
            void events_VehicleStatus_UnsetReceiveHandler();

            /// Instance ID of this subscriber (WATCHDOG-INSTANCE-ID).
            uint16_t GetSelfInstanceId() const noexcept;

            /// Instance ID of the monitored peer (SERVICE-INSTANCE-ID).
            uint16_t GetPeerInstanceId() const noexcept;

        private:
            ara::com::someip::sd::SdNetworkLayer *mSdNetworkLayer;
            ara::com::someip::pubsub::SomeIpPubSubClient *mSdClient;
            ara::com::someip::pubsub::PubSubEventReceiver *mEventReceiver;
            uint16_t mSelfInstanceId{0};
            uint16_t mPeerInstanceId{0};
            std::function<void(const VehicleStatusData &)> mReceiveHandler;

            void configureFromManifest(const std::string &manifestPath);
        };
    }
}

#endif
