#include <termios.h>
#include <unistd.h>
#include <iostream>
#include "./argument_configuration.h"

namespace application
{
    namespace helper
    {
        const std::string ArgumentConfiguration::cConfigArgument{"config"};
        const std::string ArgumentConfiguration::cEvConfigArgument{"evconfig"};
        const std::string ArgumentConfiguration::cDmConfigArgument{"dmconfig"};
        const std::string ArgumentConfiguration::cPhmConfigArgument{"phmconfig"};
        const std::string ArgumentConfiguration::cWatchdogConfigArgument{"watchdogconfig"};
        const std::string ArgumentConfiguration::cApiKeyArgument{"vccapikey"};
        const std::string ArgumentConfiguration::cBearerTokenArgument{"bearertoken"};

        ArgumentConfiguration::ArgumentConfiguration(
            int argc,
            char *argv[],
            std::string defaultConfigFile,
            std::string extendedVehicleConfigFile,
            std::string diagnosticManagerConfigFile,
            std::string healthMonitoringConfigFile,
            std::string watchdogConfigFile)
        {
            const int cConfigArgumentIndex{1};
            const int cEvConfigArgumentIndex{2};
            const int cDmConfigArgumentIndex{3};
            const int cPhmConfigArgumentIndex{4};
            const int cWatchdogConfigArgumentIndex{5};

            if (argc > cPhmConfigArgumentIndex)
            {
                std::string _configFilepath{argv[cConfigArgumentIndex]};
                mArguments[cConfigArgument] = _configFilepath;

                std::string _evConfigFilepath{argv[cEvConfigArgumentIndex]};
                mArguments[cEvConfigArgument] = _evConfigFilepath;

                std::string _dmConfigFilepath{argv[cDmConfigArgumentIndex]};
                mArguments[cDmConfigArgument] = _dmConfigFilepath;

                std::string _phmConfigFilepath{argv[cPhmConfigArgumentIndex]};
                mArguments[cPhmConfigArgument] = _phmConfigFilepath;

                if (argc > cWatchdogConfigArgumentIndex)
                {
                    mArguments[cWatchdogConfigArgument] =
                        argv[cWatchdogConfigArgumentIndex];
                }
                else
                {
                    // Derive watchdog manifest from the execution manifest directory
                    std::string _execPath{argv[cConfigArgumentIndex]};
                    std::size_t _sep{_execPath.find_last_of("/\\")};
                    std::string _dir{
                        _sep != std::string::npos
                            ? _execPath.substr(0, _sep + 1)
                            : ""};
                    mArguments[cWatchdogConfigArgument] =
                        _dir + "watchdog_manifest.arxml";
                }
            }
            else
            {
                mArguments[cConfigArgument] = defaultConfigFile;
                mArguments[cEvConfigArgument] = extendedVehicleConfigFile;
                mArguments[cDmConfigArgument] = diagnosticManagerConfigFile;
                mArguments[cPhmConfigArgument] = healthMonitoringConfigFile;
                mArguments[cWatchdogConfigArgument] = watchdogConfigFile;
            }
        }

        bool ArgumentConfiguration::trySetEchoMode(bool enabled)
        {
            const int cSuccessfulCode{0};

            struct termios _tty;
            // Read the console input file descriptor attributes
            int _successful{tcgetattr(STDIN_FILENO, &_tty)};

            if (_successful == cSuccessfulCode)
            {
                if (enabled)
                {
                    // Enable the echo attribute flag
                    _tty.c_lflag |= ECHO;
                }
                else
                {
                    // Disable the echo attribute flag
                    _tty.c_lflag &= ~ECHO;
                }

                // Appy the modified attributes to the file descriptor immediately
                _successful = tcsetattr(STDIN_FILENO, TCSANOW, &_tty);
            }

            bool _result{_successful == cSuccessfulCode};
            return _result;
        }

        bool ArgumentConfiguration::tryAskSafely(
            std::string message, std::string argumentKey)
        {
            std::cout << message << std::endl;
            bool _result{trySetEchoMode(false)};

            if (_result)
            {
                std::string userInput;
                std::cin >> userInput;
                mArguments[argumentKey] = userInput;

                _result = trySetEchoMode(true);
                if (!_result)
                {
                    // Revert the arguments dictionary to the state before the function call
                    mArguments.erase(argumentKey);
                }
            }

            return _result;
        }

        const std::map<std::string, std::string> &ArgumentConfiguration::GetArguments() const noexcept
        {
            return mArguments;
        }

        bool ArgumentConfiguration::TryAskingVccApiKey(std::string message)
        {
            return tryAskSafely(message, cApiKeyArgument);
        }

        bool ArgumentConfiguration::TryAskingBearToken(std::string message)
        {
            return tryAskSafely(message, cBearerTokenArgument);
        }

        void ArgumentConfiguration::SetDemoMode()
        {
            mArguments[cApiKeyArgument] = "";
            mArguments[cBearerTokenArgument] = "";
        }
    }
}