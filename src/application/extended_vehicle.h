#ifndef EXTENDED_VEHICLE_H
#define EXTENDED_VEHICLE_H

#include "../ara/exec/helper/modelled_process.h"
#include "../ara/phm/supervised_entity.h"
#include "../ara/com/someip/sd/someip_sd_server.h"
#include "../ara/com/someip/pubsub/someip_pubsub_server.h"
#include "../ara/com/someip/pubsub/pubsub_event_network_layer.h"
#include "./helper/network_configuration.h"
#include "./helper/curl_wrapper.h"
#include "./doip/doip_server.h"

namespace application
{
    /// @brief Vehicle status event payload (TelematicControlModuleEvent)
    struct VehicleStatusType
    {
        std::string vin;           ///< 17-character Vehicle Identification Number
        bool connectedToCloud;     ///< Whether the cloud API is reachable
    };

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
        ara::com::helper::NetworkLayer<ara::com::someip::sd::SomeIpSdMessage> *mNetworkLayer;
        ara::com::someip::sd::SomeIpSdServer *mSdServer;
        ara::com::someip::pubsub::SomeIpPubSubServer *mPubSubServer;
        ara::com::someip::pubsub::PubSubEventNetworkLayer *mEventLayer;
        helper::CurlWrapper *mCurl;
        doip::DoipServer *mDoipServer;

        std::string mResourcesUrl;

        void configureNetworkLayer(const arxml::ArxmlReader &reader);
        bool tryConfigureRestCommunication(
            std::string apiKey, std::string bearerToken, std::string &vin);

        static helper::NetworkConfiguration getNetworkConfiguration(
            const arxml::ArxmlReader &reader);

        void configureSdServer(const arxml::ArxmlReader &reader);
        void configurePubSubServer(const arxml::ArxmlReader &reader);

        void publishVehicleStatus(
            const std::string &vin, bool connectedToCloud);

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