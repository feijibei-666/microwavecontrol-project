# main.c 启动入口说明

启动顺序为：

1. `LED_Init()`：PC13 低电平有效 LED 立即点亮。
2. `NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4)`：显式配置四位抢占优先级，与 FreeRTOS 中断优先级设置一致。
3. `delay_init()`：更新系统时钟及微秒时间基准，不抢占 FreeRTOS 的 SysTick。
4. `KEY_Init()`：PB12、PB1 下拉输入，高电平按下。
5. `AppRTOS_Start()`：初始化业务状态、创建静态队列和三任务，调用 `vTaskStartScheduler()`。

实际调度基于官方 FreeRTOS 内核和 ARMCC5 Cortex-M3 移植层；业务已从裸机主循环迁移到 `app.c`、`app_rtos.c`，UI 位于 `ui.c`。

欢迎页持续等待 KEY1 短按，不通过延时自动跳转。门开 LED 亮、门关 LED 灭。当前只接入 LED、屏幕、按键，未启动加热、电机或门锁驱动。

每个新增或修改的 C 文件都有同目录同名 Markdown 说明，内核文件另注明官方来源和本项目配置。
