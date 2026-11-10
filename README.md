# STM32 微波炉控制演示项目

基于 **STM32F103C8（Cortex-M3）**、**STM32F10x 标准外设库** 和官方 **FreeRTOS Kernel V10.5.1** 的微波炉控制演示程序。输入、控制和显示由三个静态任务分别处理，任务通过队列传递按键事件和界面快照。两个按键用于软件门状态、分层菜单、时间设置、功率选择及倒计时暂停恢复，ST7735 TFT 显示当前页面，PC13 LED 演示开门照明。仓库保留门锁舵机、转盘和加热 PWM 驱动，本阶段不初始化或运行这些执行器。

界面采用深色背景、青蓝圆角卡片、大数字时间、三段功率选择和运行进度条。普通页面切换使用约 200 ms 分区擦入和方向提示；暂停与完成取消装饰动画，优先刷新状态。上电点亮 LED 并一直停留在欢迎页，只有短按 KEY1 才熄灯并进入关门主页。当前页面只显示本步骤需要的信息。

本版已通过 ARM Compiler 5 构建和当前 ARM 固件的 79 项业务、按键及绘图仿真检查，并完成约 27 秒的任务调度流程仿真。Flash 占用 17648 字节，RAM 占用 9168 字节。仿真对异常进入、返回、Tick 注入和 LCD 外设作了建模，尚未下载到开发板进行实机验证；验证范围与限制见 [验证记录](docs/validation.md)。

以下预览由实际 ARM 固件的绘图代码输出生成；实物屏幕颜色、方向和流畅度仍需开发板确认。

![分层界面预览](docs/ui-preview.png)

[查看切换过程预览](docs/ui-transition-preview.png)

## 功能与实现状态

| 功能 | 当前实现 |
| --- | --- |
| 启动界面 | 显示 `MICROWAVE`、微波炉图标及开门状态；等待短按 KEY1，不自动进入操作页面 |
| LED 照明 | 启动及软件开门时亮，短按 KEY1 关门时灭 |
| 门状态演示 | KEY1 短按切换软件门状态；运行时开门暂停倒计时 |
| 按键识别 | 两个高电平有效按键，1 ms 独立采样，支持消抖、短按及长按 |
| 时间设置 | 默认 0 秒，以 20 秒为步长设置，范围 0～200 秒，超过 200 秒回到 0 |
| 功率选择 | `Low`、`Medium`、`High` 三档，默认 `Medium` |
| 启动与恢复 | 关门且时间非零时长按 KEY1 启动；暂停后关门并长按 KEY1 恢复 |
| LCD 显示 | 欢迎、主页、时间、档位、运行、暂停和完成七个独立页面；分区动画与变化区域刷新 |
| 显示传输 | SPI1 硬件 SPI + DMA1 Channel3，128×8 RGB565 条带缓冲，稳定画面不重复传输 |
| FreeRTOS | 官方 V10.5.1，1 kHz Tick，抢占调度，三个静态应用任务及静态队列，无动态堆分配 |
| 倒计时与完成 | 按实际经过毫秒计时；归零显示 `COMPLETE`，复位软件门、时间及档位 |
| 实际门状态反馈 | 尚未实现，当前使用软件变量记录关门操作 |

屏幕上的 `Heating` 仅表示软件倒计时运行；当前不输出加热或电机 PWM。后文 PWM 参数用于说明仓库中保留的驱动配置。

## 开发环境与工程配置

