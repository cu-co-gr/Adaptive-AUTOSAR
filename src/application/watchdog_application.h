#ifndef WATCHDOG_APPLICATION_H
#define WATCHDOG_APPLICATION_H

#include <chrono>
#include <cstdint>
#include "../ara/exec/helper/modelled_process.h"

namespace application
{
    namespace vehicle_status
    {
        class VehicleStatusProxy;
        struct VehicleStatusData;
    }

    /// @brief Watchdog application: subscribes to VehicleStatus events and
    ///        logs a warning when no event is received within the timeout window.
    class WatchdogApplication : public ara::exec::helper::ModelledProcess
    {
    private:
        static const std::string cAppId;
        static const std::chrono::milliseconds cEventTimeout;

        vehicle_status::VehicleStatusProxy *mProxy{nullptr};

        uint16_t mSelfInstanceId{0};
        std::chrono::steady_clock::time_point mLastEventTime;
        bool mExpiredLogged{false};
        bool mFirstEventReceived{false};

        void onEventReceived(const vehicle_status::VehicleStatusData &data);

    protected:
        int Main(
            const std::atomic_bool *cancellationToken,
            const std::map<std::string, std::string> &arguments) override;

    public:
        /// @brief Constructor
        /// @param poller Global poller for network communication
        explicit WatchdogApplication(AsyncBsdSocketLib::Poller *poller);

        ~WatchdogApplication() override;
    };
}

#endif
