#include <gtest/gtest.h>
#include "../../../../src-gen/vehicle_status/vehicle_status_data.h"

using namespace application::vehicle_status;

TEST(VehicleStatusData, SerializeRoundTrip)
{
    VehicleStatusData _original;
    _original.Vin = "YV1AB8154413EB123"; // exactly 17 chars
    _original.ConnectedToCloud = true;
    _original.SenderInstanceId = 0x42;

    auto _payload{_original.Serialize()};
    ASSERT_EQ(_payload.size(), VehicleStatusData::cPayloadSize);

    auto _decoded{VehicleStatusData::Deserialize(_payload)};
    EXPECT_EQ(_decoded.Vin, _original.Vin);
    EXPECT_EQ(_decoded.ConnectedToCloud, _original.ConnectedToCloud);
    EXPECT_EQ(_decoded.SenderInstanceId, _original.SenderInstanceId);
}

TEST(VehicleStatusData, ShortVinZeroPadded)
{
    VehicleStatusData _original;
    _original.Vin = "ABC"; // 3 chars
    _original.ConnectedToCloud = false;
    _original.SenderInstanceId = 1;

    auto _payload{_original.Serialize()};
    ASSERT_EQ(_payload.size(), VehicleStatusData::cPayloadSize);

    // bytes 3..16 must be zero-padded
    for (std::size_t i = 3; i < VehicleStatusData::cVinLength; ++i)
        EXPECT_EQ(_payload[i], 0) << "byte " << i << " should be 0";

    auto _decoded{VehicleStatusData::Deserialize(_payload)};
    EXPECT_EQ(_decoded.Vin, "ABC");
    EXPECT_EQ(_decoded.ConnectedToCloud, false);
    EXPECT_EQ(_decoded.SenderInstanceId, 1);
}

TEST(VehicleStatusData, AllZeroPayload)
{
    std::vector<uint8_t> _full(VehicleStatusData::cPayloadSize, 0);
    auto _data{VehicleStatusData::Deserialize(_full)};
    EXPECT_TRUE(_data.Vin.empty());
    EXPECT_FALSE(_data.ConnectedToCloud);
    EXPECT_EQ(_data.SenderInstanceId, 0);
}

TEST(VehicleStatusData, ConnectedToCloudFalse)
{
    VehicleStatusData _original;
    _original.Vin = "VIN00000000000000";
    _original.ConnectedToCloud = false;
    _original.SenderInstanceId = 0;

    auto _payload{_original.Serialize()};
    EXPECT_EQ(_payload[VehicleStatusData::cVinLength], 0);

    auto _decoded{VehicleStatusData::Deserialize(_payload)};
    EXPECT_FALSE(_decoded.ConnectedToCloud);
}
