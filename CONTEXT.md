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

**Stage 7 (current):** ExecutionClient decoupling + run_demo.sh robustness.

- `<FIFO-PATH>` added to PlatformHealthManagement and ExtendedVehicle process entries in
  `configuration/machine_a/execution_manifest.arxml`. EM parses it in `parseProcessDescriptors()`
  (new `fifoPath` field on `ProcessDescriptor`) and appends it to the child's argv at spawn time.
- `platform_health_management_main.cpp` and `extended_vehicle_main.cpp` no longer include
  `execution_management.h`; FIFO path is read from `argv[3]` and the function group name is
  the manifest literal `"MachineFG"`.
- All five child-process mains now call `ExecutionClient::ReportExecutionState(kRunning)` per
  SWS_EM §5. Each main creates a `SocketRpcClient` connecting back to EM's RPC server (host/port
  parsed from the execution manifest via `helper::TryGetRpcConfiguration()`). The call runs in a
  `std::async` thread so the main-thread poller loop can service the send/receive callbacks.
  `CMakeLists.txt`: added `network_configuration` and `rpc_configuration` sources to the
  `watchdog_application` target to satisfy the new link dependency.
- `run_demo.sh` cleanup() now pkills known child binaries after `wait`, matching what
  `test_watchdog_event.sh` already did.
- All 21 functional checks pass; 475/479 unit tests pass (4 pre-existing env failures).

**Stage 6:** ara::com Proxy/Skeleton layer introduced per SWS_CM §8.2–8.4.
Applications no longer interact with SOME/IP directly.

- `src/ara/com/service_skeleton.h` and `service_proxy.h`: framework base classes.
- `src-gen/vehicle_status/`: hand-generated Proxy and Skeleton for the VehicleStatus service
  (VehicleStatusData with Serialize/Deserialize, VehicleStatusSkeleton wrapping SOME/IP SD server
  + PubSub server, VehicleStatusProxy wrapping SOME/IP SD client + PubSub receiver).
- `ExtendedVehicle` refactored to use `VehicleStatusSkeleton` exclusively.
- `WatchdogApplication` refactored to use `VehicleStatusProxy` exclusively; `mFirstEventReceived`
  flag added so the watchdog timeout only fires after at least one event has been seen (avoids
  false expiry during SD discovery startup window).
- `test_watchdog_event.sh` cleanup() extended with pkill for orphaned child processes; `-a` flag
  added to all grep calls to handle binary log files; watchdog-fired check moved after Machine B
  kill to guarantee log buffer flush.
- All 21 functional test checks pass; 475/479 unit tests pass (4 pre-existing env failures).

## TODOS

1. Termination log messages still not visible via run_demo.sh (sed pipe teardown race on Ctrl+C).
   Low priority — functional correctness is confirmed by test_watchdog_event.sh.
