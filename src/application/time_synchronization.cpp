#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>
#include "./time_synchronization.h"

namespace
{
    std::atomic_bool gRunning{true};

    void onSignal(int)
    {
        gRunning = false;
    }
}

namespace application
{
    int TimeSynchronizationMain()
    {
        std::signal(SIGTERM, onSignal);
        std::signal(SIGINT, onSignal);

        std::printf("[TimeSyncApp] Time Sync alive\n");

        const std::chrono::seconds cHeartbeatInterval{1};

        while (gRunning)
        {
            std::this_thread::sleep_for(cHeartbeatInterval);
            if (gRunning)
            {
                std::printf("[TimeSyncApp] Time Sync alive\n");
            }
        }

        return 0;
    }
}

int main(int /*argc*/, char * /*argv*/[])
{
    return application::TimeSynchronizationMain();
}
