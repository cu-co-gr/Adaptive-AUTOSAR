#include "./application/helper/argument_configuration.h"
#include "./application/helper/fifo_checkpoint_communicator.h"
#include "./application/platform/execution_management.h"
#include "./application/platform/platform_health_management.h"
#include "./application/extended_vehicle.h"
#include "./application/platform/diagnostic_manager.h"
#include "./application/watchdog_application.h"

bool running;
AsyncBsdSocketLib::Poller poller;
application::platform::ExecutionManagement *executionManagement;

void performPolling()
{
    const std::chrono::milliseconds cSleepDuration{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (running)
    {
        poller.TryPoll();
        std::this_thread::sleep_for(cSleepDuration);
    }
}

int main(int argc, char *argv[])
{
    application::helper::ArgumentConfiguration _argumentConfiguration(argc, argv);

    std::cout << "Running in demo mode (Volvo VCC API not available)." << std::endl;
    _argumentConfiguration.SetDemoMode();

    running = true;
    const auto &_args = _argumentConfiguration.GetArguments();

    const std::string &_configFilepath{
        _args.at(application::helper::ArgumentConfiguration::cConfigArgument)};
    const auto _rpcConfig{
        application::platform::ExecutionManagement::getRpcConfiguration(
            _configFilepath)};

    application::helper::FifoCheckpointCommunicator communicator(
        &poller,
        "/tmp/fifo_" + std::to_string(_rpcConfig.portNumber));

    executionManagement = new application::platform::ExecutionManagement(&poller);

    application::platform::PlatformHealthManagement platformHealthManagement(
        &poller,
        &communicator,
        application::platform::ExecutionManagement::cMachineFunctionGroup);
    application::ExtendedVehicle extendedVehicle(&poller, &communicator);
    application::platform::DiagnosticManager diagnosticManager(&poller);
    application::WatchdogApplication watchdogApplication(&poller);

    executionManagement->Register(&platformHealthManagement);
    executionManagement->Register(&extendedVehicle);
    executionManagement->Register(&diagnosticManager);
    executionManagement->Register(&watchdogApplication);

    executionManagement->Initialize(_args);

    std::future<void> _future{std::async(std::launch::async, performPolling)};

    std::getchar();
    std::system("clear");
    std::getchar();

    int _result{executionManagement->Terminate()};
    running = false;
    _future.get();
    delete executionManagement;

    return _result;
}