| 项目 | 配置 |
| --- | --- |
| 主控 | STM32F103C8 |
| 工程入口 | `project.uvprojx` |
| 构建目标 | `Target 1` |
| 编译器 | 工程记录为 ARM Compiler 5，`V5.06 update 5 (build 528)` |
| 器件支持包 | 工程记录为 `Keil.STM32F1xx_DFP.2.2.0` |
| 固件库 | STM32F10x Standard Peripheral Library V3.5.0 |
| RTOS | FreeRTOS Kernel V10.5.1，ARMCC5 / RVDS Cortex-M3 官方移植层 |
| 启动文件 | `start/startup_stm32f10x_md.s` |
| 外部晶振 | 源码默认 `HSE_VALUE = 8000000`，即 8 MHz |
| 系统时钟 | 目标配置为 HSE × 9 = 72 MHz |
| 总线时钟 | HCLK、PCLK2 为 72 MHz，PCLK1 为 36 MHz；TIM2、TIM3 时钟为 72 MHz |
| Flash | 起始地址 `0x08000000`，大小 `0x10000`（64 KB） |
| SRAM | 起始地址 `0x20000000`，大小 `0x5000`（20 KB） |
| 头文件路径 | `start`、`user`、`bsp`、`library`、`third_party/FreeRTOS/include`、`third_party/FreeRTOS/portable/RVDS/ARM_CM3` |
| 系统异常 | SysTick、SVC、PendSV 由 FreeRTOS 移植层提供；优先级分组为 `NVIC_PriorityGroup_4` |
| CubeMX | 当前项目未包含 `.ioc` 文件；外设通过标准外设库在代码中配置 |

工程同时包含 VS Code/EIDE 配置；当前 EIDE 已配置 AC5、`STM32F10X_MD` 和 ST-Link。

## 硬件与引脚映射

本阶段使用 STM32F103C8 开发板、PC13 低电平有效 LED、两个高电平有效三脚按键模块、ST7735 TFT 屏幕和下载器。若开发板已有 PC13 板载 LED，无需额外连接。舵机和加热接口为后续阶段保留。

| 模块 | 信号 | MCU 引脚 | 配置与说明 |
| --- | --- | --- | --- |
| KEY1 | 软件开关门、启动/恢复 | PB12 | 下拉输入，按下高电平，释放低电平 |
| KEY2 | 时间及功率设置 | PB1 | 下拉输入，按下高电平，释放低电平 |
| LED | 腔体照明演示 | PC13 | 2 MHz 推挽输出，低电平点亮；开门亮、关门灭 |
| 门锁舵机 | PWM | PB0 / TIM3_CH3 | 50 Hz；开门 0°，关门 90° |
| 转盘舵机 | PWM | PA1 / TIM2_CH2 | 50 Hz；运行脉宽 1000 μs，停止脉宽 1500 μs |
| 加热控制 | PWM | PA2 / TIM2_CH3 | 50 Hz；三档占空比 |
| LCD | CLK / SCK | PA5 | SPI1 硬件时钟 |
| LCD | SDA / MOSI | PA7 | SPI1 硬件数据输出 |
| LCD | RS / DC | PB7 | 低电平为命令，高电平为数据 |
| LCD | RST | PB8 | 屏幕复位 |
| LCD | CS | PB9 | 屏幕片选 |

LCD 驱动面向 1.8 英寸、128×160 的 ST7735 屏幕，接线与原工程一致。SPI1 使用模式 3、8 位、MSB 先行，72 MHz 总线默认 8 分频为 9 MHz；DMA1 Channel3 发送像素，不需要 MISO。显示任务独占 SPI/DMA。背光 BLK 为常亮接法，无独立的软件调光控制。屏幕供电及背光接法应以实际模块规格为准。

两个按键模块均为 VCC 接 3V3、GND 接公共 GND；KEY1 的 OUT 接 PB12，KEY2 的 OUT 接 PB1。模块必须支持 3.3V 供电和输出电平。按下时 OUT 为高，释放为低，不使用原手册的低电平按键逻辑。

后续接入舵机及外部驱动时，应按器件要求供电并与控制板共地。PA2 是逻辑控制信号，负载需通过匹配的驱动电路连接。

### PWM 参数

TIM2 与 TIM3 均使用 `PSC = 71`、`ARR = 19999`。在定时器时钟为 72 MHz 时：

```text
计数频率 = 72 MHz / (71 + 1) = 1 MHz
PWM 周期 = (19999 + 1) / 1 MHz = 20 ms
PWM 频率 = 50 Hz
```

