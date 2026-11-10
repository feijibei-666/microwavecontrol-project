# queue.c 来源与用途

官方 FreeRTOS Kernel V10.5.1 `queue.c`，来自 commit `def7d2df2b0506d3d249334974f51e427c17a41c`，未修改，MIT 许可。

实现任务间按键事件与显示状态快照队列。工程只使用静态队列，不使用动态分配；互斥量、计数信号量和队列集合关闭。来源记录见同目录 `SOURCE.md`。
