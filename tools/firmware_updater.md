# 微波炉固件升级上位机

双击 `MicrowaveFirmwareUpdater.exe`。把 `heating-app.bin` 放在 EXE 同一目录时，启动即选中该镜像，也可以手动挑选其他为升级分区构建的 BIN。

1. 保持 PTC 电源断开，连接 3.3 V 逻辑的 USB 转串口模块。模块 TX 接 PA10，RX 接 PA9，GND 共地。
2. 点击刷新并选择对应的 COM 口。选择 BIN 后，上位机检查大小、初始 MSP 和复位入口，并显示 CRC32。
3. 按住 KEY2，按一下 RESET，再松开 KEY2，点击开始上传。也可先点击开始上传，再在 25 秒内让板子进入 Bootloader。
4. 界面显示整镜像校验通过后，松开 KEY2 并按一下 RESET 运行新固件。若升级中断，重新进入 Bootloader 后完整上传。

上位机通过 USART1 的 115200、8N1 升级协议上传镜像。进度按 Bootloader 确认写入的字节推进，收到 `DONE` 的确认后才显示校验通过。板上加热由 APP 和按键控制。首次安装 Bootloader 仍通过 ST-Link，见 [Bootloader 说明](../bootloader/main.md)。

## Windows 打包

```powershell
python -m pip install pyserial==3.5 pyinstaller==6.16.0
python tools/package_updater.py --app build/upgrade/v1.3-heating/heating-app.bin
```

默认输出 `build/updater/MicrowaveFirmwareUpdater.exe`，并将指定镜像复制为同目录的 `heating-app.bin`。已打包的 EXE 可直接运行。
