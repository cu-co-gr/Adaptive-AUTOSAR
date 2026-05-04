#include <atomic>
#include <chrono>
#include <csignal>
#include <map>
#include <string>
#include <thread>
#include "../../ara/exec/deterministic_client.h"
#include "../helper/argument_configuration.h"
#include "./state_management.h"

static std::atomic_bool gRunning{true};

static void onSignal(int)
{
    gRunning = false;
}

int main(int argc, char *argv[])
{
    std::signal(SIGTERM, onSignal);
    std::signal(SIGINT, onSignal);

    const std::string cDefaultConfig{
        "../../configuration/execution_manifest.arxml"};

    std::map<std::string, std::string> _arguments;
    _arguments[application::helper::ArgumentConfiguration::cConfigArgument] =
        (argc > 1) ? argv[1] : cDefaultConfig;

    AsyncBsdSocketLib::Poller _poller;
    application::platform::StateManagement _stateManagement(&_poller);

    _stateManagement.Initialize(_arguments);

    const std::chrono::milliseconds cCycleDelay{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (gRunning)
    {
        try
        {
            _poller.TryPoll();
        }
        catch (const std::exception &ex)
        {
            std::cerr << "[SM polling] exception: " << ex.what() << "\n" << std::flush;
        }
        std::this_thread::sleep_for(cCycleDelay);
    }

    return _stateManagement.Terminate();
}
