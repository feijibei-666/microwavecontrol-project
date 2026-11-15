"""Windows graphical front end for the microwave controller's serial Bootloader."""

import queue
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import serial
from serial.tools import list_ports

from build_upgrade import ROOT, read_layout
from upload_app import BAUD_RATE, crc32, upload, validate_image


BACKGROUND = "#edf2f4"
CARD = "#ffffff"
INK = "#10242c"
MUTED = "#445d66"
ACCENT = "#08766e"
ERROR = "#9e2c2c"
FONT = "Microsoft YaHei UI"


class FirmwareUpdater:
    def __init__(self, root):
        self.root = root
        self.events = queue.Queue()
        self.busy = False
        self.ports = {}
        self.image_path = None
        self.image = None
        self.port_name = tk.StringVar()
        self.file_name = tk.StringVar(value="请选择 .bin 文件")
        self.file_details = tk.StringVar(value="")
        self.status = tk.StringVar(value="等待选择串口和固件")
        self.progress_label = tk.StringVar(value="")

        root.title("微波炉固件升级")
        root.geometry("740x590")
        root.minsize(650, 565)
        root.configure(background=BACKGROUND)
        root.protocol("WM_DELETE_WINDOW", self.close)
        self.build_window()
        self.refresh_ports()
        self.load_default_image()
        root.after(50, self.drain_events)

    def build_window(self):
        self.root.option_add("*Font", (FONT, 10))
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("Upload.TButton", background=ACCENT, foreground=CARD, padding=(22, 10))
        style.map("Upload.TButton",
                  background=[("disabled", "#ccd8dc"), ("active", "#056159")],
                  foreground=[("disabled", "#52676f"), ("active", CARD)])
        style.configure("Upload.Horizontal.TProgressbar", background=ACCENT,
                        troughcolor="#e3ecee", bordercolor="#e3ecee",
                        lightcolor=ACCENT, darkcolor=ACCENT)

        outer = tk.Frame(self.root, bg=BACKGROUND, padx=32, pady=28)
        outer.pack(fill="both", expand=True)
        tk.Label(outer, text="微波炉固件升级", font=(FONT, 22, "bold"),
                 background=BACKGROUND, foreground=INK).pack(anchor="w")
        tk.Label(outer, text="STM32F103  ·  USART1 / 115200", font=("Segoe UI", 10),
                 background=BACKGROUND, foreground=MUTED).pack(anchor="w", pady=(4, 18))

        card = tk.Frame(outer, background=CARD, padx=22, pady=19)
        card.pack(fill="x")
        card.columnconfigure(1, weight=1)
        tk.Label(card, text="串口", bg=CARD, fg=INK, font=(FONT, 10, "bold")).grid(
            row=0, column=0, sticky="w", padx=(0, 18))
        self.port_box = ttk.Combobox(card, state="readonly", textvariable=self.port_name, width=38)
        self.port_box.grid(row=0, column=1, sticky="ew", padx=(0, 10))
        self.port_box.bind("<<ComboboxSelected>>", self.port_selected)
        self.refresh_button = ttk.Button(card, text="刷新", command=self.refresh_ports)
        self.refresh_button.grid(row=0, column=2, sticky="ew")

        tk.Label(card, text="固件", bg=CARD, fg=INK, font=(FONT, 10, "bold")).grid(
            row=1, column=0, sticky="w", padx=(0, 18), pady=(15, 0))
        ttk.Entry(card, textvariable=self.file_name, state="readonly").grid(
            row=1, column=1, sticky="ew", padx=(0, 10), pady=(15, 0))
        self.file_button = ttk.Button(card, text="选文件", command=self.choose_image)
        self.file_button.grid(row=1, column=2, sticky="ew", pady=(15, 0))
        tk.Label(card, textvariable=self.file_details, bg=CARD, fg=MUTED, anchor="w").grid(
            row=2, column=1, sticky="ew", pady=(7, 0))

        result = tk.Frame(outer, bg=BACKGROUND)
        result.pack(fill="x", pady=(21, 0))
        self.status_label = tk.Label(result, textvariable=self.status, bg=BACKGROUND, fg=INK,
                                     anchor="w", justify="left", wraplength=590)
        self.status_label.pack(fill="x", anchor="w")
        result.bind("<Configure>", lambda event: self.status_label.configure(wraplength=event.width))
        self.bar = ttk.Progressbar(result, mode="determinate", style="Upload.Horizontal.TProgressbar")
        self.bar.pack(fill="x", pady=(10, 2))
        tk.Label(result, textvariable=self.progress_label, background=BACKGROUND,
                 foreground=MUTED, anchor="w").pack(fill="x")

        actions = tk.Frame(outer, bg=BACKGROUND)
        actions.pack(side="bottom", fill="x", pady=(16, 0))
        self.upload_button = ttk.Button(actions, text="开始上传", style="Upload.TButton",
                                        command=self.start_upload)
        self.upload_button.pack(side="right")
        tk.Label(actions, text="升级前断开 PTC 电源。\n按住 KEY2，按一下 RESET，再松开 KEY2。",
                 background=BACKGROUND, foreground=MUTED, justify="left").pack(side="left")

    def show_status(self, text, color=INK):
        self.status.set(text)
        self.status_label.configure(fg=color)

    def update_controls(self):
        self.port_box.configure(state="disabled" if self.busy or not self.ports else "readonly")
        mode = "disabled" if self.busy else "normal"
        self.file_button.configure(state=mode)
        self.refresh_button.configure(state=mode)
        ready = self.image is not None and self.port_name.get() in self.ports
        self.upload_button.configure(state="normal" if ready and not self.busy else "disabled")

    def refresh_ports(self):
        try:
            ports = sorted(list_ports.comports())
        except OSError as exc:
            self.show_status(f"读取串口失败。{exc}", ERROR)
            return
        previous = self.port_name.get()
        self.ports = {f"{port.device}  ·  {port.description}": port.device for port in ports}
        self.port_box.configure(values=list(self.ports))
        preferred = next((label for label in self.ports if "CH340" in label.upper()), "")
        self.port_name.set(previous if previous in self.ports else preferred)
        self.update_controls()
        if not self.ports:
            self.show_status("未发现串口。插上 CH340 后点击刷新。", ERROR)
        elif self.port_name.get():
            self.show_status("已找到串口，准备上传。" if self.image else "请选择要上传的固件。")
        else:
            self.show_status("请选择开发板对应的串口。")

    def port_selected(self, _event):
        self.update_controls()
        self.show_status("串口已选择。" if self.image else "请选择要上传的固件。")

    def load_default_image(self):
        candidate = (Path(sys.executable).parent / "heating-app.bin" if getattr(sys, "frozen", False)
                     else ROOT / "build" / "upgrade" / "v1.3-heating" / "heating-app.bin")
        if candidate.is_file():
            self.select_image(candidate)

    def choose_image(self):
        path = filedialog.askopenfilename(parent=self.root, title="选择 APP 固件",
                                          filetypes=[("固件 BIN", "*.bin"), ("所有文件", "*.*")])
        if path:
            self.select_image(Path(path))

    def select_image(self, path):
        self.image_path = path
        self.image = None
        self.file_name.set(str(path))
        self.file_details.set("")
        self.bar.configure(value=0)
        self.progress_label.set("")
        try:
            layout = read_layout()
            with path.open("rb") as stream:
                image = stream.read(layout.metadata_base - layout.app_base + 1)
            validate_image(image)
        except (OSError, ValueError) as exc:
            self.show_status(f"固件检查失败。{exc}", ERROR)
        else:
            self.image = image
            self.file_details.set(f"{len(image):,} 字节    CRC32 {crc32(image):08X}")
            self.show_status("固件检查通过。可以开始上传。" if self.port_name.get() in self.ports
                             else "固件检查通过。请选择开发板对应的串口。")
        self.update_controls()

    def start_upload(self):
        port = self.ports.get(self.port_name.get())
        if self.busy or not port or self.image_path is None:
            return
        # 点击上传后重新读取文件，再传固定的字节快照，避免选文件后外部替换。
        self.select_image(self.image_path)
        if self.image is None:
            return
        self.busy = True
        self.show_status(f"正在打开 {port}。")
        self.bar.configure(maximum=len(self.image), value=0)
        self.progress_label.set(f"0 / {len(self.image):,} 字节")
        self.update_controls()
        threading.Thread(target=self.upload_worker, args=(port, self.image), daemon=True).start()

    def upload_worker(self, port_name, image):
        # 串口操作在后台进行，窗口更新由 Tk 主线程读取事件队列。
        try:
            with serial.Serial(port_name, BAUD_RATE, timeout=0.25, write_timeout=4) as port:
                upload(port, image, 25,
                       on_status=lambda text: self.events.put(("status", text)),
                       on_progress=lambda sent, total: self.events.put(("progress", (sent, total))))
        except (OSError, RuntimeError, ValueError) as exc:
            self.events.put(("error", str(exc)))
        else:
            self.events.put(("done", None))

    def drain_events(self):
        while True:
            try:
                kind, value = self.events.get_nowait()
            except queue.Empty:
                break
            if kind == "status":
                self.show_status(value)
            elif kind == "progress":
                sent, total = value
                self.bar.configure(value=sent)
                self.progress_label.set(f"{sent:,} / {total:,} 字节")
            else:
                self.busy = False
                self.update_controls()
                if kind == "done":
                    self.show_status("整镜像校验通过。松开 KEY2 后按一下 RESET，启动新固件。", ACCENT)
                else:
                    self.show_status(f"上传失败。{value}", ERROR)
        self.root.after(50, self.drain_events)

    def close(self):
        if self.busy:
            messagebox.showinfo("正在上传", "请等到上传结束再关闭窗口。", parent=self.root)
        else:
            self.root.destroy()


def main():
    root = tk.Tk()
    FirmwareUpdater(root)
    root.mainloop()


if __name__ == "__main__":
    main()
