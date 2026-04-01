#ifndef EXTENDED_VEHICLE_H
#define EXTENDED_VEHICLE_H

#include "../ara/exec/helper/modelled_process.h"
#include "../ara/phm/supervised_entity.h"
#include "./helper/network_configuration.h"
#include "./helper/curl_wrapper.h"
#include "./doip/doip_server.h"
#include <cstdint>

namespace application
{
    namespace vehicle_status { class VehicleStatusSkeleton; }

    /// @brief Platform health management checkpoint types
    enum class PhmCheckpointType : uint32_t
    {
        AliveCheckpoint = 0,            ///!< Alive supervision checkpoint
        DeadlineSourceCheckpoint = 1,   ///!< Deadline supervision source checkpoint
        DeadlineTargetCheckpoint = 2    ///!< Deadline supervision target checkpoint
    };

    /// @brief Volvo extended vehicle adaptive application
    class ExtendedVehicle : public ara::exec::helper::ModelledProcess
    {
    private:
        static const std::string cAppId;
        static const ara::core::InstanceSpecifier cSeInstance;

        ara::phm::SupervisedEntity mSupervisedEntity;
        vehicle_status::VehicleStatusSkeleton *mSkeleton;
        helper::CurlWrapper *mCurl;
        doip::DoipServer *mDoipServer;

        std::string mResourcesUrl;

        bool tryConfigureRestCommunication(
            std::string apiKey, std::string bearerToken, std::string &vin);

        static helper::NetworkConfiguration getNetworkConfiguration(
            const arxml::ArxmlReader &reader);

        static DoipLib::ControllerConfig getDoipConfiguration(
            const arxml::ArxmlReader &reader);

        void configureDoipServer(
            const arxml::ArxmlReader &reader, std::string &&vin);

    protected:
        int Main(
            const std::atomic_bool *cancellationToken,
            const std::map<std::string, std::string> &arguments) override;

    public:
        /// @brief Constructor
        /// @param poller Global poller for network communication
        /// @param checkpointCommunicator Medium to communicate the supervision checkpoints
        ExtendedVehicle(
            AsyncBsdSocketLib::Poller *poller,
            ara::phm::CheckpointCommunicator *checkpointCommunicator);

        ~ExtendedVehicle() override;
    };
}

#endif