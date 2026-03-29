#include "../../ara/com/someip/rpc/socket_rpc_server.h"
#include "../../ara/exec/execution_server.h"
#include "../helper/argument_configuration.h"
#include "../../arxml/arxml_reader.h"
#include "./execution_management.h"

namespace application
{
    namespace platform
    {
        const std::string ExecutionManagement::cAppId{"ExecutionManagement"};
        const std::string ExecutionManagement::cMachineFunctionGroup{"MachineFG"};

        ExecutionManagement::ExecutionManagement(
            AsyncBsdSocketLib::Poller *poller) :
                ara::exec::helper::ModelledProcess(cAppId, poller),
                mStateServer{nullptr}
        {
        }

        void ExecutionManagement::Register(ara::exec::helper::ModelledProcess *process)
        {
            mApplicationProcesses.push_back(process);
        }

        helper::RpcConfiguration ExecutionManagement::getRpcConfiguration(
            const std::string &configFilepath)
        {
            const std::string cNetworkEndpoint{"RpcServerEP"};
            const std::string cApplicationEndpoint{"ServerUnicastTcp"};

            helper::RpcConfiguration _result;
            bool _successful{
                helper::TryGetRpcConfiguration(
                    configFilepath, cNetworkEndpoint, cApplicationEndpoint,
                    _result)};

            if (_successful)
            {
                std::cout << "Ehelper::TryGetRpcConfiguration succeed";
                return _result;
            }
            else
            {
                std::cout << "Ehelper::TryGetRpcConfiguration failed";
                throw std::runtime_error("RPC configuration failed.");
            }
        }

        void ExecutionManagement::fillFunctionGroupStates(
            std::string functionGroupShortName,
            const std::string &functionGroupContent,
            std::set<std::pair<std::string, std::string>> &functionGroupStates)
        {
            const arxml::ArxmlReader cArxmlReader(
                functionGroupContent.c_str(), functionGroupContent.length());

            const arxml::ArxmlNodeRange cFunctionGroupStateNodes{
                cArxmlReader.GetNodes(
                    {"FUNCTION-GROUP", "MODE-DECLARATION-GROUP", "MODE-DECLARATIONS"})};

            for (const auto cFunctionGroupStateNode : cFunctionGroupStateNodes)
            {
                std::string _stateShortName{cFunctionGroupStateNode.GetShortName()};
                functionGroupStates.emplace(functionGroupShortName, _stateShortName);
            }
        }

        void ExecutionManagement::fillInitialStates(
            std::string functionGroupShortName,
            const std::string &functionGroupContent,
            std::map<std::string, std::string> &initialStates)
        {
            const std::string cSourceNode{"INITIAL-MODE-REF"};
            const std::string cDestinationType{"MODE-DECLARATION"};

            const arxml::ArxmlReader cArxmlReader(
                functionGroupContent.c_str(), functionGroupContent.length());

            const arxml::ArxmlNode cModeDeclarationGroupNode{
                cArxmlReader.GetRootNode(
                    {"FUNCTION-GROUP", "MODE-DECLARATION-GROUP"})};

            std::string _initialStateShortName;
            bool _successful{
                cModeDeclarationGroupNode.TryGetReference(
                    cSourceNode, cDestinationType, _initialStateShortName)};
            if (_successful)
            {
                initialStates[functionGroupShortName] = _initialStateShortName;
            }
            else
            {
                const std::string cErrorMessage{
                    functionGroupShortName + " lacks an initial state."};
                throw std::runtime_error(cErrorMessage);
            }
        }