| 模块或档位 | 比较值 / 脉宽 | 说明 |
| --- | --- | --- |
| 门锁舵机 | 500～2500 μs | 对应代码中的 0～180°；90°为 1500 μs |
| 转盘停止 | 1500 μs | 连续旋转舵机停止设定值 |
| 转盘运行 | 1000 μs | 连续旋转舵机运行设定值 |
| 加热关闭 | 0 | 0% |
| Low | 6000 | 30% |
| Medium | 12000 | 60% |
| High | 19999 | 约 100%，实际为 99.995% |

转盘和加热输出共用 TIM2 时基，调整频率时需同步评估两个通道。连续旋转舵机的停止点及转向可能需要按实际器件校准。

## 目录与软件分层

```text
.
├── user/                   应用层
│   ├── main.c              LED、优先级分组、时基与按键初始化，启动调度器
│   ├── app.c/.h            软件门、参数、倒计时及页面业务状态机
│   ├── app_rtos.c/.h       输入、控制和显示任务，静态队列及诊断
│   ├── ui.c/.h             分层页面、条带绘图与切换动画
│   ├── FreeRTOSConfig.h    静态内存、Tick 和中断优先级配置
│   ├── rtos_hooks.c/.h     空闲任务内存、按键 Tick hook 和故障处理
│   ├── stm32f10x_conf.h     标准外设库配置
│   └── stm32f10x_it.c/.h    保留的芯片异常处理
├── bsp/                    板级驱动层
│   ├── delay.c/.h          FreeRTOS 毫秒时基、任务延时与 DWT 微秒延时
│   ├── key.c/.h            双键独立消抖、短长按事件队列
│   ├── led.c/.h            LED 控制
│   ├── servo.c/.h          门锁舵机控制
│   ├── turntable.c/.h      转盘控制
│   ├── heater.c/.h         加热 PWM 控制
│   ├── lcd.c/.h            ST7735 硬件 SPI、DMA、行缓冲与超时处理
│   └── exti.c/.h           外部中断配置，当前主流程未调用
├── library/                STM32F10x 标准外设库
├── start/                  CMSIS、启动文件及系统时钟配置
├── third_party/FreeRTOS/    官方内核、移植层、MIT 许可及来源说明
├── docs/                   绘图预览与集成验证记录
├── .cmsis/                 配套器件及头文件资源
├── .eide/                  EIDE 工程配置
├── .vscode/                编辑器及构建任务配置
├── Objects/                Keil 编译输出目录
├── Listings/               Keil 列表与映射输出目录
├── build/                  EIDE 编译输出目录
├── project.uvprojx         Keil 工程
├── project.uvoptx          Keil 调试等选项
├── project.code-workspace  VS Code 工作区
└── README.md               项目说明
```

应用层分别管理运行状态与页面状态，BSP 提供 GPIO、按键、LED 及显示接口，启动层负责中断向量、运行环境及系统时钟初始化。官方内核来源与固定 commit 见 [SOURCE.md](third_party/FreeRTOS/SOURCE.md)，内核源码保持官方原文。

| 任务 | 优先级 | 栈（32 位字） | 职责 |
| --- | --- | --- | --- |
| Input | 3 | 192 | 每 1 ms 消费双键事件并发送输入队列 |
| Control | 2 | 256 | 最长等待输入 10 ms，处理业务、LED 和倒计时，发布最新快照 |
| Display | 1 | 512 | 初始化并独占 LCD，每约 10 ms 推进绘图和动画 |
| Idle | 0 | 128 | 内核空闲任务 |

SysTick 的 FreeRTOS Tick hook 每 1 ms 调用 `KEY_Tick1ms()`。输入队列保存 12 组事件，界面快照队列只有一个元素，始终保留最新状态。控制任务独占业务状态，显示任务只消费值副本，动画期间按键和倒计时可由高优先级任务继续处理。

UI 使用静态 128×8 RGB565 条带缓冲（2048 字节），LCD 使用 256 字节行缓冲，每次刷新最多两条带。页面稳定时不发送像素；数值、提示和进度变化仅刷新相关区域。LCD 初始化的毫秒等待使用 `vTaskDelay()` 让出 CPU，欢迎页通过事件等待 KEY1。

## 编译与下载

### Keil MDK

