#include <atomic>
#include <chrono>
#include <csignal>
#include <map>
#include <string>
#include <thread>
#include "../../ara/exec/deterministic_client.h"
#include "../helper/argument_configuration.h"
#include "./diagnostic_manager.h"

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
    const std::string cDefaultDmManifest{
        "../../configuration/diagnostic_manager_manifest.arxml"};

    const std::string _execManifest{argc > 1 ? argv[1] : cDefaultExecManifest};
    const std::string _dmManifest{argc > 2 ? argv[2] : cDefaultDmManifest};

    std::map<std::string, std::string> _arguments;
    _arguments[application::helper::ArgumentConfiguration::cConfigArgument] =
        _execManifest;
    _arguments[application::helper::ArgumentConfiguration::cDmConfigArgument] =
        _dmManifest;

    AsyncBsdSocketLib::Poller _poller;
    application::platform::DiagnosticManager _diagnosticManager(&_poller);

    _diagnosticManager.Initialize(_arguments);

    const std::chrono::milliseconds cCycleDelay{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (gRunning)
    {
        _poller.TryPoll();
        std::this_thread::sleep_for(cCycleDelay);
    }

    return _diagnosticManager.Terminate();
}
