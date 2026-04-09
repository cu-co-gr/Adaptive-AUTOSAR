# Adaptive-AUTOSAR

This project is a playground for Adaptive Autosar Methodology use cases. At the same time this serves as playground to test development workflows with AI Agents, aiming to understand its capabilities and limitations to develop safety critical systems. 

System Concept - Two or more Machines interconnected over SOME/IP
Machine Concept - Adaptive Platform with one or more Adaptive Applications deployed in one of the following: 
    a. Virtual Target - Linux Server (Ubuntu 24.04.4, Intel/AMD 64-bit, x86_64)
    b. Embedded Target - Arduino UNOQ (Debian GNU/Linux 13 (trixie), ARM Cortex-A53, aarch64)
Adaptive Platform Concept - Baselined from upstream repository.  Extended to match Autosar R25-11 specifications. 

Major differences with baseline
    Execution Manager - Reworked to spawn/start and terminate OS processes.
    State Manager, Platfrom Health Manager, Diagnostic Manager are now independent executables
    Volvo Extended Vehicle API is stubbed
    Added build toolchain for aarch64
    ara::comm - Introduced skeleton/proxy pattern.
    extended_vehicle / watchdog_application - Basic applications to enable system concept and machine concept.

## Dependecies

It will be tried to use minimum number of dependencies as much as possible. The current dependencies are as follows:

- Cpp Standard: 14
- Cmake mimimum version: 3.14
- Compiler:
    - GCC C/C++ Compiler (x86-64 Linux GNU): 11.2.0
    - GCC C/C++ Compiler (aarch64 Linux GNU): 14
- Google Test: v1.12.1
- [pugixml 1.13](https://pugixml.org) (3rd party C++ libary)
- [libcurl 7.88.0](https://github.com/curl/curl) (3rd party C libary)
- [JsonCpp 1.9.5](https://github.com/open-source-parsers/jsoncpp) (3rd party C++ library)
- [Async BSD Socket Lib](https://github.com/langroodi/Async-BSD-Socket-Lib) (in-house C++ libary)
- [OBD-II Emulator](https://github.com/langroodi/OBD-II-Emulator) (in-house C++ emulator)
- [DoIP Lib](https://github.com/langroodi/DoIP-Lib) (in-house C++ libary)

## Build
Github Actions builds both x86_64 and aarch64 targets.

### Compiler debug configuration

- GCC x86_64:
```bash
cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/x86_64-linux-gnu-gcc-11 -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/x86_64-linux-gnu-g++-11 -S . -B build
```
- GCC aarch64:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake -Dbuild_tests=OFF -S . -B build-aarch64
```

### Compiling
```bash
cmake --build build
```

### Create deployment package aarch64
This was created to delpoy the application in Arduino UNOQ.
```bash
  cmake --install build-aarch64 --prefix deploy/
```

### Unit tests running (built only for x86_64 target)
```bash
cd build && ctest
```

## Run
To run the Adaptive AUTOSAR simulation, launch the following executable by passing the execution manifest file path as the argument:
```bash
./build/bin/adaptive_autosar ./configuration/execution_manifest.arxml
```
Note that execution_manifest.arxml contains the relative paths to the rest of the executables and manifests.

# Smoke Test
./scripts/test_watchdog_event.sh checks for basic machine execution and system communication.  

## Documentation

1. Please refer to upstream project.  [the project GitHub pages](https://langroodi.github.io/Adaptive-AUTOSAR/) powered by Doxygen.
2. Please refer to CONTEXT.md for user stories, knowm limitations and TODOs. 