1. 安装 Keil MDK、ARM Compiler 5 和 STM32F1 器件支持包。工程记录的版本见上表；更换工具链后需重新验证兼容性。
2. 打开 `project.uvprojx`，选择 `Target 1`，核对器件为 `STM32F103C8`、Flash 为 64 KB、SRAM 为 20 KB。
3. 在 **Options for Target → C/C++ → Define** 中核对预处理宏为 `USE_STDPERIPH_DRIVER,STM32F10X_MD`，当前工程已配置这两个宏。
4. 核对头文件搜索路径包含上表所列路径，工程中包含四个 FreeRTOS 编译文件和新应用文件，执行 **Rebuild**，根据实际日志处理错误及警告。
5. Keil 输出目录配置为 `Objects/`，输出名为 `project`，可执行文件为 `project.axf`。当前未启用 **Create HEX File**；如需 HEX，请在 **Options for Target → Output** 中勾选后重新构建。
6. 在 **Debug** 和 **Utilities** 中选择实际下载器，检查 Flash 下载算法。项目调试选项中存在 ST-Link 配置，使用前应核对本机驱动及设备连接。
7. 使用 SWD 下载时，将 SWDIO 接 PA13、SWCLK 接 PA14，并连接 GND 和下载器要求的目标参考电压；按需连接 NRST。执行 **Download**，完成后复位运行。

系统时钟代码依赖 8 MHz 外部晶振正常起振。工程器件配置中的 `CLOCK(12000000)` 元数据与源码配置不同，运行时的目标时钟应以 `start/system_stm32f10x.c` 及实际硬件为准。

### VS Code / EIDE

可通过 `project.code-workspace` 打开工作区，现有任务包括 `build`、`flash`、`build and flash`、`rebuild` 和 `clean`，均依赖 EIDE 插件命令。

当前 `.eide/eide.yml` 使用 AC5 工具链，已接入 FreeRTOS 及应用层文件，移除失效的蜂鸣器和 DHT11 源文件引用，包含 `STM32F10X_MD` 宏，并选用 ST-Link/SWD 下载。需按本机情况设置编译器、下载器驱动及连接。EIDE 构建输出为 `build/Target 1/project.axf` 和 `build/Target 1/project.hex`。修改后应重新构建，再将最新固件下载到开发板验证。

## 操作说明

消抖约 20 ms。短按在释放并消抖后生效；长按在稳定按住约 1 秒后立即生效，释放不会再触发短按。上电一直按住的键必须先释放再按。

| 当前页面 | 操作 | 结果 |
| --- | --- | --- |
| 欢迎／开门待机 | 不操作 | 一直停留，LED 亮；不显示时间与档位设置 |
| 欢迎／开门待机 | 短按 KEY1 | 软件门关闭、LED 灭，进入主页 |
| 主页 | 短按 KEY2 | 进入时间设置，本次不增加时间 |
| 主页 | 长按 KEY2 | 提示先短按 KEY2 进入时间设置，不越过菜单层级 |
| 时间设置 | 短按 KEY2 | 时间增加 20 秒；200 秒后再加一次回到 0 秒 |
| 时间设置 | 长按 KEY2 | 进入档位设置 |
| 档位设置 | 短按 KEY2 | 档位循环；从默认 Medium 开始，依次为 High、Low、Medium |
| 档位设置 | 长按 KEY2 | 返回时间设置 |
| 主页／时间／档位设置 | 短按 KEY1 | 软件开门、LED 亮，返回欢迎页；保留已设时间和档位 |
| 主页／时间／档位设置 | 长按 KEY1 | 时间大于 0 时进入运行页；0 秒时提示 `Set time first` |
| 运行 | 短按 KEY1 | 软件开门、LED 亮，进入暂停页并保留剩余毫秒 |
| 暂停，软件门打开 | 短按 KEY1 | 软件关门、LED 灭，仍停留在暂停页 |
| 暂停，软件门关闭 | 长按 KEY1 | 恢复剩余倒计时，返回运行页 |
| 软件门打开 | 长按 KEY1 或操作 KEY2 | 提示 `Close door first`，不跳到设置或运行页 |
| 正在运行 | 长按 KEY1 | 提示 `Already running`，不重复启动 |
| 运行或暂停 | 操作 KEY2 | 保持本轮设置，显示相应操作提示 |
| 倒计时归零 | 自动处理 | 进入完成页，软件门打开、LED 亮，时间恢复 0、功率恢复 Medium |
| 完成 | 短按 KEY1 | 软件关门、LED 灭，进入主页开始新一轮 |

