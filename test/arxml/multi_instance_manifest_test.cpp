#include <gtest/gtest.h>
#include <string>
#include "../../src/arxml/arxml_reader.h"

// Tests verifying that machine_a and machine_b manifests produce distinct,
// non-conflicting configuration values.  These tests use inline XML matching
// the structure of the real ARXML files so they do not depend on file paths.

namespace arxml
{
    // -----------------------------------------------------------------------
    // Execution manifest: RPC server port
    // -----------------------------------------------------------------------

    static std::string makeExecutionManifest(uint16_t rpcPort)
    {
        return std::string(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\">"
            "<AR-PACKAGES><AR-PACKAGE>"
            "<SHORT-NAME>ExecutionManifest</SHORT-NAME>"
            "<ELEMENTS>"
            "<COMMUNICATION-CLUSTER>"
            "<ETHERNET-PHYSICAL-CHANNEL>"
            "<SHORT-NAME>Localhost</SHORT-NAME>"
            "<NETWORK-ENDPOINTS>"
            "<NETWORK-ENDPOINT>"
            "<SHORT-NAME>RpcServerEP</SHORT-NAME>"
            "<NETWORK-ENDPOINT-ADDRESSES>"
            "<IPV-4-CONFIGURATION><IPV-4-ADDRESS>127.0.0.1</IPV-4-ADDRESS></IPV-4-CONFIGURATION>"
            "</NETWORK-ENDPOINT-ADDRESSES>"
            "</NETWORK-ENDPOINT>"
            "</NETWORK-ENDPOINTS>"
            "</ETHERNET-PHYSICAL-CHANNEL>"
            "<PROTOCOL-VERSION>1</PROTOCOL-VERSION>"
            "</COMMUNICATION-CLUSTER>"
            "<ETHERNET-COMMUNICATION-CONNECTOR>"
            "<SHORT-NAME>RpcServerConn</SHORT-NAME>"
            "<AP-APPLICATION-ENDPOINTS>"
            "<AP-APPLICATION-ENDPOINT>"
            "<SHORT-NAME>ServerUnicastTcp</SHORT-NAME>"
            "<TP-CONFIGURATION><TCP-TP><TCP-TP-PORT>"
            "<PORT-NUMBER>") +
            std::to_string(rpcPort) +
            std::string("</PORT-NUMBER>"
            "</TCP-TP-PORT></TCP-TP></TP-CONFIGURATION>"
            "</AP-APPLICATION-ENDPOINT>"
            "</AP-APPLICATION-ENDPOINTS>"
            "</ETHERNET-COMMUNICATION-CONNECTOR>"
            "</ELEMENTS>"
            "</AR-PACKAGE></AR-PACKAGES>"
            "</AUTOSAR>");
    }

