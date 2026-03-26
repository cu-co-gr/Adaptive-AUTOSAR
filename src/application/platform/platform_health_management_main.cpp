#include <atomic>
#include <chrono>
#include <csignal>
#include <map>
#include <string>
#include <thread>
#include "../../ara/exec/deterministic_client.h"
#include "../helper/argument_configuration.h"
#include "../helper/fifo_checkpoint_communicator.h"
#include "../helper/rpc_configuration.h"
#include "./execution_management.h"
#include "./platform_health_management.h"

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
    const std::string cDefaultPhmManifest{
        "../../configuration/health_monitoring_manifest.arxml"};

    const std::string _execManifest{argc > 1 ? argv[1] : cDefaultExecManifest};
    const std::string _phmManifest{argc > 2 ? argv[2] : cDefaultPhmManifest};

    std::map<std::string, std::string> _arguments;
    _arguments[application::helper::ArgumentConfiguration::cConfigArgument] =
        _execManifest;
    _arguments[application::helper::ArgumentConfiguration::cPhmConfigArgument] =
        _phmManifest;

    const auto _rpcConfig{
        application::platform::ExecutionManagement::getRpcConfiguration(
            _execManifest)};
    const std::string _fifoPath{
        "/tmp/fifo_" + std::to_string(_rpcConfig.portNumber)};

    AsyncBsdSocketLib::Poller _poller;
    application::helper::FifoCheckpointCommunicator _communicator(
        &_poller, _fifoPath);

    application::platform::PlatformHealthManagement _phm(
        &_poller,
        &_communicator,
        application::platform::ExecutionManagement::cMachineFunctionGroup);

    _phm.Initialize(_arguments);

    const std::chrono::milliseconds cCycleDelay{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (gRunning)
    {
        _poller.TryPoll();
        std::this_thread::sleep_for(cCycleDelay);
    }

    return _phm.Terminate();
}
