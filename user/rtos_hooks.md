# rtos_hooks.c 内核钩子说明

## 静态内存与采样

`vApplicationGetIdleTaskMemory()` 提供空闲任务的静态 TCB 和 128 字栈，满足关闭动态内存分配后的内核要求。业务任务与队列的静态存储由应用 RTOS 层提供。

`vApplicationTickHook()` 每 1 ms 调用 `KEY_Tick1ms()`。按键采样工作量固定，不调用阻塞 API，不操作 LCD；输入任务随后取出按键事件。`KEY_Init()` 应在调度器启动前完成。

## 故障定位

`RTOS_Fatal(code)` 关闭中断，先调用 `Outputs_EmergencyStop()` 关闭 PA2 并停止 PA1 转盘，再调用 `Buzzer_Stop()` 静音，随后记录故障码、点亮 PC13 LED 后停留，便于调试器观察。安全关闭不调用 RTOS API 或延时，即使外设只完成部分初始化也能执行。故障时不移动门舵机。`LED_Init()` 应在可能进入该函数之前完成。

| 调试变量 / 故障码 | 含义 |
| --- | --- |
| `rtos_fault_code` | 最近一次致命故障码；初始值为零 |
| `rtos_fault_line` | `configASSERT` 触发位置行号 |
| `rtos_fault_file` | 断言源码路径，或发生栈溢出的任务名 |
| `0xA001` | `configASSERT` 失败 |
| `0xA002` | 内核检测到任务栈溢出 |
| `0xB001` | HardFault |
| `0xB002` | MemManage |
| `0xB003` | BusFault |
| `0xB004` | UsageFault |

`RTOS_Assert()` 对接 `configASSERT`；`vApplicationStackOverflowHook()` 对接模式 2 栈检查。应用可使用其他故障码调用 `RTOS_Fatal()`，但本模块不复位硬件、不自动重启任务。
