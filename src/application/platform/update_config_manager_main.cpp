#include <atomic>
#include <chrono>
#include <csignal>
#include <string>
#include <thread>
#include <asyncbsdsocket/poller.h>
#include "../../ara/exec/deterministic_client.h"
#include "./update_config_manager.h"

static std::atomic_bool gRunning{true};

static void onSignal(int)
{
    gRunning = false;
}

int main(int argc, char *argv[])
{
    std::signal(SIGTERM, onSignal);
    std::signal(SIGINT, onSignal);

    const std::string cDefaultManifest{
        "../../configuration/machine_a/ucm_manifest.arxml"};
    const std::string cDefaultStorageRoot{"/tmp/per/ucm"};

    const std::string _manifest{argc > 1 ? argv[1] : cDefaultManifest};
    const std::string _storageRoot{argc > 2 ? argv[2] : cDefaultStorageRoot};

    std::printf("[UCM] Starting (manifest: %s, storage: %s)\n",
        _manifest.c_str(), _storageRoot.c_str());

    AsyncBsdSocketLib::Poller _poller;

    application::platform::UpdateConfigManager _ucm(
        &_poller, _manifest, _storageRoot);

    const std::chrono::milliseconds cCycleDelay{
        ara::exec::DeterministicClient::cCycleDelayMs};

    while (gRunning)
    {
        _poller.TryPoll();
        std::this_thread::sleep_for(cCycleDelay);
    }

    std::printf("[UCM] Shutting down\n");
    return 0;
}
