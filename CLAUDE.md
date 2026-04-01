# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Session management
- At the start of each session, read CONTEXT.md if it exists
- After completing any significant task, update CONTEXT.md with:
  - What was just done
  - Current state of the codebase
  - Next steps
- Before ending a session, always update CONTEXT.md

## Build Commands

Requires GCC 11.2.0 or Clang 14.0.0. All external dependencies are fetched automatically via CMake `FetchContent`.

```bash
# Configure (GCC)
cmake -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/usr/bin/x86_64-linux-gnu-gcc-11 \
  -DCMAKE_CXX_COMPILER=/usr/bin/x86_64-linux-gnu-g++-11 \
  -S . -B build

# Configure (Clang)
cmake -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/usr/bin/clang-14 \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-14 \
  -S . -B build

# Build
cmake --build build
```

## Testing

Uses Google Test (v1.12.1). Two test targets: `arxml_unit_test` (ARXML parsing) and `ara_unit_test` (all ARA interfaces).

```bash
# Run all tests
cd build && ctest

# Run a specific test binary
./build/bin/ara_unit_test
./build/bin/arxml_unit_test

# Run a single test case
./build/bin/ara_unit_test --gtest_filter=TestClassName.TestName
```

Test files live under `test/` and mirror the `src/` directory structure.

## Running the Application

Each platform service (SM, PHM, EV, DM, WA) is a separate binary spawned by Execution Management as independent OS processes. The top-level entry point is `adaptive_autosar` (EM), which reads manifests and forks children.

```bash
# Single machine (machine_a)
./build/bin/adaptive_autosar \
  ./configuration/machine_a/execution_manifest.arxml \
  ./configuration/machine_a/extended_vehicle_manifest.arxml \
  ./configuration/machine_a/diagnostic_manager_manifest.arxml \
  ./configuration/machine_a/health_monitoring_manifest.arxml \
  ./configuration/machine_a/watchdog_manifest.arxml

# Two-machine demo (simulates Machine A + Machine B communicating via SOME/IP)
./run_demo.sh
```

## Architecture Overview