        void ExecutionManagement::fillStates(
            const std::string &configFilepath,
            std::set<std::pair<std::string, std::string>> &functionGroupStates,
            std::map<std::string, std::string> &initialStates)
        {
            const arxml::ArxmlReader cArxmlReader(configFilepath);

            const arxml::ArxmlNodeRange cFunctionGroupNodes{
                cArxmlReader.GetNodes(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS", "FUNCTION-GROUPS"})};

            for (const auto cFunctionGroupNode : cFunctionGroupNodes)
            {
                const std::string cShortName{cFunctionGroupNode.GetShortName()};
                const std::string cNodeContent{cFunctionGroupNode.GetContent()};

                fillInitialStates(
                    cShortName, cNodeContent, initialStates);
                fillFunctionGroupStates(
                    cShortName, cNodeContent, functionGroupStates);
            }
        }

        std::vector<ProcessDescriptor> ExecutionManagement::parseProcessDescriptors(
            const std::string &configFilepath)
        {
            std::vector<ProcessDescriptor> _result;

            const arxml::ArxmlReader cArxmlReader(configFilepath);

            const arxml::ArxmlNodeRange cProcessNodes{
                cArxmlReader.GetNodes(
                    {"AUTOSAR", "AR-PACKAGES", "AR-PACKAGE", "ELEMENTS", "PROCESSES"})};

            for (const auto cProcessNode : cProcessNodes)
            {
                ProcessDescriptor _descriptor;
                _descriptor.shortName = cProcessNode.GetShortName();
                const std::string cContent{cProcessNode.GetContent()};

                const arxml::ArxmlReader cProcessReader(
                    cContent.c_str(), cContent.length());

                _descriptor.executablePath =
                    cProcessReader.GetRootNode(
                        {"PROCESS", "EXECUTABLE-PATH"})
                        .GetValue<std::string>();

                const std::string cBootstrapStr{
                    cProcessReader.GetRootNode(
                        {"PROCESS", "BOOTSTRAP"})
                        .GetValue<std::string>()};
                _descriptor.isBootstrap = (cBootstrapStr == "true");

                _descriptor.fifoPath =
                    cProcessReader.GetRootNode({"PROCESS", "FIFO-PATH"})
                        .GetValue<std::string>();

                const arxml::ArxmlNodeRange cArgNodes{
                    cProcessReader.GetNodes(
                        {"PROCESS", "PROCESS-ARGUMENTS"})};

                for (const auto cArgNode : cArgNodes)
                    _descriptor.arguments.push_back(
                        cArgNode.GetValue<std::string>());

                const arxml::ArxmlNodeRange cIrefNodes{
                    cProcessReader.GetNodes(
                        {"PROCESS", "FUNCTION-GROUP-STATE-IREFS"})};

                for (const auto cIrefNode : cIrefNodes)
                {
                    const std::string cIrefContent{cIrefNode.GetContent()};
                    const arxml::ArxmlReader cIrefReader(
                        cIrefContent.c_str(), cIrefContent.length());

                    std::string _fg{
                        cIrefReader.GetRootNode(
                            {"FUNCTION-GROUP-STATE-IREF", "FUNCTION-GROUP-REF"})
                            .GetValue<std::string>()};
                    std::string _state{
                        cIrefReader.GetRootNode(
                            {"FUNCTION-GROUP-STATE-IREF", "FUNCTION-GROUP-STATE-REF"})
                            .GetValue<std::string>()};

                    _descriptor.activatingStates.emplace(
                        std::move(_fg), std::move(_state));
                }

                _result.push_back(std::move(_descriptor));
            }

            return _result;
        }

        void ExecutionManagement::onStateChange(
            const std::map<std::string, std::string> &arguments)
        {
            const std::string cStartUpState{"StartUp"};

            std::string _currentState;
            if (!mStateServer->TryGetState(cMachineFunctionGroup, _currentState))
                return;

            if (_currentState == cStartUpState)
            {
                for (auto *_process : mApplicationProcesses)
                    _process->Initialize(arguments);
            }

            for (const auto &_desc : mProcessDescriptors)
            {
                if (!_desc.isBootstrap &&
                    _desc.activatingStates.count(
                        {cMachineFunctionGroup, _currentState}))
                {
                    std::vector<std::string> _argv{_desc.executablePath};
                    _argv.insert(
                        _argv.end(),
                        _desc.arguments.begin(),
                        _desc.arguments.end());
                    if (!_desc.fifoPath.empty())
                        _argv.push_back(_desc.fifoPath);
                    mProcessManager.Spawn(_desc.executablePath, _argv);
                }
            }
        }

        int ExecutionManagement::Main(
            const std::atomic_bool *cancellationToken,
            const std::map<std::string, std::string> &arguments)
        {
            ara::log::LogStream _logStream;

            try
            {
                const std::string cConfigFilepath{
                    arguments.at(helper::ArgumentConfiguration::cConfigArgument)};

                const helper::RpcConfiguration cRpcConfiguration{
                    getRpcConfiguration(cConfigFilepath)};
                ara::com::someip::rpc::SocketRpcServer _rpcServer(
                    Poller,
                    cRpcConfiguration.ipAddress,
                    cRpcConfiguration.portNumber,
                    cRpcConfiguration.protocolVersion);

                ara::exec::ExecutionServer _executionServer(&_rpcServer);

                mProcessDescriptors = parseProcessDescriptors(cConfigFilepath);

                std::set<std::pair<std::string, std::string>> _functionGroupStates;
                std::map<std::string, std::string> _initialState;
                fillStates(cConfigFilepath, _functionGroupStates, _initialState);
                mStateServer =
                    new ara::exec::StateServer(&_rpcServer,
                                               std::move(_functionGroupStates),
                                               std::move(_initialState));

                auto _onStateChangeCallback{
                    std::bind(&ExecutionManagement::onStateChange, this, arguments)};
                mStateServer->SetNotifier(
                    cMachineFunctionGroup, _onStateChangeCallback);

                for (const auto &_desc : mProcessDescriptors)
                {
                    if (_desc.isBootstrap)
                    {
                        std::vector<std::string> _argv{_desc.executablePath};
                        _argv.insert(
                            _argv.end(),
                            _desc.arguments.begin(),
                            _desc.arguments.end());
                        if (!_desc.fifoPath.empty())
                            _argv.push_back(_desc.fifoPath);
                        mProcessManager.Spawn(_desc.executablePath, _argv);
                    }
                }

                _logStream << "Execution management has been initialized.";
                Log(cLogLevel, _logStream);

                bool _running{true};

                while (!cancellationToken->load() && _running)
                {
                    _running = WaitForActivation();
                    mProcessManager.ReapFinished();
                }

                mProcessManager.TerminateAll();

                int _result{0};
                for (auto *_process : mApplicationProcesses)
                    _result += _process->Terminate();

                _logStream.Flush();
                _logStream << "Execution management has been terminated.";
                Log(cLogLevel, _logStream);

                return _result;
            }
            catch (const std::runtime_error &ex)
            {
                _logStream.Flush();
                _logStream << ex.what();
                Log(cErrorLevel, _logStream);
                

                return cUnsuccessfulExitCode;
            }
        }

        ExecutionManagement::~ExecutionManagement()
        {
            for (auto *_process : mApplicationProcesses)
                _process->Terminate();

            if (mStateServer)
                delete mStateServer;
        }
    }
}