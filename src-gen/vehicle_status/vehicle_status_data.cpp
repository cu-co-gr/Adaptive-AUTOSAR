#include "./vehicle_status_data.h"

#include <cstring>

namespace application
{
    namespace vehicle_status
    {
        constexpr std::size_t VehicleStatusData::cVinLength;
        constexpr std::size_t VehicleStatusData::cPayloadSize;

        std::vector<uint8_t> VehicleStatusData::Serialize() const
        {
            std::vector<uint8_t> _payload(cPayloadSize, 0);

            std::size_t _copyLen{
                Vin.size() < cVinLength ? Vin.size() : cVinLength};
            std::memcpy(_payload.data(), Vin.data(), _copyLen);

            _payload[cVinLength] = ConnectedToCloud ? 1 : 0;
            _payload[cVinLength + 1] = SenderInstanceId;

            return _payload;
        }

        VehicleStatusData VehicleStatusData::Deserialize(
            const std::vector<uint8_t> &payload)
        {
            VehicleStatusData _data;

            std::size_t _vinEnd{cVinLength};
            for (std::size_t i = 0; i < cVinLength; ++i)
            {
                if (payload[i] == 0)
                {
                    _vinEnd = i;
                    break;
                }
            }
            _data.Vin =
                std::string(reinterpret_cast<const char *>(payload.data()),
                            _vinEnd);

            _data.ConnectedToCloud = (payload[cVinLength] != 0);
            _data.SenderInstanceId = payload[cVinLength + 1];

            return _data;
        }
    }
}
