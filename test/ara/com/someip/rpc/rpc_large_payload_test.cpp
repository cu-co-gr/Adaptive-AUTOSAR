#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "../../../../../src/ara/com/someip/rpc/rpc_server.h"
#include "../../../../../src/ara/com/someip/rpc/someip_rpc_message.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace rpc
            {
                // Minimal RpcServer subclass exposing TryInvokeHandler for tests.
                class LargePayloadRpcServer : public RpcServer
                {
                public:
                    static const uint8_t cProtoVersion{1};
                    static const uint8_t cIfaceVersion{1};
                    static const uint16_t cServiceId{0x0100};
                    static const uint16_t cMethodId{0x0001};

                    std::vector<uint8_t> lastHandledPayload;

                    LargePayloadRpcServer()
                        : RpcServer(cProtoVersion, cIfaceVersion)
                    {
                        auto _handler = [this](
                            const std::vector<uint8_t> &rpcRequest,
                            std::vector<uint8_t> &rpcResponse) -> bool
                        {
                            lastHandledPayload = rpcRequest;
                            return true;
                        };
                        SetHandler(cServiceId, cMethodId, _handler);
                    }

                    bool Dispatch(const std::vector<uint8_t> &frame,
                                  std::vector<uint8_t> &response)
                    {
                        return TryInvokeHandler(frame, response);
                    }
                };

                const uint8_t LargePayloadRpcServer::cProtoVersion;
                const uint8_t LargePayloadRpcServer::cIfaceVersion;
                const uint16_t LargePayloadRpcServer::cServiceId;
                const uint16_t LargePayloadRpcServer::cMethodId;

                // ── Tests ─────────────────────────────────────────────────────────

                TEST(RpcLargePayloadTest, SmallPayloadRoundTrip)
                {
                    LargePayloadRpcServer _server;
                    const uint32_t _messageId{
                        (static_cast<uint32_t>(
                             LargePayloadRpcServer::cServiceId) << 16) |
                        LargePayloadRpcServer::cMethodId};

                    const std::vector<uint8_t> _rpcData{0x01, 0x02, 0x03};
                    SomeIpRpcMessage _req(
                        _messageId, 0x0001, 0x0001,
                        LargePayloadRpcServer::cProtoVersion,
                        LargePayloadRpcServer::cIfaceVersion,
                        _rpcData);

                    std::vector<uint8_t> _frame{_req.Payload()};
                    std::vector<uint8_t> _response;
                    bool _handled{_server.Dispatch(_frame, _response)};

                    ASSERT_TRUE(_handled);
                    EXPECT_EQ(_server.lastHandledPayload, _rpcData);
                }

                TEST(RpcLargePayloadTest, LargePayloadSerializeDeserialize)
                {
                    // Build a 65000-byte RPC payload
                    const size_t cPayloadSize{65000};
                    std::vector<uint8_t> _rpcData(cPayloadSize, 0xAB);

                    const uint32_t _messageId{
                        (static_cast<uint32_t>(
                             LargePayloadRpcServer::cServiceId) << 16) |
                        LargePayloadRpcServer::cMethodId};

                    SomeIpRpcMessage _req(
                        _messageId, 0x0001, 0x0001,
                        LargePayloadRpcServer::cProtoVersion,
                        LargePayloadRpcServer::cIfaceVersion,
                        _rpcData);

                    std::vector<uint8_t> _frame{_req.Payload()};

                    // Frame size = 16 header + 65000 payload
                    EXPECT_EQ(_frame.size(), 16u + cPayloadSize);

                    // Deserialize and check payload is preserved
                    SomeIpRpcMessage _deserialized{
                        SomeIpRpcMessage::Deserialize(_frame)};
                    EXPECT_EQ(_deserialized.RpcPayload(), _rpcData);
                    EXPECT_EQ(_deserialized.RpcPayload().size(), cPayloadSize);
                }

                TEST(RpcLargePayloadTest, LargePayloadHandlerReceivesFullData)
                {
                    LargePayloadRpcServer _server;

                    const size_t cPayloadSize{65000};
                    std::vector<uint8_t> _rpcData(cPayloadSize);
                    for (size_t i = 0; i < cPayloadSize; ++i)
                    {
                        _rpcData[i] = static_cast<uint8_t>(i & 0xFF);
                    }

                    const uint32_t _messageId{
                        (static_cast<uint32_t>(
                             LargePayloadRpcServer::cServiceId) << 16) |
                        LargePayloadRpcServer::cMethodId};

                    SomeIpRpcMessage _req(
                        _messageId, 0x0001, 0x0001,
                        LargePayloadRpcServer::cProtoVersion,
                        LargePayloadRpcServer::cIfaceVersion,
                        _rpcData);

                    std::vector<uint8_t> _frame{_req.Payload()};
                    std::vector<uint8_t> _response;
                    bool _handled{_server.Dispatch(_frame, _response)};

                    ASSERT_TRUE(_handled);
                    ASSERT_EQ(
                        _server.lastHandledPayload.size(), cPayloadSize);
                    EXPECT_EQ(_server.lastHandledPayload, _rpcData);
                }

                TEST(RpcLargePayloadTest, LargePayloadLengthFieldCorrect)
                {
                    const size_t cPayloadSize{65000};
                    std::vector<uint8_t> _rpcData(cPayloadSize, 0xCD);

                    const uint32_t _messageId{
                        (static_cast<uint32_t>(
                             LargePayloadRpcServer::cServiceId) << 16) |
                        LargePayloadRpcServer::cMethodId};

                    SomeIpRpcMessage _req(
                        _messageId, 0x0001, 0x0001,
                        LargePayloadRpcServer::cProtoVersion,
                        LargePayloadRpcServer::cIfaceVersion,
                        _rpcData);

                    std::vector<uint8_t> _frame{_req.Payload()};

                    // Length field is at bytes [4..7] big-endian.
                    // Length = 8 (fixed header after Length) + payload size.
                    uint32_t _lengthFromFrame =
                        (static_cast<uint32_t>(_frame[4]) << 24) |
                        (static_cast<uint32_t>(_frame[5]) << 16) |
                        (static_cast<uint32_t>(_frame[6]) << 8) |
                        (static_cast<uint32_t>(_frame[7]));

                    uint32_t _expectedLength =
                        8u + static_cast<uint32_t>(cPayloadSize);
                    EXPECT_EQ(_lengthFromFrame, _expectedLength);

                    // Total frame = 8 + Length
                    EXPECT_EQ(_frame.size(), 8u + _expectedLength);
                }

            } // namespace rpc
        } // namespace someip
    } // namespace com
} // namespace ara
