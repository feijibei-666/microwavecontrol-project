"""Build separate STM32F103C8 bootloader and upgradeable APP images with ARMCC5."""

import argparse
import binascii
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
# Layout read by ImageIsValid() in bootloader/main.c.
BOOT_METADATA = struct.Struct("<IIH")
BOOT_IMAGE_VALID = 0xB007
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
    flash_page_size: int


def read_layout():
    resources = Path(getattr(sys, "_MEIPASS", ROOT))
    header = (resources / "user" / "firmware_layout.h").read_text(encoding="utf-8-sig")
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
            constants["SRAM_BASE"], constants["SRAM_TOP"], constants["FLASH_PAGE_SIZE"],
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
    export_image(tools, output, name, image, base == layout.app_base)


def export_image(tools, output, name, image, is_app):
    run(tools, "fromelf", "--i32combined", "--output", output / f"{name}.hex", image)
    if is_app:
        run(tools, "fromelf", "--bin", "--output", output / f"{name}.bin", image)


def read_hex_content(path, base, limit):
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
    if not finished:
        raise ValueError(f"{path.name}: missing end record")
    return content


def validate_vectors(path, content, base, size, layout):
    if any(offset not in content for offset in range(8)):
        raise ValueError(f"{path.name}: missing start vectors")
    msp, reset = struct.unpack("<II", bytes(content[offset] for offset in range(8)))
    if not (layout.sram_base < msp <= layout.sram_top and msp % 8 == 0
            and reset & 1 and base + 8 <= (reset & ~1) < base + size
            and (reset & ~1) - base in content):
        raise ValueError(f"{path.name}: invalid initial stack pointer or reset vector")


def validate_image(path, base, limit, layout, binary=None):
    content = read_hex_content(path, base, limit)
    size = max(content, default=-1) + 1
    validate_vectors(path, content, base, size, layout)
    if binary is not None and (len(binary) != size
                               or any(binary[offset] != value for offset, value in content.items())):
        raise ValueError("app.bin differs from the flash image in app.hex")
    return content


def ihex_record(kind, address, data):
    record = struct.pack(">BHB", len(data), address, kind) + data
    return ":" + (record + bytes([-sum(record) & 0xFF])).hex().upper()


def write_factory(path, boot, app, layout):
    # Include the complete APP BIN so CRC covers even bytes omitted by fromelf's HEX.
    boot_image = bytes(boot.get(offset, 0xFF) for offset in range(max(boot) + 1))
    metadata = BOOT_METADATA.pack(len(app), binascii.crc32(app), BOOT_IMAGE_VALID)
    records = []
    current_upper = None
    for base, image in ((layout.boot_base, boot_image),
                        (layout.app_base, app),
                        (layout.metadata_base, metadata)):
        offset = 0
        while offset < len(image):
            upper, address = divmod(base + offset, 0x10000)
            if upper != current_upper:
                records.append(ihex_record(4, 0, upper.to_bytes(2, "big")))
                current_upper = upper
            record_size = min(16, 0x10000 - address)
            records.append(ihex_record(0, address, image[offset:offset + record_size]))
            offset += record_size
    records.append(ihex_record(1, 0, b""))
    path.write_text("\n".join(records) + "\n", encoding="ascii")
    validate_factory(path, layout)


