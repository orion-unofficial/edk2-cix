#!/usr/bin/env python3
"""Source replacement for the vendor cix_package_tool."""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

MAGIC = 0x55AA55AA
HEADER_WORDS = 4
ENTRY_WORDS = 4
ENTRY_SIZE = ENTRY_WORDS * 4
DEFAULT_ALIGN_SIZE = 4096

TYPE_NAME_OVERRIDES = {
    "ec_file": "ec_firmware.bin",
    "xip_file": "brom_xip.bin",
}

HELP_TEXT = (
    "\033[1;33m    ./cix_package_tool <option>\n\n"
    "\033[m    options:\n"
    "        -h, --help         show the help information\n"
    "        -c, --config       point config file with json format\n"
    "        -o, --output       output file/path\n"
    "        -d, --dump         dump target file\n"
    "        -O, --OTA          Package OTA package\n\n"
    "    examples:\n"
    "        If you want to package a flash package, you can run:\n"
    "\033[0;32;32m            ./cix_package_tool -c yyy/xxx.json\n"
    "\033[m        If you want to package a flash package & assign output, you can run: \n"
    "\033[0;32;32m            ./cix_package_tool -c yyy/xxx.json -o outFile\n"
    "\033[m        If you want to package a OTA package, you can run:\n"
    "\033[0;32;32m            ./cix_package_tool -c yyy/xxx.json -O cix_flash_ota.bin\n"
    "\033[m        If you want to dump a target, you can run: \n"
    "\033[0;32;32m            ./cix_package_tool -d xxx -c yyy/xxx.json\n"
    "\033[m"
)


def print_help() -> None:
    print(HELP_TEXT, end="")


def fail_with_help(message: str) -> None:
    print(message, end="")
    print_help()
    raise SystemExit(255)


def parse_int(value: object) -> int:
    if isinstance(value, str):
        return int(value, 0)
    return int(value)


def trim_fixed_region(data: bytes) -> bytes:
    end = len(data)
    while end > 0 and data[end - 1] in (0x00, 0xFF):
        end -= 1
    return data[:end]


@dataclass(frozen=True)
class ImageEntry:
    image_type: int
    address: int
    reserved_size: int
    file_path: Path
    data: bytes

    @property
    def actual_size(self) -> int:
        return len(self.data)


@dataclass(frozen=True)
class Config:
    path: Path
    cfs_version: int
    fip_version: int | None
    align_size: int
    flash_size: int
    image_count: int
    image_entries: tuple[ImageEntry, ...]
    firmware_header_addr: int | None = None
    ec_addr: int | None = None
    sec_debug_addr: int | None = None
    xip_addr: int | None = None
    ec_file: Path | None = None
    sec_debug_file: Path | None = None
    xip_file: Path | None = None
    ota_flags: int = 0

    @property
    def sorted_entries(self) -> tuple[ImageEntry, ...]:
        return tuple(sorted(self.image_entries, key=lambda entry: entry.address))

    @property
    def is_full_flash(self) -> bool:
        return self.firmware_header_addr is not None


def resolve_optional_file(base_dir: Path, raw_path: object) -> Path | None:
    if not raw_path:
        return None
    return base_dir / str(raw_path)


def load_config(config_path: Path, *, load_payloads: bool = True) -> Config:
    try:
        raw = json.loads(config_path.read_text())
    except FileNotFoundError:
        fail_with_help(f"\033[1;31m[src/cix_file_oper.c:154] \033[mOpen File({config_path}) fail\n")
    except json.JSONDecodeError:
        fail_with_help(f"\033[1;31m[src/cix_json.c:230] \033[mLoad json file({config_path}) failed.\n")
    base_dir = config_path.parent
    entries = []
    for item in raw["image_header_groups"]:
        file_path = base_dir / item["file"]
        data = file_path.read_bytes() if load_payloads else b""
        entries.append(
            ImageEntry(
                image_type=parse_int(item["image_type"]),
                address=parse_int(item["address"]),
                reserved_size=parse_int(item["size"]),
                file_path=file_path,
                data=data,
            )
        )

    return Config(
        path=config_path,
        cfs_version=parse_int(raw["cfs_version"]),
        fip_version=parse_int(raw["fip_version"]) if "fip_version" in raw else None,
        align_size=parse_int(raw.get("align_size", DEFAULT_ALIGN_SIZE)),
        flash_size=parse_int(raw["flash_size"]),
        image_count=parse_int(raw["image_count"]),
        image_entries=tuple(entries),
        firmware_header_addr=parse_int(raw["firmware_header_addr"]) if "firmware_header_addr" in raw else None,
        ec_addr=parse_int(raw["ec_addr"]) if "ec_addr" in raw else None,
        sec_debug_addr=parse_int(raw["sec_debug_addr"]) if "sec_debug_addr" in raw else None,
        xip_addr=parse_int(raw["xip_addr"]) if "xip_addr" in raw else None,
        ec_file=resolve_optional_file(base_dir, raw.get("ec_file")),
        sec_debug_file=resolve_optional_file(base_dir, raw.get("sec_debug_file")),
        xip_file=resolve_optional_file(base_dir, raw.get("xip_file")),
        ota_flags=parse_int(raw.get("ota_flags", 0)),
    )


