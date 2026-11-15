# system_stm32f10x.c 启动与向量表

保留标准外设库的时钟初始化。普通 SWD 构建与独立 Bootloader 的向量表起点仍是 Flash 基址。只有定义 `UPGRADE_APP=1` 的应用构建才使用 `firmware_layout.h` 的应用偏移量。构建脚本从同一配置生成 APP 链接分区，避免应用启动时把 VTOR 恢复到 Bootloader 向量表。
