"""Package the Windows updater and optionally copy an APP beside it."""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=ROOT / "build" / "updater")
    parser.add_argument("--app", type=Path, help="Copy this BIN alongside the EXE as heating-app.bin")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("请在 Windows 上构建 EXE")
    if args.app is not None and not args.app.is_file():
        parser.error("--app 必须指向已构建的 BIN 文件")

    destination = args.out_dir.resolve()
    with tempfile.TemporaryDirectory(prefix="microwave-updater-") as temporary:
        subprocess.run([
            sys.executable, "-m", "PyInstaller", "--noconfirm", "--onefile", "--windowed",
            "--name", "MicrowaveFirmwareUpdater", "--distpath", str(destination),
            "--specpath", temporary, "--workpath", str(Path(temporary) / "work"),
            "--add-data", f"{ROOT / 'user' / 'firmware_layout.h'}:user",
            str(ROOT / "tools" / "firmware_updater.py"),
        ], check=True, cwd=ROOT)
    if args.app is not None:
        firmware = destination / "heating-app.bin"
        if args.app.resolve() != firmware.resolve():
            shutil.copyfile(args.app, firmware)
    print(destination / "MicrowaveFirmwareUpdater.exe")


if __name__ == "__main__":
    main()