def validate_entries(entries: tuple[ImageEntry, ...]) -> None:
    seen_types: dict[int, ImageEntry] = {}
    for entry in entries:
        if entry.actual_size > entry.reserved_size:
            raise SystemExit(
                f"image_type {entry.image_type} exceeds reserved size: "
                f"{entry.actual_size:#x} > {entry.reserved_size:#x}"
            )
        if entry.image_type in seen_types:
            other = seen_types[entry.image_type]
            raise SystemExit(
                f"duplicate image_type {entry.image_type} at "
                f"{other.file_path} and {entry.file_path}"
            )
        seen_types[entry.image_type] = entry


def entry_data_for_full_flash(config: Config, entry: ImageEntry) -> bytes:
    data = entry.data
    if entry.image_type == 2 and config.fip_version is not None:
        if len(data) < 8:
            raise SystemExit("bootloader2.img is too small to carry the FIP version field")
        mutable = bytearray(data)
        struct.pack_into("<I", mutable, 4, config.fip_version)
        return bytes(mutable)
    return data


def build_full_flash(config: Config) -> bytes:
    assert config.firmware_header_addr is not None
    assert config.ec_addr is not None
    assert config.xip_addr is not None
    validate_entries(config.image_entries)

    max_end = config.firmware_header_addr + HEADER_WORDS * 4 + config.image_count * ENTRY_SIZE
    fixed_writes: list[tuple[int, bytes]] = []
    for addr, file_path in (
        (config.ec_addr, config.ec_file),
        (config.sec_debug_addr, config.sec_debug_file),
        (config.xip_addr, config.xip_file),
    ):
        if addr is None or file_path is None:
            continue
        data = file_path.read_bytes()
        fixed_writes.append((addr, data))
        max_end = max(max_end, addr + len(data))

    entry_payloads = {
        entry: entry_data_for_full_flash(config, entry) for entry in config.image_entries
    }

    for entry in config.image_entries:
        max_end = max(max_end, entry.address + len(entry_payloads[entry]))

    output = bytearray(max_end)
    for addr, data in fixed_writes:
        output[addr : addr + len(data)] = data

    for entry in config.image_entries:
        data = entry_payloads[entry]
        output[entry.address : entry.address + len(data)] = data

    struct.pack_into(
        "<4I",
        output,
        config.firmware_header_addr,
        MAGIC,
        config.cfs_version,
        config.image_count,
        0,
    )
    for index, entry in enumerate(config.sorted_entries):
        struct.pack_into(
            "<4I",
            output,
            config.firmware_header_addr + HEADER_WORDS * 4 + index * ENTRY_SIZE,
            entry.image_type,
            entry.address,
            len(entry_payloads[entry]),
            0,
        )
    return bytes(output)


def build_ota_flash(config: Config) -> bytes:
    validate_entries(config.image_entries)
    entries = config.sorted_entries
    header_size = HEADER_WORDS * 4 + len(entries) * ENTRY_SIZE
    payload = bytearray(header_size + sum(entry.actual_size for entry in entries))
    struct.pack_into(
        "<4I",
        payload,
        0,
        MAGIC,
        config.cfs_version,
        config.image_count,
        config.ota_flags,
    )
    cursor = header_size
    for index, entry in enumerate(entries):
        struct.pack_into(
            "<4I",
            payload,
            HEADER_WORDS * 4 + index * ENTRY_SIZE,
            entry.image_type,
            entry.address,
            entry.actual_size,
            0,
        )
        payload[cursor : cursor + entry.actual_size] = entry.data
        cursor += entry.actual_size
    padded_len = max(cursor, config.align_size + HEADER_WORDS * 4)
    if len(payload) < padded_len:
        payload.extend(b"\x00" * (padded_len - len(payload)))
    return bytes(payload)


