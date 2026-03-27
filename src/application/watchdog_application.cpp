#include <functional>
#include "../ara/log/log_stream.h"
#include "./helper/argument_configuration.h"
#include "../../src-gen/vehicle_status/vehicle_status_proxy.h"
#include "../../src-gen/vehicle_status/vehicle_status_data.h"
#include "./watchdog_application.h"

namespace application
{
    const std::string WatchdogApplication::cAppId{"WatchdogApplication"};
    const std::chrono::milliseconds WatchdogApplication::cEventTimeout{2000};

    WatchdogApplication::WatchdogApplication(
        AsyncBsdSocketLib::Poller *poller)
        : ModelledProcess(cAppId, poller)
    {
    }

    void WatchdogApplication::onEventReceived(
        const vehicle_status::VehicleStatusData &data)
    {
        mLastEventTime = std::chrono::steady_clock::now();
        mExpiredLogged = false;
        mFirstEventReceived = true;

        ara::log::LogStream _stream;
        _stream << "[Watchdog] "
                << static_cast<uint32_t>(mSelfInstanceId)
                << " restarted by ExtendedVehicle "
                << static_cast<uint32_t>(data.SenderInstanceId);
        Log(cLogLevel, _stream);
    }

    int WatchdogApplication::Main(
        const std::atomic_bool *cancellationToken,
        const std::map<std::string, std::string> &arguments)
    {
        const std::string &_manifestPath{
            arguments.at(
                helper::ArgumentConfiguration::cWatchdogConfigArgument)};

        mProxy = new vehicle_status::VehicleStatusProxy(Poller, _manifestPath);
        mSelfInstanceId = mProxy->GetSelfInstanceId();

        mProxy->events_VehicleStatus_SetReceiveHandler(
            std::bind(&WatchdogApplication::onEventReceived, this,
                      std::placeholders::_1));

        mLastEventTime = std::chrono::steady_clock::now();

        ara::log::LogStream _startStream;
        _startStream << "[Watchdog] Started - timeout "
                     << static_cast<uint32_t>(cEventTimeout.count()) << " ms";
        Log(cLogLevel, _startStream);

        bool _running{true};
        while (_running)
        {
            _running = WaitForActivation();

            auto _now{std::chrono::steady_clock::now()};
            auto _elapsed{
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    _now - mLastEventTime)};

            if (_elapsed > cEventTimeout && !mExpiredLogged && mFirstEventReceived)
            {
                ara::log::LogStream _warnStream;
                _warnStream << "[Watchdog] Event expired: no VehicleStatus"
                            << " received for >"
                            << static_cast<uint32_t>(cEventTimeout.count())
                            << " ms";
                Log(cLogLevel, _warnStream);
                mExpiredLogged = true;
            }
        }

        delete mProxy;
        mProxy = nullptr;

        return cSuccessfulExitCode;
    }

    WatchdogApplication::~WatchdogApplication()
    {
        delete mProxy;
    }
}
