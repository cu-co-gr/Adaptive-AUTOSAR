# Context 
In order to create and deploy adaptive applications in a way as close as possible to a comercial environment we need to expand capabilities of the Adaptive Platform(AP). In the big picture this means also to review APIs and Artifacts to be  more complete and consistent with the AUTOSAR SWSs. 
The first architectural gap addressed was to split the executables. In such a way that Execution manager indeed takes responsibility to start and execute them.
Initial conclusion was that state manager did not require major work to be consistent with SWSs as it provides the interfaces sufficient to orchestrate the state change and reporting once execution manager hands over.

# TODOS
1. Confirm if all processes are terminated correctly.  This relates to the observability issues saved in the MEMORY. 
2. After the ssh session crashed. I reconnected and then ran run_demo.sh. This time I was able to see logs from both [A] and[B] . The second time the demo seemed stuck after initialization. Likely crashed. The thir time I stopped seein the [A] logs after some seconds. This is related to the observability issue saved in MEMORY. But this suggest that there might be dangling processes.

