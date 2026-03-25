#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <map>
#include <string>
#include <vector>
#include <sys/types.h>

namespace application
{
    namespace platform
    {
        /// @brief Manages the lifecycle of child OS processes spawned by EM
        class ProcessManager
        {
        public:
            ProcessManager() = default;
            ~ProcessManager();

            /// @brief Fork and exec a child process
            /// @param executablePath Absolute or relative path to the binary
            /// @param argv Full argument list (argv[0] should be the binary name/path)
            /// @return Child PID on success, -1 on fork failure
            pid_t Spawn(
                const std::string &executablePath,
                const std::vector<std::string> &argv);

            /// @brief Send SIGTERM to all children; wait up to timeoutMs; then SIGKILL
            /// @param timeoutMs Grace period before force-killing (milliseconds)
            void TerminateAll(int timeoutMs = 2000);

            /// @brief Non-blocking reap of any finished children
            /// @return List of (pid, exitCode) pairs for processes that exited
            std::vector<std::pair<pid_t, int>> ReapFinished();

        private:
            // pid → short name for logging
            std::map<pid_t, std::string> mChildren;
        };
    }
}

#endif
