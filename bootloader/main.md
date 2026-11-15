# main.c 串口 Bootloader

Bootloader 复位后关闭 PA2。KEY2（PB1）持续按住 20 ms 或 APP 无效时进入升级模式。启动 APP 前检查 Flash 元数据、初始 MSP、Thumb 入口和整镜像 CRC，再清理中断并设置 VTOR。汇编跳板设置 MSP 后直接跳到 APP 的 Reset_Handler。

## Flash 分区

| 区域 | 地址 | 容量 |
| --- | --- | --- |
| Bootloader | `0x08000000–0x08003FFF` | 16 KiB |
| APP | `0x08004000–0x0800FBFF` | 47 KiB |
| 校验记录 | `0x0800FC00–0x0800FFFF` | 1 KiB |

布局由 `user/firmware_layout.h` 提供。校验记录依次保存镜像长度、CRC32 和有效标记。开始写入时先擦除校验页，完成后最后写有效标记。升级中断后，可重新传输整个 APP。

## 串口协议

USART1 使用 115200、8N1。PA9 为 TX，PA10 为 RX。电脑和开发板通过 3.3 V 逻辑的 USB 转串口模块交叉连接 TX/RX，并共地。USART1 RX 由 DMA1 Channel 5 搬运。Bootloader 轮询 DMA 完成或超时。电脑等 ACK 后才继续发包，Flash 擦写期间不会有正在传输的有效包。

1. 电脑发 `0x7E`，等待 ACK（`0x06`）。
2. 发 `MWUP`、32 位镜像长度和 32 位标准 CRC32。多字节字段均为小端。长度合法且目标区域擦除成功后回复 ACK。
3. 每包为 32 位字节偏移、16 位长度、数据和 32 位包 CRC32。除末包外每包 256 字节。CRC 覆盖偏移、长度及数据。通过检查并写入后回复 ACK。收到相同的上一包时，仅在内容一致时确认。完整但校验失败的包回复 NAK（`0x15`），电脑可重发。
4. 发 `DONE`，整镜像从 Flash 读回校验成功并写入有效标记后才回复 ACK。松开 KEY2 并复位，启动新 APP。

## 构建和上板

```powershell
python tools/build_upgrade.py --version V1.0 --out-dir build/upgrade/v1
python -m pip install pyserial
python tools/upload_app.py COM5 build/upgrade/v1/app.bin
```

若未自动找到 ARMCC5，给构建命令加 `--toolchain` 并指定 ARMCC 安装目录。首次用 ST-Link 烧录 `bootloader.hex`，用串口上传 `app.bin`。示例里的 COM5 换成实际串口。偏移版 `app.hex` 用于调试，常规 SWD 恢复使用原工程从 Flash 起点构建的 HEX。

上板验收时拔掉 PTC 电源。先上传 V1.0，复位确认欢迎或待机页显示对应版本。再用不同版本的 BIN 验证更新，另做一次中途断开再重传的测试。默认升级 APP 保持 PA2 关闭，需要低压联调构建时可显式传入 `--relay-led-test`。

## 实机验收

- 通过 CH340（COM5）先后上传 V1.0 和 V1.1，各 22628 字节。两次 ST-Link 回读均与本地镜像一致。
- 按住 KEY2 复位进入更新，发送并确认首个数据包后中断，再复位。有效标记仍为擦除值，VTOR 停在 `0x08000000`，重新从 COM5 上传完整 V1.1 成功。
- 最终复位后 VTOR 为 `0x08004000`，输入、控制和显示任务计数持续增加，RTOS 和 LCD 故障码为零，PA2 为低电平。此轮 PTC 断电，使用默认升级 APP。
