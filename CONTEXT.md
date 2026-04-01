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

## aarch64 Cross-Compilation (current)

Target: aarch64-linux-gnu-gcc 14.2.0 (Debian). Build dir: `build-aarch64/`.

Configure command:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/usr/bin/aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/aarch64-linux-gnu-g++ \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -Dbuild_tests=OFF \
  -S . -B build-aarch64
```

**Fixes applied (2026-04-01):**
- GCC 14 on aarch64 is stricter about implicit transitive includes than GCC 11 on x86.
  161 source files in `src/` and `src-gen/` had `#include <stdexcept>` and/or `#include <cstdint>`
  added wherever `std::runtime_error`/`uint8_t` etc. were used without the explicit include.
- The fetched `async-bsd-socket-lib` dependency (`fifo_sender.h`, `fifo_receiver.h`) also
  needed `#include <cstdint>`. These are patched in-place in `build-aarch64/_deps/`; if the
  build dir is wiped, re-apply these two patches or the configure step will fail to build
  the async socket lib.

All 6 application binaries build cleanly as ARM aarch64 ELF executables.

## TODOS

1. Termination log messages still not visible via run_demo.sh (sed pipe teardown race on Ctrl+C).
   Low priority — functional correctness is confirmed by test_watchdog_event.sh.

2. Machine A WA never subscribes to Machine B EV due to SD client FSM timing.
   Root cause: `ClientRepetitionState` constructor sets `SdClientState::Stopped` as the
   fallthrough state after exhausting repetitions. There is no SD Client Main phase, so a
   late Offer from Machine B EV (which starts slightly after Machine A WA's Repetition
   window) is never seen. Machine B WA works because Machine A EV is already offering by
   the time Machine B WA enters its Repetition phase (Machine A starts ~100 ms earlier due
   to lower RPC port 18080). Fix options: (a) add SD Client Main phase that reacts to late
   Offers, or (b) increase Repetition timeout in watchdog manifest.

3. Both machine_a and machine_b execution_manifest.arxml now have all five process entries
   (SM, PHM, EV, DM, WA) — symmetric deployment confirmed. FIFO-PATH set to /tmp/fifo_18080
   (Machine A) and /tmp/fifo_18081 (Machine B) for PHM and EV processes. WA has no FIFO-PATH.


