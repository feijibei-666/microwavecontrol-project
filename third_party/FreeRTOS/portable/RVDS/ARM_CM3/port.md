# port.c 来源与用途

官方 FreeRTOS Kernel V10.5.1 `portable/RVDS/ARM_CM3/port.c`，来自 commit `def7d2df2b0506d3d249334974f51e427c17a41c`，未修改，MIT 许可。

适用于 ARM Compiler 5 / RVDS 的 Cortex-M3。保存恢复任务上下文，启动首个任务，配置 SysTick，并实现 SVC、PendSV、SysTick 中断。

`user/FreeRTOSConfig.h` 将移植层函数名映射为启动向量表所需的 `SVC_Handler`、`PendSV_Handler`、`SysTick_Handler`。`user/stm32f10x_it.c` 不再定义这些函数，避免重复链接或中断落入空实现。

优先级使用 STM32F103 的 4 位优先级，内核为 15，可调用 RTOS FromISR API 的中断优先级数值不得小于 5。当前按键 Tick hook 不调用 RTOS API，且初期关闭 Tickless。
