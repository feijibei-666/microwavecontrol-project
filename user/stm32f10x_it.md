# stm32f10x_it.c 中断说明

保留 STM32 标准外设库的异常处理模板。HardFault、MemManage、BusFault、UsageFault 仍进入模板中的死循环；NMI 和 DebugMon 保留原处理。

`SVC_Handler`、`PendSV_Handler` 和 `SysTick_Handler` 由官方 FreeRTOS RVDS Cortex-M3 移植层提供，通过 `FreeRTOSConfig.h` 中的宏直接对应启动文件向量。本文件不再定义同名函数，避免覆盖调度器入口或重复定义。

FreeRTOS 的 1 kHz Tick 触发 `vApplicationTickHook()`；钩子位于 `rtos_hooks.c`，只调用 `KEY_Tick1ms()` 独立采样两个按键。中断中不刷新 LCD，不等待松开，不运行应用状态机，也不调用阻塞延时。

当前没有启用 EXTI 按键中断。后续新增外设中断时，若调用 `FromISR` API，NVIC 抢占优先级必须满足 `configMAX_SYSCALL_INTERRUPT_PRIORITY` 的限制。
