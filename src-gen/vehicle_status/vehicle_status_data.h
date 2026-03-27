#ifndef VEHICLE_STATUS_DATA_H
#define VEHICLE_STATUS_DATA_H

#include <cstdint>
#include <string>
#include <vector>

namespace application
{
    namespace vehicle_status
    {
        /// Wire-format payload for the VehicleStatus SOME/IP pub/sub event.
        ///
        /// Layout (19 bytes total):
        ///   [0..16] VIN — 17 bytes, zero-padded if shorter than 17 chars
        ///   [17]    connectedToCloud — 0 or 1
        ///   [18]    senderInstanceId — uint8_t cast of the publisher instance id
        struct VehicleStatusData
        {
            static constexpr std::size_t cVinLength{17};
            static constexpr std::size_t cPayloadSize{19};

            std::string Vin;
            bool ConnectedToCloud{false};
            uint8_t SenderInstanceId{0};

            /// Serialize to a cPayloadSize-byte wire payload.
            std::vector<uint8_t> Serialize() const;

            /// Deserialize from a wire payload.
            /// Precondition: payload.size() >= cPayloadSize.
            static VehicleStatusData Deserialize(
                const std::vector<uint8_t> &payload);
        };
    }
}

#endif