def validate_factory(path, layout=None):
    layout = layout or read_layout()
    app_offset = layout.app_base - layout.boot_base
    metadata_offset = layout.metadata_base - layout.boot_base
    content = read_hex_content(path, layout.boot_base,
                               layout.metadata_base + layout.flash_page_size)
    metadata_range = range(metadata_offset, metadata_offset + BOOT_METADATA.size)
    if any(offset not in content for offset in metadata_range):
        raise ValueError(f"{path.name}: incomplete APP metadata")
    metadata = bytes(content[offset] for offset in metadata_range)
    size, crc, marker = BOOT_METADATA.unpack(metadata)
    if marker != BOOT_IMAGE_VALID or not 8 <= size <= metadata_offset - app_offset:
        raise ValueError(f"{path.name}: invalid APP metadata")
    if any(app_offset + size <= offset < metadata_offset or
           offset >= metadata_offset + BOOT_METADATA.size for offset in content):
        raise ValueError(f"{path.name}: data outside the bootloader, APP or metadata")

    boot = {offset: value for offset, value in content.items() if offset < app_offset}
    app = {offset - app_offset: value for offset, value in content.items()
           if app_offset <= offset < app_offset + size}
    validate_vectors(path, boot, layout.boot_base, app_offset, layout)
    validate_vectors(path, app, layout.app_base, size, layout)
    if (len(app) != size or binascii.crc32(bytes(app[offset] for offset in range(size))) != crc):
        raise ValueError(f"{path.name}: incomplete APP or CRC32 mismatch")
    return size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, help="ARMCC5 installation or bin directory")
    parser.add_argument("--out-dir", type=Path, default=ROOT / "build" / "upgrade")
    parser.add_argument("--version", help="APP screen label, up to 10 ASCII characters (default: UPGRADE)")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--relay-led-test", action="store_true", help="Enable the APP's low-voltage LED test mode")
    modes.add_argument("--heating-app", action="store_true", help="Build the APP with countdown heating control")
    parser.add_argument("--factory-hex", action="store_true",
                        help="Also generate factory.hex with bootloader, heating APP and CRC32 metadata")
    parser.add_argument("--keil-app", type=Path, metavar="AXF",
                        help="Use a relocated, heating-enabled AXF built by Keil instead of rebuilding the APP")
    parser.add_argument("--verify-factory", type=Path, metavar="HEX",
                        help="Verify a factory HEX without running the toolchain")
    args = parser.parse_args()
    version = args.version or "UPGRADE"
    if args.version is not None and not re.fullmatch(r"[A-Za-z0-9_.-]{1,10}", args.version):
        parser.error("--version requires 1-10 ASCII letters, digits, dots, underscores or dashes")
    if args.factory_hex and not args.heating_app:
        parser.error("--factory-hex requires --heating-app")
    if args.keil_app is not None and not args.factory_hex:
        parser.error("--keil-app requires --factory-hex")

    try:
        layout = read_layout()
        if args.verify_factory is not None:
            size = validate_factory(args.verify_factory, layout)
            print(f"Factory HEX valid: {args.verify_factory} ({size} APP bytes)")
            return 0
        tools = toolchain_bin(args.toolchain)
        app_sources, includes, defines = read_app_project()
        app_defines = [*defines, "-DUPGRADE_APP=1", f'-DFIRMWARE_VERSION="{version}"']
        app_name = "app"
        if args.relay_led_test:
            app_defines.append("-DRELAY_LED_COUNTDOWN_TEST=1")
        elif args.heating_app:
            app_defines.append("-DPTC_HEATING_APP=1")
            app_name = "heating-app"
        destination = args.out_dir.resolve()
        names = ["bootloader.hex", f"{app_name}.hex", f"{app_name}.bin"]
        if args.factory_hex:
            names.append("factory.hex")
        with tempfile.TemporaryDirectory(prefix="microwave-upgrade-") as temporary:
            output = Path(temporary)
            build_image(tools, output, "bootloader", BOOT_SOURCES, includes,
                        [*defines, "-DUPGRADE_BOOTLOADER=1"], layout.boot_base, layout.app_base, layout)
            if args.keil_app is None:
                build_image(tools, output, app_name, app_sources, includes, app_defines,
                            layout.app_base, layout.metadata_base, layout)
            else:
                export_image(tools, output, app_name, args.keil_app.resolve(strict=True), True)
            boot = validate_image(output / "bootloader.hex", layout.boot_base, layout.app_base, layout)
            app = (output / f"{app_name}.bin").read_bytes()
            validate_image(output / f"{app_name}.hex", layout.app_base, layout.metadata_base,
                           layout, app)
            if args.keil_app is not None:
                if b"HEAT OUTPUT ON\x00" not in app:
                    raise ValueError("Keil APP is missing the heating UI; build the V1.3 Heating target")
                if args.version is not None and f"FW {version}\x00".encode("ascii") not in app:
                    raise ValueError(f"Keil APP does not report version {version}")
            if args.factory_hex:
                write_factory(output / "factory.hex", boot, app, layout)
            destination.mkdir(parents=True, exist_ok=True)
            for name in names:
                shutil.copyfile(output / name, destination / name)
        print(f"Bootloader: 0x{layout.boot_base:08X}, {max(boot) + 1} bytes")
        print(f"APP:        0x{layout.app_base:08X}, {len(app)} bytes")
        if args.factory_hex:
            print(f"Factory:    factory.hex, metadata at 0x{layout.metadata_base:08X}")
        print(f"Output:     {destination}")
        return 0
    except (OSError, ValueError, ElementTree.ParseError, subprocess.CalledProcessError) as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