例如，设置 60 秒、Medium 档：上电欢迎页短按 KEY1 进入主页，短按 KEY2 进入时间页，再短按三次 KEY2，保留默认功率，最后长按 KEY1 进入运行页。运行中短按 KEY1 开门并进入暂停页，再短按关门，长按恢复；归零进入完成页，短按 KEY1 回主页设置下一轮。

页面路径为：`欢迎 → 主页 → 时间设置 ⇄ 档位设置 → 运行 ⇄ 暂停 → 完成`。主页仅提供入口，时间页只突出时间，档位页只突出档位；运行页才同时显示剩余时间与当前档位。

## 当前限制与维护说明

- 本阶段真实控制 PC13 LED；门、加热状态仍为软件演示。主程序不初始化门锁、转盘和加热驱动，无实际门反馈、温度采集、温度保护及蜂鸣器功能。
- 运行及暂停期间锁定时间、功率设置，以保存本轮剩余时间；关门后需长按 KEY1 恢复，不能通过长按 KEY1 直接暂停，应短按开门暂停。
- 两键同组送达时，KEY1 优先，该组 KEY2 事件被忽略。每键驱动环形队列最多保留 7 个事件，满时丢弃新事件；Input 每 1 ms 消费，跨任务输入队列若满则记录故障并停止。
- SysTick 由 FreeRTOS 独占。`delay_ms()` 只允许在线程中使用；调度器运行后使用任务延时，启动前使用 DWT 忙等，不可在 ISR 内调用。
- `EXTIX_Init()` 虽然存在于 BSP 中，但 `main()` 未调用，当前按键由 FreeRTOS Tick hook 每 1 ms 读取 GPIO 并生成事件。
- SPI 或 DMA 等待超时会记录 `lcd_fault_code`；任务、队列、断言及栈溢出故障通过 `rtos_fault_code` 诊断并点亮 LED 后停止。实机需检查屏幕颜色、偏移、SPI 信号质量和各任务栈余量。
- 部分源文件中文注释存在编码异常。维护时应先确认原文件编码，避免批量转码造成额外变化。

修改参数时，可从以下位置查找：

| 参数 | 位置 |
| --- | --- |
| 时间步长、上限及默认功率 | `user/app.c`：`TIME_STEP`、`TIME_MAX`、`App_Init()` |
| 任务周期、栈和队列容量 | `user/app_rtos.c` |
| RTOS Tick、静态内存和中断优先级 | `user/FreeRTOSConfig.h` |
| 界面颜色、布局与动画 | `user/ui.c` |
| 长按与消抖阈值 | `bsp/key.h`：`KEY_LONG_MS`、`KEY_DEBOUNCE_MS` |
| 按键、LED 引脚 | `bsp/key.h`、`bsp/led.h` |
| 门锁开关角度 | `bsp/servo.h`：`SERVO_ANGLE_OPEN`、`SERVO_ANGLE_CLOSE` |
| 转盘运行及停止脉宽 | `bsp/turntable.h`：`TURNTABLE_PULSE_RUN`、`TURNTABLE_PULSE_STOP` |
| 加热档位比较值 | `bsp/heater.c`：`HEATER_LOW`、`HEATER_MEDIUM`、`HEATER_HIGH` |
| LCD 引脚及显示偏移 | `bsp/lcd.c`：引脚宏、`LCD_X_OFFSET`、`LCD_Y_OFFSET` |
| LCD 尺寸 | `bsp/lcd.h`：`LCD_W`、`LCD_H` |
| LCD SPI 速度 | `bsp/lcd.c`：`LCD_SPI_PRESCALER` |

按项目协作约定，后续新增或修改 `.c` 文件时，应同步维护同目录、同名的 `.md` 说明文件。
