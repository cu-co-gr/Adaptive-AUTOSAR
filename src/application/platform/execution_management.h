#ifndef EXECUTION_MANAGEMENT_H
#define EXECUTION_MANAGEMENT_H

#include <set>
#include <string>
#include <utility>
#include <vector>
#include "../../ara/exec/state_server.h"
#include "../../ara/exec/helper/modelled_process.h"
#include "./process_manager.h"
#include "../helper/rpc_configuration.h"

/// @brief AUTOSAR application namespace
namespace application
{
    /// @brief AUTOSAR platform application namespace
    namespace platform
    {
        /// @brief Describes a managed process as parsed from the execution manifest
        struct ProcessDescriptor
        {
            std::string shortName;
            std::string executablePath;
            std::vector<std::string> arguments;
            bool isBootstrap{false};
            std::set<std::pair<std::string, std::string>> activatingStates;
        };

        /// @brief Execution managment modelled process
        class ExecutionManagement : public ara::exec::helper::ModelledProcess
        {
        private:
            static const std::string cAppId;

            ProcessManager mProcessManager;
            std::vector<ara::exec::helper::ModelledProcess *> mApplicationProcesses;
            std::vector<ProcessDescriptor> mProcessDescriptors;
            ara::exec::StateServer *mStateServer;

            void onStateChange(
                const std::map<std::string, std::string> &arguments);

            static void fillFunctionGroupStates(
                std::string functionGroupShortName,
                const std::string &functionGroupContent,
                std::set<std::pair<std::string, std::string>> &functionGroupStates);

            static void fillInitialStates(
                std::string functionGroupShortName,
                const std::string &functionGroupContent,
                std::map<std::string, std::string> &initialStates);

            static void fillStates(
                const std::string &configFilepath,
                std::set<std::pair<std::string, std::string>> &functionGroupStates,
                std::map<std::string, std::string> &initialStates);

            static std::vector<ProcessDescriptor> parseProcessDescriptors(
                const std::string &configFilepath);

        protected:
            int Main(
                const std::atomic_bool *cancellationToken,
                const std::map<std::string, std::string> &arguments) override;

        public:
            /// @brief Machine function group name shared with collaborating processes
            static const std::string cMachineFunctionGroup;

            /// @brief Parse RPC configuration from the execution manifest
            /// @param configFilepath Path to the execution manifest ARXML file
            static helper::RpcConfiguration getRpcConfiguration(
                const std::string &configFilepath);

            /// @brief Constructor
            /// @param poller Global poller for network communication
            ExecutionManagement(AsyncBsdSocketLib::Poller *poller);

            ~ExecutionManagement() override;

            /// @brief Register an application process to be lifecycle-managed
            /// @param process Non-owning pointer; caller is responsible for lifetime
            void Register(ara::exec::helper::ModelledProcess *process);
        };
    }
}

#endif