def dump_full_flash(config: Config, blob: bytes, output_dir: Path) -> None:
    assert config.firmware_header_addr is not None
    assert config.ec_addr is not None
    assert config.xip_addr is not None
    output_dir.mkdir(parents=True, exist_ok=True)

    magic, version, count, _flags = struct.unpack_from("<4I", blob, config.firmware_header_addr)
    if magic != MAGIC:
        raise SystemExit("wrong binary, can't find firmware header part")
    if version != config.cfs_version:
        raise SystemExit(f"unexpected cfs_version {version:#x}")

    actual_sizes: dict[int, int] = {}
    for index in range(count):
        image_type, _addr, actual_size, _reserved = struct.unpack_from(
            "<4I",
            blob,
            config.firmware_header_addr + HEADER_WORDS * 4 + index * ENTRY_SIZE,
        )
        actual_sizes[image_type] = actual_size

    if config.ec_file is not None:
        ec_end = config.firmware_header_addr
        ec_data = blob[config.ec_addr:ec_end]
        (output_dir / TYPE_NAME_OVERRIDES["ec_file"]).write_bytes(ec_data)

    fixed_limits = [entry.address for entry in config.sorted_entries if entry.address > config.xip_addr]
    if fixed_limits:
        xip_end = min(fixed_limits)
    else:
        xip_end = len(blob)
    if config.xip_file is not None:
        xip_data = trim_fixed_region(blob[config.xip_addr:xip_end])
        (output_dir / TYPE_NAME_OVERRIDES["xip_file"]).write_bytes(xip_data)

    for entry in config.sorted_entries:
        data = blob[entry.address : entry.address + actual_sizes.get(entry.image_type, 0)]
        (output_dir / entry.file_path.name).write_bytes(data)


