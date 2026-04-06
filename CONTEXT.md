# Context
In order to create and deploy adaptive applications in a way as close as possible to a commercial
environment we need to expand capabilities of the Adaptive Platform (AP). In the big picture this
means also to review APIs and Artifacts to be more complete and consistent with the AUTOSAR SWSs. 
It also means that the AP should be deployable in an embedded target. 


## Build and deploy in Arduino UNO Q (aarch64-linux-gnu-gcc 14.2.0 (Debian)) a.k.a Machine C

Configure command:
```bash
  cmake -DCMAKE_BUILD_TYPE=Debug\
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake \
  -Dbuild_tests=OFF \
  -S . -B build-aarch64
``` 
Build command 
```bash
  cmake --build build-aarch64
```
Create deployment package 
```bash
  cmake --install build-aarch64 --prefix deploy/
```
Copy files to ArduinoUNOQ 
deploy\*
configuration\machine_c\*

Note that deploy package has been created with execution permissions. use scp -p flag to copy permisions OR run chmod after the copy to permit execution 
Note that configuration\machine_c\execution_manifest.arxml stablish relative paths to all binaries and configuration files. Make sure to honor the folder structure when copying



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

4. CMakeLists.txt is now patching the fetched `async-bsd-socket-lib` dependency (`fifo_sender.h`, `fifo_receiver.h`).
   This is needed for build-aarch64. We will need to figure out if this library should be replaced according to AUTOSAR OS Interface specs. 
   Otherwise we probably want to fix the changes in the async-bsd-socket-lib repo.  

5. We need to bring up github actions to have both builds clean.  

6. after deployment in UNOQ is clean. We will have to try and bugfix cross machine communication , IP, ports etc. Also figure out bus monitoring and functional test.  At this point we probably will need to deal with Network management. Here is where extending AP capabilities will become a topic again.  


