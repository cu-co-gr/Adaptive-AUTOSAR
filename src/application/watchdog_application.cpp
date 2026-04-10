#include <functional>
#include <sys/stat.h>
#include "../ara/log/log_stream.h"
#include "./helper/argument_configuration.h"
#include "../../src-gen/vehicle_status/vehicle_status_proxy.h"
#include "../../src-gen/vehicle_status/vehicle_status_data.h"
#include "./watchdog_application.h"
#include <cstdint>

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

        std::string _msg =
            std::string{"[Watchdog] "} +
            std::to_string(static_cast<uint32_t>(mSelfInstanceId)) +
            " restarted by ExtendedVehicle " +
            std::to_string(static_cast<uint32_t>(data.SenderInstanceId));

        ara::log::LogStream _stream;
        _stream << _msg;
        Log(cLogLevel, _stream);

        // Note: not thread-safe — single-writer assumption applies (no concurrent
        // access in this educational deployment).
        if (mStorage)
        {
            mStorage->SetValue(std::to_string(mLogCounter), _msg);
            ++mLogCounter;
            ara::log::LogStream _perStream;
            _perStream << "[Per] Entry " << mLogCounter - 1
                       << " written to " << mStoragePath;
            Log(cLogLevel, _perStream);
        }
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

        // Open persistency storage now that the instance ID is known.
        // Try /var/per first; fall back to /tmp/per if not writable.
        {
            const std::string cPreferred{"/var/per"};
            const std::string cFallback{"/tmp/per"};
            std::string _perDir;
            if (::mkdir(cPreferred.c_str(), 0755) == 0 || errno == EEXIST)
                _perDir = cPreferred;
            else
            {
                ::mkdir(cFallback.c_str(), 0755);
                _perDir = cFallback;
            }
            mStoragePath =
                _perDir + "/watchdog_" +
                std::to_string(static_cast<uint32_t>(mSelfInstanceId)) + ".json";
        }
        auto _openResult{ara::per::KeyValueStorage::Open(mStoragePath)};
        {
            ara::log::LogStream _perStream;
            if (_openResult.HasValue())
            {
                auto _kvs{std::move(_openResult).Value()};
                mStorage = std::unique_ptr<ara::per::KeyValueStorage>(
                    new ara::per::KeyValueStorage(std::move(_kvs)));
                _perStream << "[Per] Storage opened at " << mStoragePath;
            }
            else
            {
                _perStream << "[Per] Storage unavailable at " << mStoragePath;
            }
            Log(cLogLevel, _perStream);
        }

        mProxy->events_VehicleStatus_SetReceiveHandler(
            std::bind(&WatchdogApplication::onEventReceived, this,
                      std::placeholders::_1));

        mLastEventTime = std::chrono::steady_clock::now();

        std::string _startMsg =
            std::string{"[Watchdog] Started - timeout "} +
            std::to_string(static_cast<uint32_t>(cEventTimeout.count())) +
            " ms";

        ara::log::LogStream _startStream;
        _startStream << _startMsg;
        Log(cLogLevel, _startStream);

        if (mStorage)
        {
            mStorage->SetValue(std::to_string(mLogCounter), _startMsg);
            ++mLogCounter;
            ara::log::LogStream _perStream;
            _perStream << "[Per] Entry 0 written to " << mStoragePath;
            Log(cLogLevel, _perStream);
        }

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
                std::string _expMsg =
                    std::string{"[Watchdog] Event expired: no VehicleStatus"} +
                    " received for >" +
                    std::to_string(static_cast<uint32_t>(cEventTimeout.count())) +
                    " ms";

                ara::log::LogStream _warnStream;
                _warnStream << _expMsg;
                Log(cLogLevel, _warnStream);
                mExpiredLogged = true;

                if (mStorage)
                {
                    mStorage->SetValue(std::to_string(mLogCounter), _expMsg);
                    ++mLogCounter;
                    mStorage->SyncToStorage();
                    ara::log::LogStream _perStream;
                    _perStream << "[Per] Synced " << mLogCounter
                               << " entries to " << mStoragePath;
                    Log(cLogLevel, _perStream);
                }
            }
        }

        delete mProxy;
        mProxy = nullptr;

        if (mStorage)
        {
            mStorage->SyncToStorage();
            ara::log::LogStream _perStream;
            _perStream << "[Per] Synced " << mLogCounter
                       << " entries to " << mStoragePath;
            Log(cLogLevel, _perStream);
        }

        return cSuccessfulExitCode;
    }

    WatchdogApplication::~WatchdogApplication()
    {
        delete mProxy;
    }
}
