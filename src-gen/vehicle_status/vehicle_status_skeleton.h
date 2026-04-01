#ifndef VEHICLE_STATUS_SKELETON_H
#define VEHICLE_STATUS_SKELETON_H

#include <cstdint>
#include <string>
#include "ara/com/service_skeleton.h"
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
                class SomeIpSdServer;
            }
            namespace pubsub
            {
                class SomeIpPubSubServer;
                class PubSubEventNetworkLayer;
            }
        }
    }
}

namespace application
{
    namespace vehicle_status
    {
        /// @brief Generated skeleton for the VehicleStatus SOME/IP service.
        ///
        /// Simulates what an AUTOSAR code generator would produce from
        /// extended_vehicle_service_interface.arxml (SWS_CM §8.4).
        /// Applications interact only with this class — SOME/IP internals are
        /// entirely hidden.
        class VehicleStatusSkeleton : public ara::com::ServiceSkeleton
        {
        public:
            /// @brief Construct and configure the skeleton from the manifest.
            /// @param poller  Global async I/O poller.
            /// @param manifestPath  Path to the provided service instance manifest.
            /// @throws std::runtime_error if the manifest cannot be parsed.
            VehicleStatusSkeleton(
                AsyncBsdSocketLib::Poller *poller,
                const std::string &manifestPath);

            ~VehicleStatusSkeleton() override;

            /// Start SOME/IP service discovery advertising.
            void OfferService() override;

            /// Stop advertising and release SD resources.
            void StopOfferService() override;

            /// Publish a VehicleStatus event to all subscribers.
            /// The skeleton injects mInstanceId as SenderInstanceId before
            /// serialising, so callers need not set that field.
            void events_VehicleStatus_Send(const VehicleStatusData &data);

        private:
            ara::com::someip::sd::SdNetworkLayer *mNetworkLayer;
            ara::com::someip::sd::SomeIpSdServer *mSdServer;
            ara::com::someip::pubsub::SomeIpPubSubServer *mPubSubServer;
            ara::com::someip::pubsub::PubSubEventNetworkLayer *mEventLayer;
            uint16_t mInstanceId{0};

            void configureFromManifest(const std::string &manifestPath);
        };
    }
}

#endif
