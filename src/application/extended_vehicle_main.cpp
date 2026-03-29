#include <atomic>
#include <chrono>
#include <csignal>
#include <future>
#include <map>
#include <string>
#include <thread>
#include "../ara/com/someip/rpc/socket_rpc_client.h"
#include "../ara/core/instance_specifier.h"
#include "../ara/exec/deterministic_client.h"
#include "../ara/exec/execution_client.h"
#include "./helper/argument_configuration.h"
#include "./helper/fifo_checkpoint_communicator.h"
#include "./helper/rpc_configuration.h"
#include "./extended_vehicle.h"

static std::atomic_bool gRunning{true};

static void onSignal(int)
{
    gRunning = false;
}

int main(int argc, char *argv[])
{
    std::signal(SIGTERM, onSignal);
    std::signal(SIGINT, onSignal);

    const std::string cDefaultExecManifest{
        "../../configuration/execution_manifest.arxml"};
    const std::string cDefaultEvManifest{
        "../../configuration/extended_vehicle_manifest.arxml"};
    const std::string cDefaultFifoPath{"/tmp/fifo_18080"};

    const std::string _execManifest{argc > 1 ? argv[1] : cDefaultExecManifest};
    const std::string _evManifest{argc > 2 ? argv[2] : cDefaultEvManifest};
    const std::string _fifoPath{argc > 3 ? argv[3] : cDefaultFifoPath};

    std::map<std::string, std::string> _arguments;
    _arguments[application::helper::ArgumentConfiguration::cConfigArgument] =
        _execManifest;
    _arguments[application::helper::ArgumentConfiguration::cEvConfigArgument] =
        _evManifest;
    _arguments[application::helper::ArgumentConfiguration::cApiKeyArgument] = "";
    _arguments[application::helper::ArgumentConfiguration::cBearerTokenArgument] = "";

    application::helper::RpcConfiguration _rpcConfig;
    application::helper::TryGetRpcConfiguration(
        _execManifest, "RpcServerEP", "ServerUnicastTcp", _rpcConfig);

    AsyncBsdSocketLib::Poller _poller;
    application::helper::FifoCheckpointCommunicator _communicator(
        &_poller, _fifoPath);

    ara::com::someip::rpc::SocketRpcClient _rpcClient(
        &_poller,
        _rpcConfig.ipAddress,
        _rpcConfig.portNumber,
        _rpcConfig.protocolVersion);

    ara::core::InstanceSpecifier _instanceSpec{"ExtendedVehicle"};
    ara::exec::ExecutionClient _execClient(_instanceSpec, &_rpcClient);

    application::ExtendedVehicle _extendedVehicle(&_poller, &_communicator);

    _extendedVehicle.Initialize(_arguments);

    auto _stateReport = std::async(
        std::launch::async,
        [&_execClient]()
        {
            return _execClient.ReportExecutionState(
                ara::exec::ExecutionState::kRunning);
        });

    const std::chrono::milliseconds cCycleDelay{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (gRunning)
    {
        _poller.TryPoll();
        std::this_thread::sleep_for(cCycleDelay);
    }

    _stateReport.wait_for(std::chrono::seconds{1});
    return _extendedVehicle.Terminate();
}