    TEST(MultiInstanceManifestTest, MachineARpcPort)
    {
        const uint16_t cExpectedPort{18080};
        const std::string cContent{makeExecutionManifest(cExpectedPort)};

        ArxmlReader _reader(cContent.c_str(), cContent.length());
        const ArxmlNode cPortNode{
            _reader.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "ETHERNET-COMMUNICATION-CONNECTOR", "AP-APPLICATION-ENDPOINTS",
                 "AP-APPLICATION-ENDPOINT", "TP-CONFIGURATION", "TCP-TP",
                 "TCP-TP-PORT", "PORT-NUMBER"})};

        EXPECT_EQ(cPortNode.GetValue<uint16_t>(), cExpectedPort);
    }

    TEST(MultiInstanceManifestTest, MachineBRpcPort)
    {
        const uint16_t cExpectedPort{18081};
        const std::string cContent{makeExecutionManifest(cExpectedPort)};

        ArxmlReader _reader(cContent.c_str(), cContent.length());
        const ArxmlNode cPortNode{
            _reader.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "ETHERNET-COMMUNICATION-CONNECTOR", "AP-APPLICATION-ENDPOINTS",
                 "AP-APPLICATION-ENDPOINT", "TP-CONFIGURATION", "TCP-TP",
                 "TCP-TP-PORT", "PORT-NUMBER"})};

        EXPECT_EQ(cPortNode.GetValue<uint16_t>(), cExpectedPort);
    }

    TEST(MultiInstanceManifestTest, RpcPortsAreDistinct)
    {
        const std::string cMachineA{makeExecutionManifest(18080)};
        const std::string cMachineB{makeExecutionManifest(18081)};

        ArxmlReader _readerA(cMachineA.c_str(), cMachineA.length());
        ArxmlReader _readerB(cMachineB.c_str(), cMachineB.length());

        EXPECT_NE(
            _readerA.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "ETHERNET-COMMUNICATION-CONNECTOR", "AP-APPLICATION-ENDPOINTS",
                 "AP-APPLICATION-ENDPOINT", "TP-CONFIGURATION", "TCP-TP",
                 "TCP-TP-PORT", "PORT-NUMBER"}).GetValue<uint16_t>(),
            _readerB.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "ETHERNET-COMMUNICATION-CONNECTOR", "AP-APPLICATION-ENDPOINTS",
                 "AP-APPLICATION-ENDPOINT", "TP-CONFIGURATION", "TCP-TP",
                 "TCP-TP-PORT", "PORT-NUMBER"}).GetValue<uint16_t>());
    }

    // -----------------------------------------------------------------------
    // Extended vehicle manifest: service instance ID, DoIP port, logical address
    // -----------------------------------------------------------------------

    static std::string makeExtendedVehicleManifest(
        uint16_t serviceInstanceId,
        uint16_t tcpPort,
        uint16_t logicalAddress,
        uint64_t eid)
    {
        return std::string(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<AUTOSAR xmlns=\"http://autosar.org/schema/r4.0\">"
            "<AR-PACKAGES><AR-PACKAGE>"
            "<SHORT-NAME>ExtendedVehicleManifest</SHORT-NAME>"
            "<ELEMENTS>"
            "<DO-IP-INSTANTIATION>"
            "<EID>") + std::to_string(eid) + std::string("</EID>"
            "<GID>1</GID>"
            "<LOGICAL-ADDRESS>") + std::to_string(logicalAddress) + std::string("</LOGICAL-ADDRESS>"
            "</DO-IP-INSTANTIATION>"
            "<ETHERNET-COMMUNICATION-CONNECTOR>"
            "<SHORT-NAME>ExtendedVehicleConn</SHORT-NAME>"
            "<AP-APPLICATION-ENDPOINTS>"
            "<AP-APPLICATION-ENDPOINT>"
            "<SHORT-NAME>ServerUnicastTcp</SHORT-NAME>"
            "<TP-CONFIGURATION><TCP-TP><TCP-TP-PORT>"
            "<PORT-NUMBER>") + std::to_string(tcpPort) + std::string("</PORT-NUMBER>"
            "</TCP-TP-PORT></TCP-TP></TP-CONFIGURATION>"
            "</AP-APPLICATION-ENDPOINT>"
            "</AP-APPLICATION-ENDPOINTS>"
            "</ETHERNET-COMMUNICATION-CONNECTOR>"
            "<PROVIDED-SOMEIP-SERVICE-INSTANCE>"
            "<SERVICE-INTERFACE-DEPLOYMENT>"
            "<SERVICE-INTERFACE-ID>5</SERVICE-INTERFACE-ID>"
            "</SERVICE-INTERFACE-DEPLOYMENT>"
            "<SERVICE-INSTANCE-ID>") + std::to_string(serviceInstanceId) + std::string("</SERVICE-INSTANCE-ID>"
            "</PROVIDED-SOMEIP-SERVICE-INSTANCE>"
            "</ELEMENTS>"
            "</AR-PACKAGE></AR-PACKAGES>"
            "</AUTOSAR>");
    }

    TEST(MultiInstanceManifestTest, MachineAServiceInstanceId)
    {
        const uint16_t cExpectedId{0};
        const std::string cContent{makeExtendedVehicleManifest(cExpectedId, 8081, 1, 1)};

        ArxmlReader _reader(cContent.c_str(), cContent.length());
        const ArxmlNode cNode{
            _reader.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "PROVIDED-SOMEIP-SERVICE-INSTANCE", "SERVICE-INSTANCE-ID"})};

        EXPECT_EQ(cNode.GetValue<uint16_t>(), cExpectedId);
    }

    TEST(MultiInstanceManifestTest, MachineBServiceInstanceId)
    {
        const uint16_t cExpectedId{1};
        const std::string cContent{makeExtendedVehicleManifest(cExpectedId, 8082, 2, 2)};

        ArxmlReader _reader(cContent.c_str(), cContent.length());
        const ArxmlNode cNode{
            _reader.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "PROVIDED-SOMEIP-SERVICE-INSTANCE", "SERVICE-INSTANCE-ID"})};

        EXPECT_EQ(cNode.GetValue<uint16_t>(), cExpectedId);
    }

    TEST(MultiInstanceManifestTest, MachineBDoipPort)
    {
        const uint16_t cExpectedPort{8082};
        const std::string cContent{makeExtendedVehicleManifest(1, cExpectedPort, 2, 2)};

        ArxmlReader _reader(cContent.c_str(), cContent.length());
        const ArxmlNode cNode{
            _reader.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "ETHERNET-COMMUNICATION-CONNECTOR", "AP-APPLICATION-ENDPOINTS",
                 "AP-APPLICATION-ENDPOINT", "TP-CONFIGURATION", "TCP-TP",
                 "TCP-TP-PORT", "PORT-NUMBER"})};

        EXPECT_EQ(cNode.GetValue<uint16_t>(), cExpectedPort);
    }

    TEST(MultiInstanceManifestTest, MachineBLogicalAddress)
    {
        const uint16_t cExpectedAddr{2};
        const std::string cContent{makeExtendedVehicleManifest(1, 8082, cExpectedAddr, 2)};

        ArxmlReader _reader(cContent.c_str(), cContent.length());
        const ArxmlNode cNode{
            _reader.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "DO-IP-INSTANTIATION", "LOGICAL-ADDRESS"})};

        EXPECT_EQ(cNode.GetValue<uint16_t>(), cExpectedAddr);
    }

    TEST(MultiInstanceManifestTest, InstancePortsAreDistinct)
    {
        const std::string cMachineA{makeExtendedVehicleManifest(0, 8081, 1, 1)};
        const std::string cMachineB{makeExtendedVehicleManifest(1, 8082, 2, 2)};

        ArxmlReader _readerA(cMachineA.c_str(), cMachineA.length());
        ArxmlReader _readerB(cMachineB.c_str(), cMachineB.length());

        EXPECT_NE(
            _readerA.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "ETHERNET-COMMUNICATION-CONNECTOR", "AP-APPLICATION-ENDPOINTS",
                 "AP-APPLICATION-ENDPOINT", "TP-CONFIGURATION", "TCP-TP",
                 "TCP-TP-PORT", "PORT-NUMBER"}).GetValue<uint16_t>(),
            _readerB.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "ETHERNET-COMMUNICATION-CONNECTOR", "AP-APPLICATION-ENDPOINTS",
                 "AP-APPLICATION-ENDPOINT", "TP-CONFIGURATION", "TCP-TP",
                 "TCP-TP-PORT", "PORT-NUMBER"}).GetValue<uint16_t>());

        EXPECT_NE(
            _readerA.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "PROVIDED-SOMEIP-SERVICE-INSTANCE", "SERVICE-INSTANCE-ID"}).GetValue<uint16_t>(),
            _readerB.GetRootNode(
                {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS",
                 "PROVIDED-SOMEIP-SERVICE-INSTANCE", "SERVICE-INSTANCE-ID"}).GetValue<uint16_t>());
    }
}
