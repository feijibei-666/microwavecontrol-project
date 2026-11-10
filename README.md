# STM32 微波炉控制演示项目

基于 **STM32F103C8（Cortex-M3）** 和 **STM32F10x 标准外设库** 的微波炉控制演示程序。项目通过双按键完成关门、时间设置、功率选择和运行切换，使用 ST7735 TFT 显示界面，并输出门锁舵机、转盘舵机和加热控制 PWM 信号。

本文按当前源码和工程配置编写。界面欢迎页版本为 `V1.1`；文档中的操作效果为代码预期行为，尚未在本次文档编写中进行重新编译或实机验证。

## 功能与实现状态

| 功能 | 当前实现 |
| --- | --- |
| 启动界面 | 显示 `Microwave Demo` 和 `V1.1`，约 2 秒后进入 `Standby` |
| 门锁演示 | 上电处于开门位置；第一次按下并松开 KEY1 后，舵机转到关门位置 |
| 腔体照明演示 | 待机开门时 LED 点亮，执行关门后熄灭 |
| 时间设置 | 默认 0 秒，以 20 秒为步长设置，范围 0～200 秒，超过 200 秒回到 0 |
| 功率选择 | `Low`、`Medium`、`High` 三档，默认 `Medium` |
| 启动与停止 | 关门后长按并松开 KEY1，切换加热 PWM 和转盘运行状态 |
| LCD 显示 | 显示设置模式、时间和功率，支持 RGB565 颜色及 8×16 ASCII 字符 |
| 倒计时与自动停止 | 尚未实现，当前时间值只用于设置和显示 |
| 实际门状态反馈 | 尚未实现，当前使用软件变量记录关门操作 |

加热模块输出的是控制用 PWM 信号；其占空比表示代码中的档位设定，不代表已经测得的实际加热功率。

## 开发环境与工程配置

| 项目 | 配置 |
| --- | --- |
| 主控 | STM32F103C8 |
| 工程入口 | `project.uvprojx` |
| 构建目标 | `Target 1` |
| 编译器 | 工程记录为 ARM Compiler 5，`V5.06 update 5 (build 528)` |
| 器件支持包 | 工程记录为 `Keil.STM32F1xx_DFP.2.2.0` |
| 固件库 | STM32F10x Standard Peripheral Library V3.5.0 |
| 启动文件 | `start/startup_stm32f10x_md.s` |
| 外部晶振 | 源码默认 `HSE_VALUE = 8000000`，即 8 MHz |
| 系统时钟 | 目标配置为 HSE × 9 = 72 MHz |
| 总线时钟 | HCLK、PCLK2 为 72 MHz，PCLK1 为 36 MHz；TIM2、TIM3 时钟为 72 MHz |
| Flash | 起始地址 `0x08000000`，大小 `0x10000`（64 KB） |
| SRAM | 起始地址 `0x20000000`，大小 `0x5000`（20 KB） |
| 头文件路径 | `start`、`user`、`bsp`、`library` |
| CubeMX | 当前项目未包含 `.ioc` 文件；外设通过标准外设库在代码中配置 |

工程同时包含 VS Code/EIDE 配置，但其中存在需要核对的历史配置，详见下文。

## 硬件与引脚映射

按源码连接 STM32F103C8 开发板、两个按键、状态 LED、门锁舵机、连续旋转舵机和 ST7735 TFT 屏幕。

| 模块 | 信号 | MCU 引脚 | 配置与说明 |
| --- | --- | --- | --- |
| KEY1 | 关门、启动/停止 | PB12 | 上拉输入，按下为低电平 |
| KEY2 | 时间及功率设置 | PB1 | 上拉输入，按下为低电平 |
| LED | 腔体照明演示 | PC13 | 推挽输出，低电平点亮 |
| 门锁舵机 | PWM | PB0 / TIM3_CH3 | 50 Hz；开门 0°，关门 90° |
| 转盘舵机 | PWM | PA1 / TIM2_CH2 | 50 Hz；运行脉宽 1000 μs，停止脉宽 1500 μs |
| 加热控制 | PWM | PA2 / TIM2_CH3 | 50 Hz；三档占空比 |
| LCD | CLK / SCK | PA5 | 软件 SPI 时钟 |
| LCD | SDA / MOSI | PA7 | 软件 SPI 数据输出 |
| LCD | RS / DC | PB7 | 低电平为命令，高电平为数据 |
| LCD | RST | PB8 | 屏幕复位 |
| LCD | CS | PB9 | 屏幕片选 |