def dump_ota_flash(config: Config, blob: bytes, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    magic, version, count, _flags = struct.unpack_from("<4I", blob, 0)
    if magic != MAGIC:
        raise SystemExit("wrong binary, can't find firmware header part")
    if version != config.cfs_version:
        raise SystemExit(f"unexpected cfs_version {version:#x}")

    cursor = HEADER_WORDS * 4 + count * ENTRY_SIZE
    entries = sorted(config.image_entries, key=lambda entry: entry.address)
    for index, entry in enumerate(entries):
        _image_type, _addr, actual_size, _reserved = struct.unpack_from(
            "<4I",
            blob,
            HEADER_WORDS * 4 + index * ENTRY_SIZE,
        )
        data = blob[cursor : cursor + actual_size]
        (output_dir / entry.file_path.name).write_bytes(data)
        cursor += actual_size


def full_flash_header_entries(config: Config, blob: bytes) -> list[tuple[int, int, int]]:
    assert config.firmware_header_addr is not None
    _magic, _version, count, _flags = struct.unpack_from("<4I", blob, config.firmware_header_addr)
    entries = []
    for index in range(count):
        image_type, address, actual_size, _reserved = struct.unpack_from(
            "<4I",
            blob,
            config.firmware_header_addr + HEADER_WORDS * 4 + index * ENTRY_SIZE,
        )
        entries.append((image_type, address, actual_size))
    return entries


def parse_args(argv: list[str]) -> argparse.Namespace:
    if not argv:
        fail_with_help("Please input right parameter\n")

    if any(arg in ("-h", "--help") for arg in argv):
        print_help()
        raise SystemExit(0)

    for index, arg in enumerate(argv):
        if arg.startswith("--") and arg not in {"--config", "--output", "--dump", "--OTA"}:
            print(f"./cix_package_tool: unrecognized option '{arg}'", file=sys.stderr)
            fail_with_help("\033[1;31m[src/main.c:140] \033[munknown option : \x00\n")
        if arg in {"-c", "--config", "-o", "--output", "-d", "--dump", "-O", "--OTA"}:
            if index + 1 >= len(argv) or argv[index + 1].startswith("-"):
                option = arg[1] if arg.startswith("-") and not arg.startswith("--") else arg
                print(f"./cix_package_tool: option requires an argument -- '{option}'", file=sys.stderr)
                fail_with_help(f"\033[1;31m[src/main.c:140] \033[munknown option : {option}\n")

    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-c", "--config", help="JSON configuration file")
    parser.add_argument("-o", "--output", help="Package a flash image and write this output path")
    parser.add_argument("-O", "--OTA", dest="ota_output", help="Package an OTA image and write this output path")
    parser.add_argument("-d", "--dump", help="Dump the target flash image into ./unpack")
    try:
        args, extras = parser.parse_known_args(argv)
    except SystemExit:
        print_help()
        raise SystemExit(255)
    if extras or not args.config:
        print_help()
        raise SystemExit(255)
    modes = sum(bool(value) for value in (args.output, args.ota_output, args.dump))
    if modes != 1:
        print_help()
        raise SystemExit(255)
    return args


def print_full_flash_success(config: Config, output_arg: str) -> None:
    if config.sec_debug_file is None:
        print("\033[1;32m[src/cix_json.c:332] \033[mSkip package secure debug fw")
    bootloader1 = next((entry for entry in config.image_entries if entry.image_type == 1), None)
    if bootloader1 is not None:
        print(f"\033[1;32m[src/cix_package_tool.c:298] \033[mImage address: 0x{bootloader1.address:x}")
    print("====================================================")
    print("Package flash binary successful, it name was:")
    print(output_arg)
    print("====================================================")


def print_ota_success(output_arg: str) -> None:
    print("====================================================")
    print("Package flash binary for OTA successful, it name was:")
    print(output_arg)
    print("====================================================")


def print_dump_summary(config: Config, blob: bytes, output_dir: Path) -> None:
    if not config.is_full_flash:
        for path in sorted(output_dir.iterdir()):
            if path.is_file():
                print(f"Save file: {path}")
        return

    assert config.firmware_header_addr is not None
    assert config.ec_addr is not None
    assert config.xip_addr is not None

    if config.sec_debug_file is None:
        print("\033[1;32m[src/cix_json.c:332] \033[mSkip package secure debug fw")

    magic, version, count, _flags = struct.unpack_from("<4I", blob, config.firmware_header_addr)
    header_entries = full_flash_header_entries(config, blob)
    entry_by_type = {entry.image_type: entry for entry in config.image_entries}
    bootloader1 = entry_by_type.get(1)
    first_entry_addr = min((address for _image_type, address, _size in header_entries), default=config.firmware_header_addr)
    header_len = first_entry_addr - config.firmware_header_addr + HEADER_WORDS * 4
    ec_len = config.firmware_header_addr - config.ec_addr
    xip_len = (config.firmware_header_addr - config.xip_addr + config.align_size) & 0xFFFFFFFF

    print("=============================================")
    print("Fixed Firmware Address in target image was:")
    print(f"         EC addr: 0x{config.ec_addr:08x};        EC len: 0x{ec_len:08x}")
    print(f" Img header addr: 0x{config.firmware_header_addr:08x};    Header Len: 0x{header_len:08x}")
    print(f"        XIP addr: 0x{config.xip_addr:08x};       XIP Len: 0x{xip_len:08x}")
    if bootloader1 is not None:
        print(f"Bootloader1 addr: 0x{bootloader1.address:08x};")
    print("=============================================")
    print("Detail info about sub images just like follow:")
    print(f"Magic: 0x{magic:x}")
    print(f"CFS_version: 0x{version:x}")
    print(f"Count of sub iamges: 0x{count:x}")
    print("=============================================")

    for index, (image_type, address, actual_size) in enumerate(header_entries, start=1):
        entry = entry_by_type.get(image_type)
        image_name = entry.file_path.name if entry is not None else ""
        print(f"Sub_image [{index}]:")
        print(f"Image Type: {image_type}")
        print(f"Image Name: {image_name}")
        print(f"Image Addr: 0x{address:x}({address})")
        print(f"Image Size: 0x{actual_size:x}({actual_size})")
        print()

    save_paths: list[Path] = []
    if config.ec_file is not None:
        save_paths.append(output_dir / TYPE_NAME_OVERRIDES["ec_file"])
    if config.xip_file is not None:
        save_paths.append(output_dir / TYPE_NAME_OVERRIDES["xip_file"])
    save_paths.extend(output_dir / entry_by_type[image_type].file_path.name for image_type, _address, _size in header_entries)
    for path in save_paths:
        print(f"Save file: {path}")
        if path.name == TYPE_NAME_OVERRIDES["xip_file"]:
            print(f"\033[1;31m[src/cix_file_oper.c:185] \033[mWrite data into file({path}) fail")


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    config = load_config(Path(args.config), load_payloads=not bool(args.dump))

    if args.output:
        Path(args.output).write_bytes(build_full_flash(config))
        print_full_flash_success(config, args.output)
        return 0
    if args.ota_output:
        Path(args.ota_output).write_bytes(build_ota_flash(config))
        print_ota_success(args.ota_output)
        return 0

    dump_path = Path(args.dump)
    blob = dump_path.read_bytes()
    output_dir = Path.cwd() / "unpack"
    if config.is_full_flash:
        dump_full_flash(config, blob, output_dir)
    else:
        dump_ota_flash(config, blob, output_dir)
    print_dump_summary(config, blob, output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
