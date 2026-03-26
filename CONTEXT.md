# Context
In order to create and deploy adaptive applications in a way as close as possible to a commercial
environment we need to expand capabilities of the Adaptive Platform (AP). In the big picture this
means also to review APIs and Artifacts to be more complete and consistent with the AUTOSAR SWSs.

The first architectural gap addressed was to split the executables so that Execution Manager
indeed takes responsibility to start and execute them as independent OS processes.

State Manager did not require major work to be consistent with the SWSs as it already provides
sufficient interfaces to orchestrate state changes once EM hands over.

## Completed

**Stage 5 (committed d14d317e):** Each platform service (ExtendedVehicle, DiagnosticManager,
PlatformHealthManagement, WatchdogApplication) has its own main() entry point and CMake target.
EM now has a SIGTERM/SIGINT handler so that Ctrl+C triggers a clean shutdown sequence:
signal → gRunning=false → Terminate() → ProcessManager::TerminateAll() → SIGTERM to children
(2s grace) → SIGKILL backstop. Each child runs its own Terminate() within the grace window.

## TODOS

1. ~~Confirm if all processes are terminated correctly~~ — **Done.** Root cause was that `main.cpp`
   had no signal handler, so EM died immediately on Ctrl+C without calling TerminateAll().
   Fixed in Stage 5.

2. Dangling processes on re-run (demo stuck/crashed on second launch after SSH reconnect).
   Now that EM calls TerminateAll() on shutdown, this should be resolved. Needs re-testing.
   Remaining risk: TCP TIME_WAIT on RPC ports if shutdown completes before OS releases sockets.

3. Termination log messages still not visible: `sed` (the log prefixer in run_demo.sh) dies
   simultaneously with all other processes on SIGTERM, so the pipe tears down before children
   can flush their "terminated" log lines. Low priority — functional correctness is confirmed
   by test_watchdog_event.sh.