LCD 驱动面向 1.8 英寸、128×160 的 ST7735 屏幕，采用 GPIO 模拟 SPI，未使用硬件 SPI 外设，也不需要 MISO。背光 BLK 在驱动说明中为常亮接法，无独立的软件调光控制。屏幕供电及背光接法应以实际模块规格为准。

按键按下时应将对应输入拉到 GND。舵机及外部驱动按器件要求供电，并与控制板共地。PA2 是逻辑控制信号，负载需通过匹配的驱动电路连接。

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
│   ├── main.c              初始化、界面及按键控制流程
│   ├── stm32f10x_conf.h     标准外设库配置
│   └── stm32f10x_it.c/.h    异常与中断处理模板
├── bsp/                    板级驱动层
│   ├── delay.c/.h          SysTick 阻塞延时
│   ├── key.c/.h            按键消抖、短按及长按识别
│   ├── led.c/.h            LED 控制
│   ├── servo.c/.h          门锁舵机控制
│   ├── turntable.c/.h      转盘控制
│   ├── heater.c/.h         加热 PWM 控制
│   ├── lcd.c/.h            ST7735 软件 SPI 显示驱动
│   └── exti.c/.h           外部中断配置，当前主流程未调用
├── library/                STM32F10x 标准外设库
├── start/                  CMSIS、启动文件及系统时钟配置
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

应用层直接调用 BSP 接口；BSP 使用标准外设库配置 GPIO、TIM 和 SysTick；启动层负责中断向量、运行环境及系统时钟初始化。当前采用裸机轮询方式，无 RTOS，按键处理和延时均为阻塞式。

## 编译与下载

### Keil MDK

1. 安装 Keil MDK、ARM Compiler 5 和 STM32F1 器件支持包。工程记录的版本见上表；更换工具链后需重新验证兼容性。
2. 打开 `project.uvprojx`，选择 `Target 1`，核对器件为 `STM32F103C8`、Flash 为 64 KB、SRAM 为 20 KB。
3. 在 **Options for Target → C/C++ → Define** 中核对预处理宏。当前工程显式配置了 `USE_STDPERIPH_DRIVER`，但工程文件及本地头文件未显式启用 `STM32F10X_MD`。若构建环境未自动提供该宏，需要将其加入，最终应包含 `USE_STDPERIPH_DRIVER,STM32F10X_MD`。
4. 核对头文件搜索路径包含 `start`、`user`、`bsp`、`library`，执行 **Rebuild**，根据实际日志处理错误及警告。
5. Keil 输出目录配置为 `Objects/`，输出名为 `project`，可执行文件为 `project.axf`。当前未启用 **Create HEX File**；如需 HEX，请在 **Options for Target → Output** 中勾选后重新构建。
6. 在 **Debug** 和 **Utilities** 中选择实际下载器，检查 Flash 下载算法。项目调试选项中存在 ST-Link 配置，使用前应核对本机驱动及设备连接。
7. 使用 SWD 下载时，将 SWDIO 接 PA13、SWCLK 接 PA14，并连接 GND 和下载器要求的目标参考电压；按需连接 NRST。执行 **Download**，完成后复位运行。

系统时钟代码依赖 8 MHz 外部晶振正常起振。工程器件配置中的 `CLOCK(12000000)` 元数据与源码配置不同，运行时的目标时钟应以 `start/system_stm32f10x.c` 及实际硬件为准。

### VS Code / EIDE

可通过 `project.code-workspace` 打开工作区，现有任务包括 `build`、`flash`、`build and flash`、`rebuild` 和 `clean`，均依赖 EIDE 插件命令。

当前 `.eide/eide.yml` 使用 AC5 工具链，但还引用了项目中不存在的 `bsp/buzzer.c`、`bsp/dht11.c`，且 J-Link 下载配置中的器件信息为 `null`。使用此路径前需修正源文件列表、核对 `STM32F10X_MD` 宏，并配置本机编译器及实际下载器。现有编译产物不能证明当前源码可以直接重新构建。

