# main.c 启动入口说明

启动顺序为：

1. `LED_Init()`：PC13 低电平有效 LED 立即点亮。
2. `Heater_Init()`、`Buzzer_Init()`：PA2、PB15 先预装低电平再配置输出，继电器关闭、蜂鸣器静音。复位到本步骤之前仍须靠外部硬件下拉保证关闭。
3. `NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4)`：显式配置四位抢占优先级，与 FreeRTOS 中断优先级设置一致。
4. `delay_init()`：更新系统时钟及微秒时间基准，不抢占 FreeRTOS 的 SysTick。
5. `KEY_Init()`：PB12、PB1 下拉输入，高电平按下。
6. `Outputs_Init()`：启动 PB0 门舵机及 PA1 转盘 PWM，初始开门、转盘停止；PA2 保持关闭。
7. `AppRTOS_Start()`：初始化业务状态、创建静态队列和四个应用任务，调用 `vTaskStartScheduler()`。

实际调度基于官方 FreeRTOS 内核和 ARMCC5 Cortex-M3 移植层；业务已从裸机主循环迁移到 `app.c`、`app_rtos.c`，UI 位于 `ui.c`。

欢迎页持续等待 KEY1 短按，不通过延时自动跳转。门开 LED 亮、门关 LED 灭。双舵机和有源蜂鸣器已接入；本阶段所有加热调用都保持 PA2 低电平，不接 PTC 进行测试。

每个新增或修改的 C 文件都有同目录同名 Markdown 说明，内核文件另注明官方来源和本项目配置。
