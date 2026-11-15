"""Build separate STM32F103C8 bootloader and upgradeable APP images with ARMCC5."""

import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
TOOL_NAMES = ("armcc", "armasm", "armlink", "fromelf")
BOOT_SOURCES = (
    "start/startup_stm32f10x_md.s",
    "start/core_cm3.c",
    "start/system_stm32f10x.c",
    "library/stm32f10x_gpio.c",
    "library/stm32f10x_rcc.c",
    "library/stm32f10x_flash.c",
    "library/stm32f10x_usart.c",
    "library/stm32f10x_dma.c",
    "bsp/heater.c",
    "bootloader/main.c",
    "bootloader/jump.s",
)


@dataclass(frozen=True)
class FirmwareLayout:
    boot_base: int
    app_base: int
    metadata_base: int
    sram_base: int
    sram_top: int


def read_layout():
    header = (ROOT / "user" / "firmware_layout.h").read_text(encoding="utf-8-sig")
    constants = {
        name: int(value, 16)
        for name, value in re.findall(
            r"^#define[ \t]+FIRMWARE_([A-Z_]+)[ \t]+(0x[0-9A-Fa-f]+)[uUlL]*[ \t]*$",
            header,
            flags=re.MULTILINE,
        )
    }
    try:
        boot_base = constants["FLASH_BASE"]
        return FirmwareLayout(
            boot_base, boot_base + constants["APP_OFFSET"], constants["METADATA_BASE"],
            constants["SRAM_BASE"], constants["SRAM_TOP"],
        )
    except KeyError as error:
        raise ValueError(f"Missing FIRMWARE_{error.args[0]} literal in user/firmware_layout.h") from error


def toolchain_bin(explicit):
    extension = ".exe" if os.name == "nt" else ""
    candidates = [explicit] if explicit else []
    if not explicit:
        cache = ROOT / "build" / "Target 1" / "builder.params"
        if cache.is_file():
            try:
                location = json.loads(cache.read_text(encoding="utf-8-sig"))
                if location.get("toolchainLocation"):
                    candidates.append(Path(location["toolchainLocation"]))
            except (OSError, ValueError):
                # A stale IDE cache must not prevent locating ARMCC5 on PATH.
                pass
        compiler = shutil.which("armcc" + extension)
        if compiler:
            candidates.append(Path(compiler).parent)
    for path in candidates:
        path = path.expanduser().resolve()
        for directory in (path, path / "bin"):
            tools = {name: directory / (name + extension) for name in TOOL_NAMES}
            if all(tool.is_file() for tool in tools.values()):
                return tools
    raise ValueError("ARMCC5 tools not found. Use --toolchain with the ARMCC or ARMCC/bin directory.")


def read_app_project():
    project = ElementTree.parse(ROOT / "project.uvprojx")
    target = project.find("./Targets/Target[TargetName='Target 1']")
    if target is None:
        raise ValueError("Keil Target 1 is missing")
    compiler = target.find("./TargetOption/TargetArmAds/Cads/VariousControls")
    if compiler is None:
        raise ValueError("Keil compiler settings are missing")
    includes = [
        "-I" + path.replace("\\", "/")
        for path in compiler.findtext("IncludePath", "").split(";")
        if path.strip()
    ]
    defines = [
        "-D" + name.strip()
        for name in compiler.findtext("Define", "").split(",")
        if name.strip()
    ]
    sources = [
        path
        for node in target.findall("./Groups/Group/Files/File")
        for path in [Path(node.findtext("FilePath", "").replace("\\", "/"))]
        if path.suffix.lower() in {".c", ".s"}
        and node.findtext("./FileOption/CommonProperty/IncludeInBuild") != "0"
    ]
    return sources, includes, defines


def run(tools, name, *args):
    subprocess.run([str(tools[name]), *map(str, args)], cwd=ROOT, check=True)


def build_image(tools, output, name, sources, includes, defines, base, limit, layout):
    objects = []
    for source in map(Path, sources):
        if not (ROOT / source).is_file():
            raise ValueError(f"Source file missing: {source}")
        obj = output / name / source.with_suffix(".o")
        obj.parent.mkdir(parents=True, exist_ok=True)
        if source.suffix.lower() == ".s":
            run(tools, "armasm", "--cpu", "Cortex-M3", "--apcs=interwork", "--li", "-g",
                "-o", obj, source)
        else:
            run(tools, "armcc", "-c", "--cpu", "Cortex-M3", "--apcs=interwork", "--li",
                "--c99", "-O0", "--split_sections", "--diag_suppress=1", "--diag_suppress=1295",
                "-g", *includes, *defines, "-o", obj, source)
        objects.append(obj)

    # Only the assigned image range is linkable. The metadata page belongs to the bootloader.
    scatter = output / f"{name}.sct"
    region = f"0x{base:08X} 0x{limit - base:08X}"
    scatter.write_text(
        f"LR_FLASH {region} {{\n"
        f"  ER_FLASH {region} {{\n"
        "    *.o (RESET, +First)\n"
        "    *(InRoot$$Sections)\n"
        "    .ANY (+RO)\n"
        "    .ANY (+XO)\n"
        "  }\n"
        f"  RW_RAM 0x{layout.sram_base:08X} 0x{layout.sram_top - layout.sram_base:08X}"
        " { .ANY (+RW +ZI) }\n"
        "}\n",
        encoding="ascii",
    )
    image = output / f"{name}.axf"
    response = output / f"{name}.lnp"
    response.write_text(
        "\n".join([
            f'--cpu Cortex-M3 --scatter "{scatter}" --strict --summary_stderr --info summarysizes',
            *[f'"{obj}"' for obj in objects],
            f'-o "{image}"',
        ]),
        encoding="utf-8",
    )
    run(tools, "armlink", "--via", response)
    run(tools, "fromelf", "--i32combined", "--output", output / f"{name}.hex", image)
    if name == "app":
        run(tools, "fromelf", "--bin", "--output", output / "app.bin", image)