This is a C++14 educational implementation of the [AUTOSAR Adaptive Platform](https://www.autosar.org/standards/adaptive-platform) standard.

### Layer Structure

```
application/platform/   ← EM (main entry point) + SM, PHM, DM — each has its own *_main.cpp binary
application/             ← ExtendedVehicle (EV) and WatchdogApplication (WA) — also separate binaries
application/doip/        ← DoIP automotive diagnostic protocol
ara/                     ← ARA (AUTOSAR Runtime for Adaptive Applications) interfaces
  core/                  ← Result<T>, Optional<T>, ErrorCode, ErrorDomain
  log/                   ← Logging framework (console/file sinks)
  com/                   ← SOME/IP middleware (RPC, Pub/Sub, Service Discovery, E2E)
  exec/                  ← Execution management, ModelledProcess base class
  sm/                    ← State management (function group states)
  diag/                  ← UDS diagnostics with routing and debouncing
  phm/                   ← Platform Health Management (supervisor watchdogs)
arxml/                   ← ARXML manifest file parsing (pugixml-based)
src-gen/vehicle_status/  ← Hand-generated Proxy/Skeleton for VehicleStatus service (ara::com layer)
```

### Key Design Patterns

- **`Result<T>`** (in `ara/core/result.h`): Rust-style error handling used throughout instead of exceptions. Functions return `Result<T>` wrapping a value or an `ErrorCode`.
- **`ModelledProcess`**: Base class for all platform components (`execution_management`, `state_management`, etc.), providing a lifecycle model.
- **Finite State Machines**: Generic `FiniteStateMachine<T>` template in `ara/com/helper/`. Used in Service Discovery (`com/someip/sd/fsm/`) and Pub/Sub (`com/someip/pubsub/fsm/`).
- **`NetworkLayer<T>`**: Template abstraction for protocol-agnostic networking with `NetworkLayerObserver` callbacks.
- **Async I/O**: All network I/O goes through `AsyncBsdSocketLib::Poller` (external dependency). The main loop polls this alongside platform services.

### Communication Stack (`ara/com/someip/`)

- `rpc/` — RpcServer and RpcClient for request/response messaging (also used by ExecutionClient to report state back to EM)
- `sd/` — Service Discovery with FSM-controlled server/client agents
- `pubsub/` — Event notification; publisher and subscriber agents with FSM
- `e2e/` — End-to-End protection (Profile11) for data integrity

Applications use the `src-gen/` Proxy/Skeleton layer rather than SOME/IP directly. `VehicleStatusSkeleton` (used by EV) wraps the SD server + PubSub server; `VehicleStatusProxy` (used by WA) wraps the SD client + PubSub receiver.

### Configuration

Platform behavior is configured via ARXML manifests in `configuration/`. The `arxml/` library parses these at startup to configure function groups, services, and health supervision parameters.

## Code Conventions

From [CONTRIBUTING.md](CONTRIBUTING.md):

- **Braces:** Allman style (opening brace on its own line)
- **Line length:** 80-character soft limit
- **Classes/structs/public members:** `PascalCase`
- **Namespaces:** `lowercase`
- **File names:** snake_case derived from class name (`MyClass` → `my_class.h`)
- **Private members:** `mXxx` prefix
- **Local variables:** `_xxx` prefix
- **Static constants:** `cXxx` prefix
- **Use `auto`** sparingly — only for iterators or when the type is explicitly visible
- **Prefer `using`** over `typedef`

Commit messages optionally use emoji prefixes: `:beer:` (feature), `:microscope:` (test), `:syringe:` (bugfix), `:shower:` (refactor), `:scroll:` (docs).

## AUTOSAR AP Development Step Workflow

This project follows the AUTOSAR Adaptive Platform Methodology
([AUTOSAR_AP_TR_Methodology.pdf](doc/autosar_specs/AUTOSAR_AP_TR_Methodology.pdf)).
When introducing a new application or feature, work through these steps in order:

### 1. Service Interface Design (§3.8)
Create `configuration/<name>_service_interface.arxml`:
- Define data types (`DATA-CONSTR`, `IMPLEMENTATION-DATA-TYPE`)
- Define the `SERVICE-INTERFACE` with events, methods, and fields
- Define the SOME/IP binding deployment (`SOMEIP-SERVICE-INTERFACE-DEPLOYMENT`)

Reference: [extended_vehicle_service_interface.arxml](configuration/extended_vehicle_service_interface.arxml)

### 2. SW Component Design (§3.3.2.5)
Create `configuration/sw_component_design.arxml`:
- Define `ADAPTIVE-APPLICATION-SW-COMPONENT-TYPE` for application-level components only
  (exclude platform functional clusters: EM, SM, PHM, DM)
- Declare ports: `P-PORT-PROTOTYPE` (provided) and `R-PORT-PROTOTYPE` (required)
- Reference the service interfaces defined in step 1

Reference: [sw_component_design.arxml](configuration/sw_component_design.arxml)

### 3. Executable + SW Composition + Process Design (§3.3.2.4/6/3)
Create `configuration/executable_description.arxml`:
- `COMPOSITION-SW-COMPONENT-TYPE`: lists application SWC prototypes (no platform FCs)
- `EXECUTABLE`: references the composition, sets timer granularity
- `PROCESS-DESIGN`: links executable to machine function group state (e.g., MachineFG::StartUp)

Reference: [executable_description.arxml](configuration/executable_description.arxml)

### 4. SW Interaction Description (§3.3.2.7)
Create `configuration/sw_interaction_description.arxml`:
- Document connectors for each port (SD, DoIP, pub/sub, PHM supervised entity)
- Note implementation gaps (planned vs. implemented interactions)
- Document API-based FC interactions (ara::log, ara::exec, ara::diag) — no ports

Reference: [sw_interaction_description.arxml](configuration/sw_interaction_description.arxml)

### 5. Implementation (§3.4)
- Implement the `ModelledProcess` subclass under `src/application/`
- `main()` acts as composition root: construct all objects, call `Register()`, then `Initialize()`
- Platform processes (SM exception aside) must not be coupled inside ExecutionManagement;
  they are constructed in `main()` and registered via `ExecutionManagement::Register()`

Reference: [src/main.cpp](src/main.cpp), [src/application/platform/execution_management.h](src/application/platform/execution_management.h)

### 6. Integration Manifests (§3.10)
Create or update manifest files under `configuration/`:
- `execution_manifest.arxml` — RPC port, function group states
- `<name>_manifest.arxml` — service instance bindings (SOME/IP, DoIP, PHM)
- `health_monitoring_manifest.arxml` — supervised entities and supervision parameters

### 7. Build & Test
```bash
cmake --build build
cd build && ctest
./build/bin/adaptive_autosar \
  ./configuration/machine_a/execution_manifest.arxml \
  ./configuration/machine_a/extended_vehicle_manifest.arxml \
  ./configuration/machine_a/diagnostic_manager_manifest.arxml \
  ./configuration/machine_a/health_monitoring_manifest.arxml \
  ./configuration/machine_a/watchdog_manifest.arxml
```

Note: 4 async BSD socket tests (`TcpCommunicationTest`, `PollerTest`) fail due to
environment-dependent port availability — these are pre-existing and unrelated to
application logic.
