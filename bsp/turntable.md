# turntable.c 转盘舵机驱动

- 信号接 PA1/TIM2_CH2，外部 5V 供电并与 STM32 共地。
- TIM2 在本阶段仅服务转盘；继电器 PA2 改为普通 GPIO，不再重配或共享 TIM2 时基。
- PWM 为 50 Hz，初始及停止脉宽为 `TURNTABLE_PULSE_STOP`（1500 μs）；运行脉宽为 `TURNTABLE_PULSE_RUN`（1000 μs）。连续旋转舵机的停止点需实机校准。
- 初始化先将 PA1 保持低电平，装载停止脉宽后才切换为复用输出，避免首帧使用未初始化参数。
- `Turntable_Start()`、`Turntable_Stop()` 只更新预装载比较值，不重置周期；命令最迟在下一 PWM 帧边界生效。
- `Turntable_EmergencyStop()` 无 RTOS 调用、无延时。驱动已就绪时写入中位停止脉宽；未初始化或半初始化时将 PA1 改为低电平 GPIO，避免启动未知 PWM。

故障前已经初始化的定时器在 CPU 停止任务调度后仍继续产生停止脉冲。没有有效 PWM 时舵机行为取决于器件，不能把断开信号视为经认证的机械制动。