def validate_image(path, base, limit, layout, binary=None):
    upper_address = 0
    content = {}
    finished = False
    for line in path.read_text(encoding="ascii").splitlines():
        if finished or not line.startswith(":"):
            raise ValueError(f"{path.name}: invalid Intel HEX record")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or record[0] + 5 != len(record) or sum(record) & 0xFF:
            raise ValueError(f"{path.name}: invalid Intel HEX checksum or record length")
        address = int.from_bytes(record[1:3], "big")
        kind, data = record[3], record[4:-1]
        if kind == 4 and address == 0 and len(data) == 2:
            upper_address = int.from_bytes(data, "big") << 16
        elif kind == 0:
            first = upper_address + address
            if not (base <= first < first + len(data) <= limit):
                raise ValueError(f"{path.name}: data outside its flash partition")
            for offset, value in enumerate(data, first - base):
                if offset in content:
                    raise ValueError(f"{path.name}: overlapping Intel HEX records")
                content[offset] = value
        elif kind == 1 and address == 0 and not data:
            finished = True
        elif kind != 5 or address != 0 or len(data) != 4:
            raise ValueError(f"{path.name}: unsupported Intel HEX record")
    if not finished or any(offset not in content for offset in range(8)):
        raise ValueError(f"{path.name}: missing end record or start vectors")
    msp, reset = struct.unpack("<II", bytes(content[offset] for offset in range(8)))
    if not (layout.sram_base < msp <= layout.sram_top and msp % 8 == 0
            and reset & 1 and base <= (reset & ~1) < limit and (reset & ~1) - base in content):
        raise ValueError(f"{path.name}: invalid initial stack pointer or reset vector")
    if binary is not None:
        app = binary.read_bytes()
        if len(app) != max(content) + 1 or any(app[offset] != value for offset, value in content.items()):
            raise ValueError("app.bin differs from the flash image in app.hex")
    return max(content) + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, help="ARMCC5 installation or bin directory")
    parser.add_argument("--out-dir", type=Path, default=ROOT / "build" / "upgrade")
    parser.add_argument("--version", default="UPGRADE", help="APP screen label, up to 10 ASCII characters")
    parser.add_argument("--relay-led-test", action="store_true", help="Enable the APP's low-voltage LED test mode")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,10}", args.version):
        parser.error("--version requires 1-10 ASCII letters, digits, dots, underscores or dashes")

    try:
        tools = toolchain_bin(args.toolchain)
        layout = read_layout()
        app_sources, includes, defines = read_app_project()
        app_defines = [*defines, "-DUPGRADE_APP=1", f'-DFIRMWARE_VERSION="{args.version}"']
        if args.relay_led_test:
            app_defines.append("-DRELAY_LED_COUNTDOWN_TEST=1")
        destination = args.out_dir.resolve()
        with tempfile.TemporaryDirectory(prefix="microwave-upgrade-") as temporary:
            output = Path(temporary)
            build_image(tools, output, "bootloader", BOOT_SOURCES, includes,
                        [*defines, "-DUPGRADE_BOOTLOADER=1"], layout.boot_base, layout.app_base, layout)
            build_image(tools, output, "app", app_sources, includes, app_defines,
                        layout.app_base, layout.metadata_base, layout)
            boot_size = validate_image(output / "bootloader.hex", layout.boot_base, layout.app_base, layout)
            app_size = validate_image(
                output / "app.hex", layout.app_base, layout.metadata_base, layout, output / "app.bin"
            )
            destination.mkdir(parents=True, exist_ok=True)
            for name in ("bootloader.hex", "app.hex", "app.bin"):
                shutil.copyfile(output / name, destination / name)
        print(f"Bootloader: 0x{layout.boot_base:08X}, {boot_size} bytes")
        print(f"APP:        0x{layout.app_base:08X}, {app_size} bytes")
        print(f"Output:     {destination}")
        return 0
    except (OSError, ValueError, ElementTree.ParseError, subprocess.CalledProcessError) as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
