#!/usr/bin/env python3
"""Upload a relocated APP.bin to the microwave controller over USART1."""

import argparse
import binascii
import struct
import sys
import time
from pathlib import Path

from build_upgrade import read_layout


BAUD_RATE = 115200
CHUNK_SIZE = 256
FRAME_ATTEMPTS = 4
HELLO = b"\x7e"
ACK = b"\x06"
NAK = b"\x15"


def crc32(data):
    return binascii.crc32(data) & 0xFFFFFFFF


def validate_image(image):
    layout = read_layout()
    max_size = layout.metadata_base - layout.app_base
    if not 8 <= len(image) <= max_size:
        raise ValueError(f"镜像长度必须在 8 至 {max_size} 字节之间。")

    # 在发送会触发擦写的镜像头之前，按 Bootloader 的规则验证两个启动向量。
    msp, reset = struct.unpack_from("<II", image)
    reset_address = reset & ~1
    if not layout.sram_base < msp <= layout.sram_top or msp % 8:
        raise ValueError("APP 初始栈指针无效，请选择升级构建生成的 app.bin。")
    if not reset & 1 or not layout.app_base + 8 <= reset_address < layout.app_base + len(image):
        raise ValueError(
            f"APP 复位向量无效，请选择链接到 0x{layout.app_base:08X} 的 app.bin。"
        )


def discover(port, ready_timeout):
    deadline = time.monotonic() + ready_timeout
    port.timeout = 0.25
    while time.monotonic() < deadline:
        port.write(HELLO)
        if port.read(1) == ACK:
            port.timeout = 4
            return
    raise RuntimeError("Bootloader 未响应。请按住 KEY2 复位，再运行上传命令。")


def upload(port, image, ready_timeout, *, on_status=None, on_progress=None):
    """Upload a validated image; optionally report status and ACKed byte counts."""
    def report(message):
        if on_status is None:
            print(message, flush=True)
        else:
            on_status(message)

    total = len(image)
    report("等待 Bootloader。按住 KEY2 复位，进入升级模式后松开。")
    port.reset_input_buffer()
    discover(port, ready_timeout)

    if on_status is not None:
        on_status("Bootloader 已连接，正在准备 Flash。")
    port.write(struct.pack("<4sII", b"MWUP", total, crc32(image)))
    if port.read(1) != ACK:
        raise RuntimeError("Bootloader 未接受镜像头。请检查板卡和 APP.bin，复位后重新上传。")

    if on_status is not None:
        on_status("正在写入固件。")
    for offset in range(0, total, CHUNK_SIZE):
        chunk = image[offset:offset + CHUNK_SIZE]
        body = struct.pack("<IH", offset, len(chunk)) + chunk
        frame = body + struct.pack("<I", crc32(body))
        for _ in range(FRAME_ATTEMPTS):
            port.write(frame)
            reply = port.read(1)
            if reply == ACK:
                break
            if reply != NAK:
                # ACK 没有序号，超时后无法区分迟到的 ACK 和新一帧的 ACK。
                raise RuntimeError(f"偏移 {offset} 的帧响应异常。请复位后重新上传。")
        else:
            raise RuntimeError(f"偏移 {offset} 的数据帧连续被拒绝。请复位后重新上传。")

        sent = offset + len(chunk)
        if on_progress is not None:
            on_progress(sent, total)
        elif sent % 4096 == 0 or sent == total:
            report(f"已写入 {sent}/{total} 字节")

    # 最后一帧 ACK 仅确认当帧写入，DONE 的 ACK 才确认整镜像有效。
    if on_status is not None:
        on_status("正在校验完整固件。")
    port.write(b"DONE")
    reply = port.read(1)
    if reply == NAK:
        raise RuntimeError("整镜像校验失败。请按住 KEY2 复位并重新上传。")
    if reply != ACK:
        raise RuntimeError("未收到升级完成回执。请松开 KEY2 后复位检查版本，必要时重新上传。")
    report("升级校验通过。松开 KEY2 后复位以启动 APP。")


def main():
    parser = argparse.ArgumentParser(description="通过串口上传微波炉控制器 APP.bin")
    parser.add_argument("port", help="USB 转串口设备名，例如 COM5")
    parser.add_argument("image", type=Path, help="按升级分区构建的 app.bin 路径")
    parser.add_argument("--ready-timeout", type=int, default=10, metavar="秒",
                        help="等待 Bootloader 响应的时间，默认 10 秒")
    args = parser.parse_args()
    if args.ready_timeout <= 0:
        parser.error("--ready-timeout 必须大于 0")

    try:
        import serial
    except ImportError:
        print("缺少 pyserial。请运行 python -m pip install pyserial。", file=sys.stderr)
        return 1

    try:
        image = args.image.read_bytes()
        validate_image(image)
        with serial.Serial(args.port, BAUD_RATE, timeout=0.25, write_timeout=4) as port:
            upload(port, image, args.ready_timeout)
    except KeyboardInterrupt:
        print("\n上传中断。重新上传前请按住 KEY2 复位。", file=sys.stderr)
        return 130
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"上传失败。{exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
