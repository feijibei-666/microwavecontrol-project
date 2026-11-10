# FreeRTOS 内核来源

仅导入本工程需要的官方 FreeRTOS Kernel V10.5.1 文件；源码保持官方原文，没有复制手册附件中的定制内核。

- 官方仓库：https://github.com/FreeRTOS/FreeRTOS-Kernel
- 固定 tag：`V10.5.1`
- tag 对象：`d7b40dbed508c305c2a32ccf3982045ec9ba8734`
- 固定 commit：`def7d2df2b0506d3d249334974f51e427c17a41c`
- 下载包：https://codeload.github.com/FreeRTOS/FreeRTOS-Kernel/tar.gz/refs/tags/V10.5.1
- 许可：MIT，见 `LICENSE.md`，并保留每个源码的版权与许可声明。

编译文件为 `tasks.c`、`list.c`、`queue.c` 和 `portable/RVDS/ARM_CM3/port.c`。导入全部公开头文件以保留头文件间的依赖；未加入 event groups、stream buffer、software timer、协程和 heap 实现。

项目设置位于 `user/FreeRTOSConfig.h`：静态任务和队列，动态分配关闭，1 kHz Tick，抢占和时间片启用。SysTick、SVC、PendSV 名称通过宏直接映射到官方 Cortex-M3 移植层。官方源码不包含项目业务改动。

导入的 23 个 C/H 文件已与上述官方版本逐字节比较，全部一致；逐文件 SHA-256 见 `SHA256SUMS`。
