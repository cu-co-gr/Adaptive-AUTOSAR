# =============================================================================
# CMake Toolchain File for ARM64 (aarch64) Cross-Compilation
# Target: ARM Cortex-A53 (Arduino UNO Q, Raspberry Pi, automotive HPCs)
# Host:   x86_64 Linux (development server)
# Usage:  cmake -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake ...
# =============================================================================

# --- Target System -----------------------------------------------------------
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# --- Cross Compiler ----------------------------------------------------------
# Installed via: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
set(CMAKE_C_COMPILER   /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)
set(CMAKE_AR           /usr/bin/aarch64-linux-gnu-ar)
set(CMAKE_RANLIB       /usr/bin/aarch64-linux-gnu-ranlib)
set(CMAKE_STRIP        /usr/bin/aarch64-linux-gnu-strip)

# --- Sysroot (optional, uncomment if you have a target sysroot) -------------
# A sysroot is a copy of the target filesystem containing libraries/headers.
# Without one, the cross-compiler uses its own bundled sysroot which is
# sufficient for this project's dependencies.
#
# set(CMAKE_SYSROOT /path/to/aarch64-sysroot)
# set(CMAKE_FIND_ROOT_PATH /path/to/aarch64-sysroot)

# --- Search Path Behavior ----------------------------------------------------
# Tell CMake to look for programs on the HOST (build machine),
# but look for libraries and headers on the TARGET.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# --- Compiler Flags ----------------------------------------------------------
# -march=armv8-a    : ARMv8-A architecture (Cortex-A53 baseline)
# -mtune=cortex-a53 : Optimize for Cortex-A53 specifically (UNO Q / RPi)
set(CMAKE_C_FLAGS_INIT   "-march=armv8-a -mtune=cortex-a53")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a -mtune=cortex-a53")

# --- OpenSSL cross-compile hint ----------------------------------------------
# When cross-compiling, CMake may not find the ARM version of OpenSSL.
set(OPENSSL_ROOT_DIR /usr/lib/aarch64-linux-gnu)
set(OPENSSL_INCLUDE_DIR /usr/include)
set(OPENSSL_CRYPTO_LIBRARY /usr/lib/aarch64-linux-gnu/libcrypto.so)
set(OPENSSL_SSL_LIBRARY /usr/lib/aarch64-linux-gnu/libssl.so)

# --- Skip gtest build on cross compile (optional) -----------------------------------------------
set(CMAKE_CROSSCOMPILING TRUE)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

# Disable test execution in fetched dependencies during cross-compilation
set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
