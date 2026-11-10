# key.c 按键驱动说明

## 硬件

- KEY1：OUT 接 PB12；KEY2：OUT 接 PB1。
- 两个三脚模块均使用支持的 3.3V 电源，VCC 接 3V3，GND 共地。
- 按下输出高电平，释放输出低电平；GPIO 配置为下拉输入。

## 事件识别

FreeRTOS 独占 `SysTick_Handler()`，其 `vApplicationTickHook()` 每 1 ms 调用 `KEY_Tick1ms()`。两个按键分别维护消抖、按住时长和事件队列，不使用等待松开的循环。输入任务不重复采样 GPIO，只消费事件。

- 输入连续稳定约 20 ms 后确认状态变化。
- 短按在消抖后的释放沿产生一次 `KEY_SHORT`。
- 稳定按下约 1000 ms 后产生一次 `KEY_LONG`，持续按住不连发，释放时不再生成短按。
- 上电或清空事件时仍按住的键，必须先释放，才能重新产生有效事件。
- 每键使用 8 槽环形队列，最多保存 7 个待处理事件；队列满时丢弃新事件。`InputTask` 每 1 ms 读取，屏幕刷新期间由 Tick hook 继续采样。
- 输入任务将两个键的一组事件写入长度为 12 的静态 FreeRTOS 队列，控制任务消费并处理业务；同组 KEY1 优先。跨任务队列满时记录诊断并调用 `RTOS_Fatal(4)`。

## 接口

- `KEY_Init()`：配置 GPIO 并初始化状态；在开启界面操作前调用一次。
- `KEY_Tick1ms()`：仅由 1 ms FreeRTOS Tick hook 调用，不在任务中重复调用。
- `KEY1_Scan()` / `KEY2_Scan()`：立即返回一个待处理事件，无事件则返回 `KEY_NONE`。
- `KEY_ClearEvents()`：显示任务在 LCD 初始化完成、开放输入前调用，丢弃启动阶段操作；持续按住的键须释放后重按。
- `KEY1_Pressed()`：兼容入口，消费 KEY1 事件并判断是否短按；不要与 `KEY1_Scan()` 在同一轮重复消费。

消费事件和清空队列时只短暂关闭中断，并恢复原有 PRIMASK 状态。Tick hook 中不调用 LCD、延时或业务处理；业务由控制任务独占处理。初始化顺序为先 `KEY_Init()`，再启动 FreeRTOS 调度器。
