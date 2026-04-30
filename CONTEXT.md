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
  d. Machine A and Machine B deployed in Linux Server. Machine C deployed in Arduino UNOQ. Ethernet connection is through usb (this is called USB Ethernet gadget or RNDIS/CDC-ECM).  UNOQ IP address is 192.168.2.96, Linux Server IP is 192.168.2.94
  e. UNOQ's debian build does not come with usb gadget which is needed for ethernet connection between Machine C and Linux Server.  For not this script needs to be run manually after a reboot 
     sudo /usr/local/bin/setup-rndis.sh
  f. I have seen occassionally the IP dropped in the Linux server. These commands fix the problem if ever happens 
      sudo systemctl daemon-reload
      sudo systemctl restart systemd-networkd

### 1.5 Common commands to remember 
Unpacking in linux: tar xzf adaptive-autosar-aarch64.tar.gz
Sniffing ethernet traffic in Linux: sudo tcpdump -i any -n host 192.168.2.96 
Look for hung process in Linux: ps aux | grep "optional but useful"
Then kill a hung process(PID):  kill -9 PID 
Copy a folder and all its contents with ssh connection in powershell: 
   scp -rf sourceuser@sourceIP destinationuser@destinationIP 



## 2. Stories 
a. aarch64 deployment package includes dynamic libraries. In a real Adaptive Autosar workflow I believe these libraries are part of the machine configuration and/or part of the OS package. I need to understad further the related AUTOSAR Workflow and likley the OS configuration. For embedded Linux this brings me to try the Yocto project. 

b. Now that we have two physical hosts, communication is fragile as it depends on manually starting the adaptive platform in both hosts.  This suggest the network management is likley the next functional objective.

c. In a real Adaptive Autosar workflow, deployment means to transfer/download executables and manifests. The AP should take care of installing and executing them.  Assuming this is close to the OTA concept of classic autosar, it includes diagnostic sessions and diagnostic routines for transfer. But it also requires the AA Configuration Manager Functional cluster.  

d. FC Persistency is partially implemented. System Design needs to be matured 

Documentation (./doc/autosar_specs/)
AUTOSAR_AP_EXP_SWArchitecture: Sections 9.4.1, 10.3.1 and 11.3.1.  Do not forget to review section 8 for context. 
AUTOSAR_AP_EXP_PlatformDesign: Section 10
AUTOSAR_AP_SWS_Persistency: 

e. How a new application is added to a machine. 
1. AUTOSAR_AP_EXP_SWArchitecture,
9.7 Overview and interfaces of the main entities inlvolved in deploying SW Packages. UCM, VUCM and Registry. 
10.6 The basic use cases seems to be "Transfer SW Package",  "Process SW Package" and "Activate SW Package". VUCM seems to be the orchestrateor , and UCM the executor.
12 Deployment example. highlights that SW Packages can be Application Package, Platform Package and Platform Core Package. 
13.1 Overview of UCM, Package, SW Cluster and SW Cluster contents. Although is not very clear, seemt this describes mainly Platform packages. 
2. AUTOSAR_AP_EXP_PlatformDesign,
13.2.1 - Explains that data transfer is done over ara::com. 
13.3.1 - Is described as non-normative, but provides a good example of how the SW Package is created
13.4 - This basically gives more information on the workflow for Process SW Package and Activare SW package uses cases. 
3. AUTOSAR_AP_TR_Methodology, 
2.9 Software Cluster Design - This is the set of artifacts that will be deployed. UCM is knowledgeable of the type of artifacts allowed and how each type is deployed.
--2.10 SW Cluster Integration - Apparently the assembling a SW Cluster deserves as detailed workflow
--2.11 SW Packaging - This is the artifact that the UCM will consume. This seems to be basically the envelope of the SW Cluster. 
4. Use case Transfer SW Package seems to be the most fundamental use case. Basically machine A will transfer a SW Package to Machine C.  The simplest SW Package seems to be an update to an existing functional cluster.  How about adding Time Synchronization ? 
6. Because of the complexity of transfering a whole application , most likely we need to take care of implementing a more robust communication manager.  in the spec it is described as a daemon too.
plan: zazzy-humming-bentley.md
Steps 0-11 complete (Phase 1). Phase 2 (full binary transfer) complete. Step 12 (CI/aarch64 tarball update) pending.

### Phase 2 UCM/VUCM — full binary transfer (implemented)
- VUCM reads SW package from disk with stat(), streams in 64 KB chunks via TransferData RPCs.
  Basename (without path and .swpkg.tar.gz suffix) is sent as PackageInfoType::name.
- UCM PackageManager TransferExit Phase 2 path stores as <name>.swpkg.tar.gz in FileStorage.
- PackageManager::Reset() (public) resets state to kIdle; called by skeleton on client disconnect.
- SocketRpcServer::SetDisconnectHandler() added; fires when recv() returns ≤ 0.
- PackageManagementSkeleton registers disconnect handler → Reset() for UCM self-healing.
- 5 new unit tests in test/ara/ucm/package_manager_phase2_test.cpp; 391/396 pass total.




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
10. time_synchronization is a platform component. 
11. review /design arxmls for duplicities and inconsistencies
12. VUCM, UCM phase2 real inter machine transfer — DONE (loopback). Cross-machine (Machine A → Machine C on real Ethernet) is next.
13. fix or remove carried over failing unit tests. I think they have to do with either NM or the OBD-Vechicle Simulator network.  
14. We probably want to create a machine manifest to consolidate filesystem information such as /var/per , /tmp/per, tmp/ucm_b/ etc
15. We probably want to use SIGUSR1 to trigger VUCM/UCM Transfer and Activate scenarios
16. Machine A and Machine C still need UCM imo, so both become updatable. 
17. we pobably want to reorganize the folder sturcture to match the deployment view. 
18. create aarch64's time_sync_placeholder-1.0.0.swpkg.tar.gz
19. 

## 4. Implementation notes and limitations per functional cluster
### Persistency / ara::per
Storage files are written to /var/per/ (created on first run). If mkdir returns anything other than EEXIST (i.e., permission denied), they fall back to /tmp/per/

### Watchdog Application 
onEventReceived callback persists directly (not thread-safe vs main loop). This is good as long as system design ensures single writer

## 5. Repotitory reorganization 

-design
--Platform
--Application
---ExtendedVehicle
---WatchdogApplication
-src
--ara
--Platform status
--Platform Core
--Application
-src-gen
-cmake
-deployment
--manifests
---machine_a
---machine_b
---machine_c
--build
--build-aarch64
--sw_packages
---adaptive-autosar-aarch64.tar.gz
---time_sync_placeholder-1.0.0.swpkg.tar.gz
-unit-test
--Platform
--Application


