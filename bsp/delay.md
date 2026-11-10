# delay.c 时间基准说明

## 配置

`delay_init()` 调用 `SystemCoreClockUpdate()`，启用 Cortex-M3 的 DWT 周期计数器。正常使用 8 MHz 外部晶振、72 MHz 系统时钟；应在初始化 LCD 之前调用。

SysTick 由官方 FreeRTOS Cortex-M3 移植层统一配置并处理，Tick 频率为 1 kHz。本文件不读写 SysTick 的 LOAD、VAL、CTRL，也不再提供独立的 `delay_tick()`。

`delay_millis()` 返回内核 Tick 换算的毫秒数。采用 32 位无符号减法计算经过时间，可跨越约 49.7 天的计数回绕；调度器启动前其值保持为零。

## 延时接口

- `delay_ms()`：调度器运行后调用 `vTaskDelay()` 阻塞当前任务并让出 CPU；增加一 Tick 防止在 Tick 边缘调用时提前返回。调度器启动前使用 DWT 等待，供 LCD 上电初始化使用。
- `delay_us()`：轮询 DWT 周期计数器，以不超过 1000 μs 的小段等待，不关闭中断。任务抢占或中断可能延长实际等待，不适合产生精密波形。
- `delay_ms(0)` 立即返回；非零毫秒延时禁止在中断中调用，违反时进入 `configASSERT` 故障处理。

业务倒计时应通过时间差推进；任务周期使用 `xTaskDelayUntil()`，避免将固定阻塞延时累计为周期误差。
