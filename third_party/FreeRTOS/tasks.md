# tasks.c 来源与用途

官方 FreeRTOS Kernel V10.5.1 `tasks.c`，来自 commit `def7d2df2b0506d3d249334974f51e427c17a41c`，未修改，MIT 许可。

实现任务创建、优先级调度、延时、Tick 和栈余量查询。工程采用静态任务创建，关闭动态分配、Tickless 和软件定时器。配置见 `user/FreeRTOSConfig.h`，来源记录见同目录 `SOURCE.md`。
