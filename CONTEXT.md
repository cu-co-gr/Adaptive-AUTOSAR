# Context
In order to create and deploy adaptive applications in a way as close as possible to a commercial
environment we need to expand capabilities of the Adaptive Platform (AP). In the big picture this
means also to review APIs and Artifacts to be more complete and consistent with the AUTOSAR SWSs. 

This file extends README.md providing further details, known limitations and TODOs.  Since my Linux experience is limited some topics might be too detailed.

## 1. Annotations about MY development environment.
  a. Visual Studio Code hosted in windows. repository and build hosted in Linux Server. SHH connection.
  b. I am using Arduino UNOQ only for hosting and testing machine C. 
  c. I was not able to set SSH connection between Linux server and Arduino UNOQ for file transfer.  This means that if I need to deploy a local build I have to do Linux server -> Windows -> UnoQ (Linux). For now this means I need to run the following command in UNOQ after the copy to allow execution
    chmod +x ./build/bin/*  
  d. Machine A and Machine B deployed in Linux Server. Machine C deployed in Arduino UNOQ. Ethernet connection is through usb (this is called USB Ethernet gadget or RNDIS/CDC-ECM).

### 1.5 Common commands to remember 
Unpacking in linux: tar xzf adaptive-autosar-aarch64.tar.gz
Sniffing ethernet traffic in Linux: sudo tcpdump -i any -n host 192.168.2.96 
Look for hung process in Linux: ps aux | grep "optional but useful"
Then kill a hung process(PID):  kill -9 PID 
Copy a folder and all its contents with ssh connection in powershell: scp -rf sourceuser@sourceIP destinationuser@destinationIP 


## 2. Stories 
a. aarch64 deployment package includes dynamic libraries. In a real Adaptive Autosar workflow I believe these libraries are part of the machine configuration and/or part of the OS package. I need to understad further the related AUTOSAR Workflow and likley the OS configuration. For embedded Linux this brings me to try the Yocto project. 

b. Now that we have two physical hosts, communication is fragile as it depends on manually starting the adaptive platform in both hosts.  This suggest the network management is likley the next functional objective.

c. In a real Adaptive Autosar workflow, deployment means to transfer/download executables and manifests. The AP should take care of installing and executing them.  Assuming this is close to the OTA concept of classic autosar, it includes diagnostic sessions and diagnostic routines for transfer. But it also requires the AA Configuration Manager Functional cluster.  

d. FC Persistency is partially implemented. System Design needs to be matured 

Documentation (./doc/autosar_specs/)
AUTOSAR_AP_EXP_SWArchitecture: Sections 9.4.1, 10.3.1 and 11.3.1.  Do not forget to review section 8 for context. 
AUTOSAR_AP_EXP_PlatformDesign: Section 10
AUTOSAR_AP_SWS_Persistency: 



## 3. TODOS

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

4. CMakeLists.txt is now patching the fetched `async-bsd-socket-lib` dependency (`fifo_sender.h`, `fifo_receiver.h`).
   This is needed for build-aarch64. We will need to figure out if this library should be replaced according to AUTOSAR OS Interface specs. 
   Otherwise we probably want to fix the changes in the async-bsd-socket-lib repo.   

7. test_watchdog_event seems to need cleanup.  the test passes based on the log which suggest that communication side is ok.  Yet the wire analysis fails. Assumption is this is purely test / observation problem and not functional issue.  

8. deployment package for x86_64 might be useful. it will allow test artifacts from CI builds.

9. ara::per Persistency has been implemented as a library (ara_per). Storage files are written to
   /var/per/ (created on first run). If mkdir returns anything other than EEXIST (i.e., permission denied), they fall back to /tmp/per/EV writes to extended_vehicle.json; WA writes to watchdog_<instanceId>.json. 
   Known limitation:
   - WatchdogApplication: onEventReceived callback persists directly (not thread-safe vs main loop).
     Single-writer assumption from Story d keeps this benign for current deployment.

## 4. Implementation notes and limitations per functional cluster
### Persistency / ara::per
Storage files are written to /var/per/ (created on first run). If mkdir returns anything other than EEXIST (i.e., permission denied), they fall back to /tmp/per/

### Watchdog Application 
onEventReceived callback persists directly (not thread-safe vs main loop). This is good as long as system design ensures single writer




