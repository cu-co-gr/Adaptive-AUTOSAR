#include "./process_manager.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace application
{
    namespace platform
    {
        ProcessManager::~ProcessManager()
        {
            TerminateAll();
        }

        pid_t ProcessManager::Spawn(
            const std::string &executablePath,
            const std::vector<std::string> &argv)
        {
            std::vector<char *> _cArgv;
            _cArgv.reserve(argv.size() + 1);
            for (const auto &_arg : argv)
                _cArgv.push_back(const_cast<char *>(_arg.c_str()));
            _cArgv.push_back(nullptr);

            pid_t _pid{fork()};

            if (_pid == 0)
            {
                // Child: replace image with the target binary
                execv(executablePath.c_str(), _cArgv.data());
                // execv only returns on error
                std::cerr << "[ProcessManager] execv failed for "
                          << executablePath << ": "
                          << std::strerror(errno) << "\n";
                _exit(1);
            }
            else if (_pid > 0)
            {
                mChildren[_pid] = executablePath;
            }
            else
            {
                std::cerr << "[ProcessManager] fork failed: "
                          << std::strerror(errno) << "\n";
            }

            return _pid;
        }

        std::vector<std::pair<pid_t, int>> ProcessManager::ReapFinished()
        {
            std::vector<std::pair<pid_t, int>> _finished;
            int _status;
            pid_t _pid;

            while ((_pid = waitpid(-1, &_status, WNOHANG)) > 0)
            {
                int _exitCode{WIFEXITED(_status) ? WEXITSTATUS(_status) : 1};
                _finished.emplace_back(_pid, _exitCode);
                mChildren.erase(_pid);
            }

            return _finished;
        }

        void ProcessManager::TerminateAll(int timeoutMs)
        {
            for (const auto &_kv : mChildren)
                kill(_kv.first, SIGTERM);

            const int cPollIntervalMs{50};
            int _elapsed{0};

            while (!mChildren.empty() && _elapsed < timeoutMs)
            {
                ReapFinished();
                if (!mChildren.empty())
                {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(cPollIntervalMs));
                    _elapsed += cPollIntervalMs;
                }
            }

            // Force-kill any children that ignored SIGTERM
            for (const auto &_kv : mChildren)
                kill(_kv.first, SIGKILL);

            int _status;
            for (const auto &_kv : mChildren)
                waitpid(_kv.first, &_status, 0);

            mChildren.clear();
        }
    }
}