## 操作说明

按键长按阈值约为 1 秒，消抖延时为 20 ms。按键事件在松开后返回，因此长按操作需要“按住约 1 秒后松开”。

| 当前状态 | 操作 | 结果 |
| --- | --- | --- |
| 上电 | 等待约 2 秒 | 欢迎页结束后显示 `Standby`，LED 点亮，门锁处于开门位置，加热关闭、转盘停止 |
| 开门待机 | 按下并松开 KEY1 | 门锁转到 90°，LED 熄灭，进入设置显示页，默认 `Time: 0s`、`Power: Medium` |
| 关门后，尚未选择设置模式 | 短按 KEY2 | 进入时间设置模式，本次不增加时间 |
| 时间设置模式 | 短按 KEY2 | 时间增加 20 秒；200 秒后再加一次回到 0 秒 |
| 关门后，非功率设置模式 | 长按 KEY2 | 进入功率设置模式 |
| 功率设置模式 | 短按 KEY2 | 按 `Low → Medium → High → Low` 循环；从默认 Medium 开始，首次切换为 High |
| 功率设置模式 | 长按 KEY2 | 切换回时间设置模式 |
| 关门且已停止 | 长按 KEY1 | 按当前功率设置输出加热 PWM，同时启动转盘 |
| 正在运行 | 长按 KEY1 | 关闭加热 PWM，同时停止转盘 |
| 关门后 | 短按 KEY1 | 当前无对应动作 |

例如，设置 60 秒、Medium 档：上电待机后按下并松开 KEY1，短按 KEY2 进入时间设置，再短按三次 KEY2，保留默认功率，最后长按并松开 KEY1 启动。**当前不会在 60 秒后自动停止，需再次长按并松开 KEY1 停止。**

## 当前限制与维护说明

- `cook_seconds` 只参与设置和显示，未接入计时逻辑；时间为 0 时也可以启动。
- `door_closed` 在关门操作后置为 1，没有实际门开关输入，也没有运行期间开门停机逻辑。当前主流程没有重新开门的操作分支，复位后重新进入开门待机状态。
- 运行期间仍能修改时间和功率设置；功率修改仅更新变量及显示，正在输出的加热 PWM 要到停止后再次启动时才应用新值。
- 界面未单独显示运行/停止状态，也未实现完成提示、温度采集或温度保护。EIDE 中的蜂鸣器及 DHT11 文件引用不代表已有对应功能。
- 按键扫描等待松开期间会阻塞主循环；硬件定时器 PWM 仍保持输出。`SysTick_Handler()` 当前为空，SysTick 用于轮询延时。
- `EXTIX_Init()` 虽然存在于 BSP 中，但 `main()` 未调用，当前按键功能使用轮询扫描。
- 部分源文件中文注释存在编码异常。维护时应先确认原文件编码，避免批量转码造成额外变化。

修改参数时，可从以下位置查找：

| 参数 | 位置 |
| --- | --- |
| 时间步长、上限及默认功率 | `user/main.c`：`TIME_STEP`、`TIME_MAX`、`power_level` |
| 长按阈值 | `bsp/key.c`：`KEY_LONG_MS` |
| 按键、LED 引脚 | `bsp/key.h`、`bsp/led.h` |
| 门锁开关角度 | `bsp/servo.h`：`SERVO_ANGLE_OPEN`、`SERVO_ANGLE_CLOSE` |
| 转盘运行及停止脉宽 | `bsp/turntable.h`：`TURNTABLE_PULSE_RUN`、`TURNTABLE_PULSE_STOP` |
| 加热档位比较值 | `bsp/heater.c`：`HEATER_LOW`、`HEATER_MEDIUM`、`HEATER_HIGH` |
| LCD 引脚及显示偏移 | `bsp/lcd.c`：引脚宏、`LCD_X_OFFSET`、`LCD_Y_OFFSET` |
| LCD 尺寸 | `bsp/lcd.h`：`LCD_W`、`LCD_H` |

按项目协作约定，后续新增或修改 `.c` 文件时，应同步维护同目录、同名的 `.md` 说明文件。
