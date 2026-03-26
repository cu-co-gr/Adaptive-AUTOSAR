#include <atomic>
#include <chrono>
#include <csignal>
#include <future>
#include <map>
#include <string>
#include <thread>
#include "./application/helper/argument_configuration.h"
#include "./application/platform/execution_management.h"

static std::atomic_bool gRunning{true};

static void onSignal(int)
{
    gRunning = false;
}

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
    std::signal(SIGTERM, onSignal);
    std::signal(SIGINT, onSignal);

    const std::string cDefaultConfig{"./configuration/execution_manifest.arxml"};

    std::map<std::string, std::string> _args;
    _args[application::helper::ArgumentConfiguration::cConfigArgument] =
        (argc > 1) ? argv[1] : cDefaultConfig;

    running = true;

    executionManagement = new application::platform::ExecutionManagement(&poller);

    executionManagement->Initialize(_args);

    std::future<void> _future{std::async(std::launch::async, performPolling)};

    const std::chrono::milliseconds cSleepDuration{100};
    while (gRunning)
        std::this_thread::sleep_for(cSleepDuration);

    int _result{executionManagement->Terminate()};
    running = false;
    _future.get();
    delete executionManagement;

    return _result;
}
