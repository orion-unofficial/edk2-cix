#!/usr/bin/env python3
"""Read-only probe tool for the Radxa Orion O6 ITE5570 EC.

This tool intentionally stays on the non-destructive side of the protocol for
its built-in survey paths:

- It only issues commands that are read-only in the local UEFI sources.
- It uses the Linux i2c-dev userspace interface with I2C_RDWR so the request
  and response happen as a combined write-then-read transaction.
- It does not send write/update/reset commands.
- It does not read the ACPI event register by default, because that may
  acknowledge or clear pending events.

An expert-only `call-acpi` passthrough is also available once `/proc/acpi/call`
exists. That subcommand will invoke exactly the ACPI method expression provided
by the user, so it should only be used with understood-to-be-safe methods.

The wire format and command selection come from:
  - Platform/CIX/Sky1/Include/Library/EcLib.h
  - Platform/CIX/Sky1/Library/Ite5570EcLib/Ite5570EcFunction.c
  - Platform/Radxa/Orion/O6/Drivers/AcpiPlatfomTables/EC.asl
"""

from __future__ import annotations

import argparse
import ctypes
import errno
import fcntl
import json
import os
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable


EC_PROTOCOL = 0xDA
EC_REQUEST_VERSION = 0x03

I2C_M_RD = 0x0001
I2C_RDWR = 0x0707

EC_RES_SUCCESS = 0
CROS_EC_DEV_IOCXCMD = 0xC014EC00
CROS_EC_COMMAND_HEADER = struct.Struct("<5I")
CROS_EC_DEV_NAMES = (
    "cros_ec",
    "cros_fp",
    "cros_ish",
    "cros_pd",
    "cros_scp",
    "cros_tp",
)

EC_BATT_FLAG_AC_PRESENT = 0x01
EC_BATT_FLAG_BATT_PRESENT = 0x02
EC_BATT_FLAG_DISCHARGING = 0x04
EC_BATT_FLAG_CHARGING = 0x08
EC_BATT_FLAG_LEVEL_CRITICAL = 0x10
EC_BATT_FLAG_INVALID_DATA = 0x20

EC_CMD_BATTERY_GET_STATIC = 0x0600
EC_CMD_BATTERY_GET_DYNAMIC = 0x0601
EC_CMD_PWM_GET_FAN_TARGET_RPM = 0x0020
EC_CMD_PWM_GET_DUTY = 0x0026
EC_CMD_GET_BOARD_ID = 0x3E01
EC_CMD_GET_CHARGER_INFO = 0x3E02
EC_CMD_GET_PMIC_VERSION = 0x3E03
EC_CMD_GET_GREENPAK_VERSION = 0x3E06
EC_CMD_GET_PD_VERSION = 0x3E07
EC_CMD_GET_PWROFFRSN = 0x3E09
EC_CMD_GET_FARM_ID = 0x3E0A
EC_CMD_GET_PVT_TEMP = 0x3E0C
EC_CMD_GET_4S_FORCE_SHD_EVT = 0x3E23
EC_CMD_GET_EC_VERSION = 0x3FFF


class ProbeError(RuntimeError):
    """Base error for probe failures."""


class EcProtocolError(ProbeError):
    """The EC replied with a malformed or non-success response."""


def checksum_with_hole(payload: bytes, hole_index: int) -> int:
    total = 0
    for index, value in enumerate(payload):
        if index == hole_index:
            continue
        total += value
    return (-total) & 0xFF


def make_request(command: int, payload: bytes = b"", *, command_version: int = 0) -> bytes:
    inner = bytearray(8 + len(payload))
    inner[0] = EC_REQUEST_VERSION
    inner[1] = 0
    inner[2:4] = command.to_bytes(2, "big")
    inner[4] = command_version & 0xFF
    inner[5] = 0
    inner[6:8] = len(payload).to_bytes(2, "big")
    inner[8:] = payload
    inner[1] = checksum_with_hole(inner, 1)
    return bytes([EC_PROTOCOL]) + bytes(inner)


@dataclass(frozen=True)
class EcResponse:
    transport_result: int
    packet_length: int
    struct_version: int
    result: int
    data: bytes
    raw: bytes


def parse_response(raw: bytes) -> EcResponse:
    if len(raw) < 10:
        raise EcProtocolError(f"response too short: expected at least 10 bytes, got {len(raw)}")

    transport_result = raw[0]
    packet_length = raw[1]
    if packet_length != len(raw):
        raise EcProtocolError(
            f"response packet length mismatch: header says {packet_length}, read {len(raw)}"
        )

    inner = raw[2:]
    struct_version = inner[0]
    checksum = inner[1]
    result = int.from_bytes(inner[2:4], "big")
    data_length = int.from_bytes(inner[4:6], "big")
    data = inner[8:]

    if data_length != len(data):
        raise EcProtocolError(
            f"response data length mismatch: header says {data_length}, payload is {len(data)}"
        )

    expected_checksum = checksum_with_hole(inner, 1)
    if checksum != expected_checksum:
        raise EcProtocolError(
            f"response checksum mismatch: got 0x{checksum:02x}, expected 0x{expected_checksum:02x}"
        )

    if result != EC_RES_SUCCESS:
        raise EcProtocolError(f"EC command failed with result code {result}")

    return EcResponse(
        transport_result=transport_result,
        packet_length=packet_length,
        struct_version=struct_version,
        result=result,
        data=data,
        raw=raw,
    )


def strip_ascii(data: bytes) -> str:
    return data.split(b"\x00", 1)[0].decode("ascii", errors="replace").rstrip()


def decode_board_id(data: bytes) -> dict[str, Any]:
    if len(data) != 2:
        raise EcProtocolError(f"board-id payload should be 2 bytes, got {len(data)}")
    value = int.from_bytes(data, "big")
    sku = value & 0x7
    memory = (value >> 3) & 0x7
    sku_ext = (value >> 6) & 0x1
    mem_ext = (value >> 7) & 0x1
    rev = (value >> 8) & 0x3
    return {
        "raw": value,
        "raw_hex": f"0x{value:04x}",
        "sku": sku + (sku_ext << 3),
        "memory": memory + (mem_ext << 3),
        "revision": rev,
    }


def decode_battery_static(data: bytes) -> dict[str, Any]:
    if len(data) != 40:
        raise EcProtocolError(f"battery-static payload should be 40 bytes, got {len(data)}")
    design_capacity, design_voltage, manufacturer, model, serial, kind, cycle_count = struct.unpack(
        ">HH8s8s8s8sI", data
    )
    return {
        "design_capacity_mAh": design_capacity,
        "design_voltage_mV": design_voltage,
        "manufacturer": strip_ascii(manufacturer),
        "model": strip_ascii(model),
        "serial": strip_ascii(serial),
        "type": strip_ascii(kind),
        "cycle_count": cycle_count,
    }


def decode_battery_dynamic(data: bytes) -> dict[str, Any]:
    if len(data) != 14:
        raise EcProtocolError(f"battery-dynamic payload should be 14 bytes, got {len(data)}")
    values = struct.unpack(">7H", data)
    flags = values[4]
    flag_names = []
    for bit, label in (
        (EC_BATT_FLAG_AC_PRESENT, "ac_present"),
        (EC_BATT_FLAG_BATT_PRESENT, "battery_present"),
        (EC_BATT_FLAG_DISCHARGING, "discharging"),
        (EC_BATT_FLAG_CHARGING, "charging"),
        (EC_BATT_FLAG_LEVEL_CRITICAL, "critical"),
        (EC_BATT_FLAG_INVALID_DATA, "invalid_data"),
    ):
        if flags & bit:
            flag_names.append(label)
    return {
        "actual_voltage_mV": values[0],
        "actual_current_mA": values[1],
        "remaining_capacity_mAh": values[2],
        "full_capacity_mAh": values[3],
        "flags": flags,
        "flag_names": flag_names,
        "desired_voltage_mV": values[5],
        "desired_current_mA": values[6],
    }


def decode_farm_id(data: bytes) -> dict[str, Any]:
    if len(data) != 1:
        raise EcProtocolError(f"farm-id payload should be 1 byte, got {len(data)}")
    return {"farm_id": data[0], "farm_id_hex": f"0x{data[0]:02x}"}


def decode_pmic_version(data: bytes) -> dict[str, Any]:
    if len(data) != 3:
        raise EcProtocolError(f"pmic-version payload should be 3 bytes, got {len(data)}")
    return {
        "pmic_versions_raw": list(data),
        "pmic_versions_hex": [f"0x{value:02x}" for value in data],
    }


def decode_pd_version(data: bytes) -> dict[str, Any]:
    if len(data) != 4:
        raise EcProtocolError(f"pd-version payload should be 4 bytes, got {len(data)}")
    pd1, pd2 = struct.unpack(">HH", data)
    return {
        "pd1_raw": pd1,
        "pd1_version": f"{pd1 & 0xFF}.{(pd1 >> 8) & 0xFF}",
        "pd2_raw": pd2,
        "pd2_version": f"{pd2 & 0xFF}.{(pd2 >> 8) & 0xFF}",
        "display_order": [f"{pd2 & 0xFF}.{(pd2 >> 8) & 0xFF}", f"{pd1 & 0xFF}.{(pd1 >> 8) & 0xFF}"],
    }


def decode_greenpak_version(data: bytes) -> dict[str, Any]:
    if len(data) != 1:
        raise EcProtocolError(f"greenpak-version payload should be 1 byte, got {len(data)}")
    return {"greenpak_version": data[0], "greenpak_version_hex": f"0x{data[0]:02x}"}


def decode_charger_info(data: bytes) -> dict[str, Any]:
    if len(data) != 11:
        raise EcProtocolError(f"charger-info payload should be 11 bytes, got {len(data)}")
    name, charger_type, online = struct.unpack(">7s3sB", data)
    return {
        "name": strip_ascii(name),
        "type": strip_ascii(charger_type),
        "online": online,
        "online_bool": bool(online),
    }


def decode_fan_rpm(data: bytes) -> dict[str, Any]:
    if len(data) != 4:
        raise EcProtocolError(f"fan-rpm payload should be 4 bytes, got {len(data)}")
    rpm = int.from_bytes(data, "big")
    return {"fan_target_rpm": rpm}


def decode_pwm_duty(data: bytes) -> dict[str, Any]:
    if len(data) != 2:
        raise EcProtocolError(f"pwm-duty payload should be 2 bytes, got {len(data)}")
    duty = int.from_bytes(data, "big")
    return {"pwm_duty_percent": duty}


def decode_pvt_temp(data: bytes) -> dict[str, Any]:
    if len(data) != 2:
        raise EcProtocolError(f"pvt-temp payload should be 2 bytes, got {len(data)}")
    temp_h, temp_l = data
    return {
        "integral_celsius": temp_h,
        "fractional_centicelsius": temp_l,
        "celsius": temp_h + (temp_l / 100.0),
        "deci_kelvin_for_acpi": (temp_h * 10) + (temp_l // 10) + 2732,
    }


def decode_poweroff_reason(data: bytes) -> dict[str, Any]:
    if len(data) != 1:
        raise EcProtocolError(f"poweroff-reason payload should be 1 byte, got {len(data)}")
    return {"poweroff_reason": data[0], "poweroff_reason_hex": f"0x{data[0]:02x}"}


def decode_ec_version(data: bytes) -> dict[str, Any]:
    return {"ec_version": strip_ascii(data)}


@dataclass(frozen=True)
class ReadOnlyCommand:
    name: str
    code: int
    request_payload: bytes
    response_data_length: int
    decoder: Callable[[bytes], dict[str, Any]]
    description: str

    @property
    def response_packet_length(self) -> int:
        return 10 + self.response_data_length


BASIC_COMMANDS: tuple[ReadOnlyCommand, ...] = (
    ReadOnlyCommand("ec_version", EC_CMD_GET_EC_VERSION, b"", 19, decode_ec_version, "Read the EC version string."),
    ReadOnlyCommand("board_id", EC_CMD_GET_BOARD_ID, b"", 2, decode_board_id, "Read the board ID bitfield."),
    ReadOnlyCommand(
        "battery_static_0",
        EC_CMD_BATTERY_GET_STATIC,
        b"\x00",
        40,
        decode_battery_static,
        "Read battery-0 static information.",
    ),
    ReadOnlyCommand(
        "battery_dynamic_0",
        EC_CMD_BATTERY_GET_DYNAMIC,
        b"\x00",
        14,
        decode_battery_dynamic,
        "Read battery-0 dynamic information.",
    ),
    ReadOnlyCommand("pmic_version", EC_CMD_GET_PMIC_VERSION, b"", 3, decode_pmic_version, "Read PMIC versions."),
    ReadOnlyCommand("pd_version", EC_CMD_GET_PD_VERSION, b"", 4, decode_pd_version, "Read PD controller versions."),
    ReadOnlyCommand(
        "greenpak_version",
        EC_CMD_GET_GREENPAK_VERSION,
        b"",
        1,
        decode_greenpak_version,
        "Read the GreenPak version byte.",
    ),
    ReadOnlyCommand(
        "charger_info",
        EC_CMD_GET_CHARGER_INFO,
        b"",
        11,
        decode_charger_info,
        "Read charger name/type/online state.",
    ),
    ReadOnlyCommand(
        "fan_target_rpm",
        EC_CMD_PWM_GET_FAN_TARGET_RPM,
        b"",
        4,
        decode_fan_rpm,
        "Read the EC target RPM value for the fan.",
    ),
    ReadOnlyCommand(
        "pwm_duty_0_0",
        EC_CMD_PWM_GET_DUTY,
        b"\x00\x00",
        2,
        decode_pwm_duty,
        "Read PWM duty for type 0 / index 0, matching the O6 ACPI method.",
    ),
    ReadOnlyCommand("pvt_temp", EC_CMD_GET_PVT_TEMP, b"", 2, decode_pvt_temp, "Read the EC temperature sample."),
)

EXTRA_READS: tuple[ReadOnlyCommand, ...] = (
    ReadOnlyCommand("farm_id", EC_CMD_GET_FARM_ID, b"", 1, decode_farm_id, "Read the farm identifier byte."),
    ReadOnlyCommand(
        "poweroff_reason",
        EC_CMD_GET_PWROFFRSN,
        b"",
        1,
        decode_poweroff_reason,
        "Read the EC power-off reason byte.",
    ),
)


@dataclass(frozen=True)
class ReadOnlyAcpiMethod:
    name: str
    method: str
    args: tuple[str, ...]
    decoder: Callable[[str], dict[str, Any]]
    description: str

    @property
    def expression(self) -> str:
        return " ".join((self.method, *self.args)).strip()


def parse_acpi_call_integer(raw: str) -> int:
    text = raw.replace("\x00", "").replace("\r", "").strip()
    if not text or text == "not called":
        raise ProbeError("acpi_call did not report a result")
    if text.startswith("Error:"):
        raise ProbeError(f"acpi_call failed: {text}")
    try:
        return int(text, 0)
    except ValueError:
        pass
    raise ProbeError(f"unexpected integer result from acpi_call: {text}")


def decode_acpi_tmp(raw: str) -> dict[str, Any]:
    value = parse_acpi_call_integer(raw)
    return {
        "raw": raw.strip(),
        "deci_kelvin": value,
        "celsius": (value - 2732) / 10.0,
        "millidegree_celsius": (value - 2732) * 100,
    }


def decode_acpi_pwm(raw: str) -> dict[str, Any]:
    value = parse_acpi_call_integer(raw)
    return {
        "raw": raw.strip(),
        "pwm_duty_percent": value,
    }


def decode_acpi_power_resource_sta(raw: str) -> dict[str, Any]:
    value = parse_acpi_call_integer(raw)
    return {
        "raw": raw.strip(),
        "status": value,
        "is_on": bool(value),
    }


def decode_acpi_device_sta(raw: str) -> dict[str, Any]:
    value = parse_acpi_call_integer(raw)
    return {
        "raw": raw.strip(),
        "status": value,
        "status_hex": f"0x{value:x}",
        "present": bool(value & 0x01),
        "enabled": bool(value & 0x02),
        "show_in_ui": bool(value & 0x04),
        "functioning": bool(value & 0x08),
        "battery_present": bool(value & 0x10),
    }


ACPI_READS: tuple[ReadOnlyAcpiMethod, ...] = (
    ReadOnlyAcpiMethod(
        "ectz_tmp",
        r"\_SB.ECTZ._TMP",
        (),
        decode_acpi_tmp,
        "Read the EC-backed ACPI thermal-zone temperature.",
    ),
    ReadOnlyAcpiMethod(
        "hwmn_gfpw",
        r"\_SB.HWMN.GFPW",
        ("0", "0"),
        decode_acpi_pwm,
        "Read fan PWM through the CIXHA024 hardware-monitor wrapper.",
    ),
    ReadOnlyAcpiMethod(
        "ecfn_sta",
        r"\_SB.ECFN._STA",
        (),
        decode_acpi_power_resource_sta,
        "Read the ACPI fan power-resource on/off state.",
    ),
    ReadOnlyAcpiMethod(
        "ec0_gfpw",
        r"\_SB.EC0.GFPW",
        (),
        decode_acpi_pwm,
        "Read fan PWM directly through the CIXHA015 EC wrapper.",
    ),
    ReadOnlyAcpiMethod(
        "hwmn_sta",
        r"\_SB.HWMN._STA",
        (),
        decode_acpi_device_sta,
        "Read the CIXHA024 device status bitmap.",
    ),
    ReadOnlyAcpiMethod(
        "ec0_sta",
        r"\_SB.EC0._STA",
        (),
        decode_acpi_device_sta,
        "Read the CIXHA015 device status bitmap.",
    ),
)


class I2CMsg(ctypes.Structure):
    _fields_ = [
        ("addr", ctypes.c_uint16),
        ("flags", ctypes.c_uint16),
        ("len", ctypes.c_uint16),
        ("buf", ctypes.c_void_p),
    ]


class I2CRdwrIoctlData(ctypes.Structure):
    _fields_ = [
        ("msgs", ctypes.POINTER(I2CMsg)),
        ("nmsgs", ctypes.c_uint32),
    ]


class LinuxI2CTransport:
    def __init__(self, devnode: Path) -> None:
        self.devnode = devnode
        self.fd: int | None = None

    def __enter__(self) -> "LinuxI2CTransport":
        try:
            self.fd = os.open(self.devnode, os.O_RDWR)
        except FileNotFoundError as exc:
            raise ProbeError(f"{self.devnode}: device node not found") from exc
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def transfer(self, address: int, request: bytes, response_length: int) -> bytes:
        if self.fd is None:
            raise ProbeError("I2C device not open")

        write_buffer = (ctypes.c_ubyte * len(request))(*request)
        read_buffer = (ctypes.c_ubyte * response_length)()
        messages = (I2CMsg * 2)()
        messages[0] = I2CMsg(
            addr=address,
            flags=0,
            len=len(request),
            buf=ctypes.addressof(write_buffer),
        )
        messages[1] = I2CMsg(
            addr=address,
            flags=I2C_M_RD,
            len=response_length,
            buf=ctypes.addressof(read_buffer),
        )
        ioctl_data = I2CRdwrIoctlData(
            msgs=ctypes.cast(messages, ctypes.POINTER(I2CMsg)),
            nmsgs=2,
        )

        try:
            fcntl.ioctl(self.fd, I2C_RDWR, ioctl_data)
        except OSError as exc:
            if exc.errno in (errno.EACCES, errno.EPERM):
                raise ProbeError(f"{self.devnode}: permission denied; root or i2c group access may be required") from exc
            raise ProbeError(f"{self.devnode}: I2C_RDWR failed: {exc}") from exc

        return bytes(read_buffer)


class LinuxCrosEcTransport:
    def __init__(self, devnode: Path) -> None:
        self.devnode = devnode
        self.fd: int | None = None

    def __enter__(self) -> "LinuxCrosEcTransport":
        try:
            self.fd = os.open(self.devnode, os.O_RDWR)
        except FileNotFoundError as exc:
            raise ProbeError(f"{self.devnode}: device node not found") from exc
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def transfer(self, command: int, payload: bytes, response_length: int, *, version: int = 0) -> dict[str, Any]:
        if self.fd is None:
            raise ProbeError("cros_ec device not open")

        buffer_size = CROS_EC_COMMAND_HEADER.size + max(len(payload), response_length)
        ioctl_buffer = bytearray(buffer_size)
        CROS_EC_COMMAND_HEADER.pack_into(
            ioctl_buffer,
            0,
            version,
            command,
            len(payload),
            response_length,
            0,
        )
        ioctl_buffer[CROS_EC_COMMAND_HEADER.size:CROS_EC_COMMAND_HEADER.size + len(payload)] = payload

        try:
            ioctl_result = fcntl.ioctl(self.fd, CROS_EC_DEV_IOCXCMD, ioctl_buffer, True)
        except OSError as exc:
            if exc.errno in (errno.EACCES, errno.EPERM):
                raise ProbeError(f"{self.devnode}: permission denied; root access may be required") from exc
            raise ProbeError(f"{self.devnode}: CROS_EC_DEV_IOCXCMD failed: {exc}") from exc

        if isinstance(ioctl_result, (bytes, bytearray)):
            raw = bytes(ioctl_result)
        else:
            raw = bytes(ioctl_buffer)

        version_out, command_out, outsize_out, insize_out, result = CROS_EC_COMMAND_HEADER.unpack_from(raw, 0)
        if result != EC_RES_SUCCESS:
            raise EcProtocolError(f"EC command failed with result code {result}")

        data_start = CROS_EC_COMMAND_HEADER.size
        data_end = data_start + response_length
        return {
            "command_version": version_out,
            "command": command_out,
            "outsize": outsize_out,
            "insize": insize_out,
            "result": result,
            "data": raw[data_start:data_end],
            "raw": raw,
        }


class LinuxAcpiCallTransport:
    def __init__(self, procnode: Path) -> None:
        self.procnode = procnode

    def __enter__(self) -> "LinuxAcpiCallTransport":
        if not self.procnode.exists():
            raise ProbeError(f"{self.procnode}: device node not found")
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        return None

    def call(self, method: str, args: tuple[str, ...] = ()) -> str:
        expression = " ".join((method, *args)).strip()
        try:
            with self.procnode.open("w", encoding="utf-8") as handle:
                handle.write(expression)
                handle.write("\n")
        except FileNotFoundError as exc:
            raise ProbeError(f"{self.procnode}: device node not found") from exc
        except PermissionError as exc:
            raise ProbeError(f"{self.procnode}: permission denied; root access may be required") from exc
        except OSError as exc:
            raise ProbeError(f"{self.procnode}: write failed: {exc}") from exc

        result = read_text(self.procnode)
        if result is None:
            raise ProbeError(f"{self.procnode}: unable to read call result")
        return result


def read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8", errors="replace").strip()
    except FileNotFoundError:
        return None
    except OSError:
        return None


def describe_hwmon_value(name: str, value: str) -> str | None:
    if re.fullmatch(r"pwm\d+_enable", name):
        if value == "0":
            return "no control / full speed"
        if value == "1":
            return "manual mode"
        if value.isdigit() and int(value) >= 2:
            return "automatic mode"
    return None


def classify_acpi_call_output(raw: str) -> dict[str, Any]:
    text = raw.replace("\x00", "").replace("\r", "").strip()
    result: dict[str, Any] = {"raw": text}
    if not text:
        result["type"] = "empty"
        return result
    if text == "not called":
        result["type"] = "not_called"
        return result
    if text.startswith("Error:"):
        result["type"] = "error"
        return result
    try:
        value = int(text, 0)
        result["type"] = "integer"
        result["value"] = value
        result["value_hex"] = f"0x{value:x}"
        return result
    except ValueError:
        pass
    if text.startswith('"') and text.endswith('"'):
        result["type"] = "string"
        result["value"] = text[1:-1]
        return result
    if text.startswith("{") and text.endswith("}"):
        result["type"] = "buffer"
        return result
    if text.startswith("[") and text.endswith("]"):
        result["type"] = "package"
        return result
    result["type"] = "unknown"
    return result


def collect_i2c_adapters() -> list[dict[str, Any]]:
    sysfs_root = Path("/sys/class/i2c-dev")
    adapters: list[dict[str, Any]] = []
    if sysfs_root.exists():
        for entry in sorted(sysfs_root.iterdir()):
            name = read_text(entry / "name")
            adapters.append(
                {
                    "name": entry.name,
                    "devnode": f"/dev/{entry.name}",
                    "description": name,
                }
            )
    else:
        sysfs_bus_root = Path("/sys/bus/i2c/devices")
        if sysfs_bus_root.exists():
            for entry in sorted(sysfs_bus_root.glob("i2c-*")):
                name = read_text(entry / "name")
                adapters.append(
                    {
                        "name": entry.name,
                        "devnode": f"/dev/{entry.name}",
                        "description": name,
                    }
                )
        else:
            for devnode in sorted(Path("/dev").glob("i2c-*")):
                adapters.append({"name": devnode.name, "devnode": str(devnode), "description": None})
    return adapters


def collect_cros_ec_devices() -> list[dict[str, Any]]:
    devices: list[dict[str, Any]] = []
    seen: set[str] = set()
    class_root = Path("/sys/class/chromeos")
    if class_root.exists():
        for entry in sorted(class_root.iterdir()):
            name = entry.name
            if name in seen:
                continue
            seen.add(name)
            devices.append(
                {
                    "name": name,
                    "devnode": f"/dev/{name}",
                    "exists": Path(f"/dev/{name}").exists(),
                    "sysfs": str(entry),
                }
            )

    for name in CROS_EC_DEV_NAMES:
        devnode = Path(f"/dev/{name}")
        if name in seen:
            continue
        if devnode.exists():
            seen.add(name)
            devices.append(
                {
                    "name": name,
                    "devnode": str(devnode),
                    "exists": True,
                    "sysfs": str(class_root / name) if class_root.exists() else None,
                }
            )

    return devices


def collect_thermal_zones(
    root: Path = Path("/sys/class/thermal"),
) -> list[dict[str, Any]]:
    zones: list[dict[str, Any]] = []
    if not root.exists():
        return zones

    for entry in sorted(root.glob("thermal_zone*")):
        trip_points = {
            child.name: read_text(child)
            for child in sorted(entry.glob("trip_point_*"))
            if child.is_file()
        }
        zones.append(
            {
                "name": entry.name,
                "type": read_text(entry / "type"),
                "temp": read_text(entry / "temp"),
                "mode": read_text(entry / "mode"),
                "policy": read_text(entry / "policy"),
                "device": str((entry / "device").resolve()) if (entry / "device").exists() else None,
                "trip_point_source": "sysfs",
                "trip_point_count": len(trip_points),
                "trip_points": trip_points,
            }
        )
    return zones


def collect_cooling_devices() -> list[dict[str, Any]]:
    devices: list[dict[str, Any]] = []
    root = Path("/sys/class/thermal")
    if not root.exists():
        return devices

    for entry in sorted(root.glob("cooling_device*")):
        devices.append(
            {
                "name": entry.name,
                "type": read_text(entry / "type"),
                "cur_state": read_text(entry / "cur_state"),
                "max_state": read_text(entry / "max_state"),
                "device": str((entry / "device").resolve()) if (entry / "device").exists() else None,
            }
        )
    return devices


def collect_hwmon() -> list[dict[str, Any]]:
    items: list[dict[str, Any]] = []
    root = Path("/sys/class/hwmon")
    if not root.exists():
        return items

    interesting_prefixes = ("fan", "pwm", "temp", "in", "curr", "power")

    for entry in sorted(root.glob("hwmon*")):
        sensors: dict[str, str] = {}
        for child in sorted(entry.iterdir()):
            if not child.is_file():
                continue
            if not child.name.startswith(interesting_prefixes):
                continue
            if child.name.endswith("_label"):
                continue
            value = read_text(child)
            if value is not None:
                sensors[child.name] = value
        items.append(
            {
                "name": entry.name,
                "chip_name": read_text(entry / "name"),
                "device": str((entry / "device").resolve()) if (entry / "device").exists() else None,
                "files": [child.name for child in sorted(entry.iterdir()) if child.is_file()],
                "sensors": sensors,
            }
        )
    return items


def collect_optional_interfaces() -> dict[str, Any]:
    return {
        "proc_acpi_call": Path("/proc/acpi/call").exists(),
        "module_acpi_call": Path("/sys/module/acpi_call").exists(),
        "debugfs_acpi_root": Path("/sys/kernel/debug/acpi").exists(),
        "debugfs_acpi_custom_method": Path("/sys/kernel/debug/acpi/custom_method").exists(),
        "module_cix_fan": Path("/sys/module/cix_fan").exists(),
        "module_i2c_dev": Path("/sys/module/i2c_dev").exists(),
        "module_cros_ec": Path("/sys/module/cros_ec").exists(),
        "module_cros_ec_chardev": Path("/sys/module/cros_ec_chardev").exists(),
    }


def collect_acpi_devices() -> list[dict[str, Any]]:
    known_hids = {
        "CIXHA015": "custom EC wrapper",
        "CIXHA024": "custom hardware-monitor wrapper",
        "PNP0C0B": "generic ACPI fan",
        "PNP0C0C": "generic ACPI power button",
    }
    sysfs_root = Path("/sys/bus/acpi/devices")
    devices: list[dict[str, Any]] = []
    if not sysfs_root.exists():
        return devices

    for entry in sorted(sysfs_root.iterdir()):
        hid = read_text(entry / "hid")
        if hid not in known_hids:
            continue
        devices.append(
            {
                "sysfs": str(entry),
                "hid": hid,
                "role": known_hids[hid],
                "path": read_text(entry / "path"),
                "status": read_text(entry / "status"),
                "modalias": read_text(entry / "modalias"),
            }
        )
    return devices


def format_result(name: str, decoded: dict[str, Any]) -> str:
    if name == "ec_version":
        return decoded["ec_version"]
    if name == "board_id":
        return (
            f"{decoded['raw_hex']} "
            f"(sku={decoded['sku']}, memory={decoded['memory']}, rev={decoded['revision']})"
        )
    if name == "battery_static_0":
        return (
            f"{decoded['manufacturer']} {decoded['model']} "
            f"{decoded['design_capacity_mAh']}mAh {decoded['design_voltage_mV']}mV "
            f"cycles={decoded['cycle_count']}"
        )
    if name == "battery_dynamic_0":
        return (
            f"{decoded['remaining_capacity_mAh']}/{decoded['full_capacity_mAh']}mAh "
            f"{decoded['actual_voltage_mV']}mV {decoded['actual_current_mA']}mA "
            f"flags={','.join(decoded['flag_names']) or 'none'}"
        )
    if name == "pmic_version":
        return " ".join(decoded["pmic_versions_hex"])
    if name == "pd_version":
        return " ".join(decoded["display_order"])
    if name == "greenpak_version":
        return decoded["greenpak_version_hex"]
    if name == "charger_info":
        return f"{decoded['name']} {decoded['type']} online={decoded['online_bool']}"
    if name == "fan_target_rpm":
        return f"{decoded['fan_target_rpm']} RPM"
    if name == "pwm_duty_0_0":
        return f"{decoded['pwm_duty_percent']}%"
    if name == "pvt_temp":
        return f"{decoded['celsius']:.2f} C"
    if name == "farm_id":
        return decoded["farm_id_hex"]
    if name == "poweroff_reason":
        return decoded["poweroff_reason_hex"]
    return json.dumps(decoded, sort_keys=True)


def format_acpi_result(name: str, decoded: dict[str, Any]) -> str:
    if name == "ectz_tmp":
        return f"{decoded['celsius']:.2f} C ({decoded['deci_kelvin']} dK)"
    if name in {"hwmn_gfpw", "ec0_gfpw"}:
        return f"{decoded['pwm_duty_percent']}%"
    if name == "ecfn_sta":
        return "on" if decoded["is_on"] else "off"
    if name in {"hwmn_sta", "ec0_sta"}:
        flags = []
        for key, label in (
            ("present", "present"),
            ("enabled", "enabled"),
            ("show_in_ui", "show_in_ui"),
            ("functioning", "functioning"),
            ("battery_present", "battery_present"),
        ):
            if decoded[key]:
                flags.append(label)
        return f"{decoded['status_hex']} ({','.join(flags) or 'none'})"
    return json.dumps(decoded, sort_keys=True)


def resolve_devnode(args: argparse.Namespace) -> Path:
    if args.device:
        return Path(args.device)
    if args.bus is not None:
        return Path(f"/dev/i2c-{args.bus}")
    raise ProbeError("either --device or --bus is required for this command")


def resolve_survey_backend(args: argparse.Namespace) -> tuple[str, Path | None]:
    if args.backend == "i2c":
        return "i2c", resolve_devnode(args)

    if args.backend == "cros-ec":
        if args.ec_device:
            return "cros-ec", Path(args.ec_device)
        devices = collect_cros_ec_devices()
        for device in devices:
            if device["exists"]:
                return "cros-ec", Path(device["devnode"])
        raise ProbeError("no /dev/cros_ec-style device found")

    if args.ec_device:
        return "cros-ec", Path(args.ec_device)
    if args.device or args.bus is not None:
        return "i2c", resolve_devnode(args)

    adapters = collect_i2c_adapters()
    if adapters:
        first = adapters[0]
        return "i2c", Path(first["devnode"])

    devices = collect_cros_ec_devices()
    for device in devices:
        if device["exists"]:
            return "cros-ec", Path(device["devnode"])

    raise ProbeError(
        "no Linux i2c-dev adapter or cros_ec chardev was found; "
        "check /sys/bus/i2c/devices, /sys/class/chromeos, and /dev"
    )


def run_survey(args: argparse.Namespace) -> int:
    commands = list(BASIC_COMMANDS)
    if args.include_extra:
        commands.extend(EXTRA_READS)

    backend, devnode = resolve_survey_backend(args)
    address = int(args.address, 0)

    results: list[dict[str, Any]] = []
    if backend == "i2c":
        assert devnode is not None
        with LinuxI2CTransport(devnode) as transport:
            for command in commands:
                request = make_request(command.code, command.request_payload)
                item: dict[str, Any] = {
                    "backend": backend,
                    "name": command.name,
                    "description": command.description,
                    "command": f"0x{command.code:04x}",
                    "request_hex": request.hex(),
                    "response_length": command.response_packet_length,
                }
                try:
                    raw_response = transport.transfer(address, request, command.response_packet_length)
                    parsed = parse_response(raw_response)
                    decoded = command.decoder(parsed.data)
                    item["status"] = "ok"
                    item["response_hex"] = raw_response.hex()
                    item["transport_result"] = parsed.transport_result
                    item["decoded"] = decoded
                except Exception as exc:  # pragma: no cover - exercised on target hardware
                    item["status"] = "error"
                    item["error"] = str(exc)
                results.append(item)
    else:
        assert devnode is not None
        with LinuxCrosEcTransport(devnode) as transport:
            for command in commands:
                item = {
                    "backend": backend,
                    "name": command.name,
                    "description": command.description,
                    "command": f"0x{command.code:04x}",
                    "response_length": command.response_data_length,
                    "request_payload_hex": command.request_payload.hex(),
                }
                try:
                    response = transport.transfer(
                        command.code,
                        command.request_payload,
                        command.response_data_length,
                    )
                    decoded = command.decoder(response["data"])
                    item["status"] = "ok"
                    item["response_hex"] = response["raw"].hex()
                    item["decoded"] = decoded
                except Exception as exc:  # pragma: no cover - exercised on target hardware
                    item["status"] = "error"
                    item["error"] = str(exc)
                results.append(item)

    payload = {
        "tool": "o6_ec_probe",
        "backend": backend,
        "devnode": str(devnode),
        "address": f"0x{address:02x}" if backend == "i2c" else None,
        "results": results,
    }

    if args.json:
        json.dump(payload, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    if backend == "i2c":
        print(f"Backend: i2c-dev")
        print(f"I2C device: {devnode}")
        print(f"EC address: 0x{address:02x}")
    else:
        print(f"Backend: cros_ec chardev")
        print(f"EC device: {devnode}")
    print("")
    for item in results:
        if item["status"] == "ok":
            print(f"{item['name']}: {format_result(item['name'], item['decoded'])}")
        else:
            print(f"{item['name']}: ERROR: {item['error']}")
    return 0


def run_list_i2c(args: argparse.Namespace) -> int:
    adapters = collect_i2c_adapters()
    if args.json:
        json.dump(adapters, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0
    if not adapters:
        print("No I2C adapters found in /sys/class/i2c-dev or /dev.")
        return 0
    for adapter in adapters:
        description = f" ({adapter['description']})" if adapter["description"] else ""
        print(f"{adapter['devnode']}{description}")
    return 0


def run_list_cros_ec(args: argparse.Namespace) -> int:
    devices = collect_cros_ec_devices()
    if args.json:
        json.dump(devices, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0
    if not devices:
        print("No cros_ec-style device nodes or /sys/class/chromeos entries found.")
        return 0
    for device in devices:
        suffix = " [present]" if device["exists"] else " [sysfs-only]"
        print(f"{device['devnode']}{suffix}")
    return 0


def run_list_acpi_call(args: argparse.Namespace) -> int:
    procnode = Path(args.procnode)
    payload = {
        "procnode": str(procnode),
        "exists": procnode.exists(),
        "module_loaded": Path("/sys/module/acpi_call").exists(),
        "methods": [
            {
                "name": method.name,
                "method": method.method,
                "args": list(method.args),
                "expression": method.expression,
                "description": method.description,
            }
            for method in ACPI_READS
        ],
    }
    if args.json:
        json.dump(payload, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    state = "present" if payload["exists"] else "missing"
    print(f"{procnode} [{state}]")
    if payload["module_loaded"]:
        print("module: acpi_call loaded")
    elif payload["exists"]:
        print("module: acpi_call backing module not visible in /sys/module")

    print("")
    print("Curated read-only ACPI calls:")
    for method in ACPI_READS:
        print(f"  {method.name}: {method.expression}")
        print(f"    {method.description}")
    return 0


def run_survey_acpi(args: argparse.Namespace) -> int:
    procnode = Path(args.procnode)
    results: list[dict[str, Any]] = []
    with LinuxAcpiCallTransport(procnode) as transport:
        for method in ACPI_READS:
            item = {
                "backend": "acpi-call",
                "name": method.name,
                "description": method.description,
                "method": method.method,
                "args": list(method.args),
                "expression": method.expression,
            }
            try:
                raw_result = transport.call(method.method, method.args)
                decoded = method.decoder(raw_result)
                item["status"] = "ok"
                item["raw_result"] = raw_result
                item["decoded"] = decoded
            except Exception as exc:  # pragma: no cover - exercised on target hardware
                item["status"] = "error"
                item["error"] = str(exc)
            results.append(item)

    payload = {
        "tool": "o6_ec_probe",
        "backend": "acpi-call",
        "procnode": str(procnode),
        "results": results,
    }
    if args.json:
        json.dump(payload, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    print("Backend: acpi_call")
    print(f"Interface: {procnode}")
    print("")
    for item in results:
        if item["status"] == "ok":
            print(f"{item['name']}: {format_acpi_result(item['name'], item['decoded'])}")
        else:
            print(f"{item['name']}: ERROR: {item['error']}")
    return 0


def run_call_acpi(args: argparse.Namespace) -> int:
    procnode = Path(args.procnode)
    acpi_args = tuple(args.arg or [])
    with LinuxAcpiCallTransport(procnode) as transport:
        raw_result = transport.call(args.method, acpi_args)

    payload = {
        "tool": "o6_ec_probe",
        "backend": "acpi-call",
        "procnode": str(procnode),
        "method": args.method,
        "args": list(acpi_args),
        "expression": " ".join((args.method, *acpi_args)).strip(),
        "result": classify_acpi_call_output(raw_result),
    }
    if args.json:
        json.dump(payload, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    result = payload["result"]
    print(f"Interface: {procnode}")
    print(f"Expression: {payload['expression']}")
    print(f"Result type: {result['type']}")
    print(f"Raw result: {result['raw']}")
    return 0 if result["type"] != "error" else 1


def run_scan_acpi(args: argparse.Namespace) -> int:
    payload = {
        "acpi_devices": collect_acpi_devices(),
        "cros_ec_devices": collect_cros_ec_devices(),
        "thermal_zones": sorted(str(path) for path in Path("/sys/class/thermal").glob("thermal_zone*")),
        "cooling_devices": sorted(str(path) for path in Path("/sys/class/thermal").glob("cooling_device*")),
    }
    if args.json:
        json.dump(payload, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    if payload["acpi_devices"]:
        print("ACPI devices:")
        for device in payload["acpi_devices"]:
            role = device["role"]
            path = device["path"] or "unknown path"
            print(f"  {device['hid']}: {role} [{path}]")
    else:
        print("No matching ACPI devices found in /sys/bus/acpi/devices.")

    if payload["cros_ec_devices"]:
        print("")
        print("ChromeOS EC devices:")
        for device in payload["cros_ec_devices"]:
            state = "present" if device["exists"] else "sysfs-only"
            print(f"  {device['devnode']} [{state}]")

    print("")
    print(f"Thermal zones: {len(payload['thermal_zones'])}")
    print(f"Cooling devices: {len(payload['cooling_devices'])}")
    return 0


def run_scan_sysfs(args: argparse.Namespace) -> int:
    payload = {
        "thermal_zones": collect_thermal_zones(),
        "cooling_devices": collect_cooling_devices(),
        "hwmon": collect_hwmon(),
        "optional_interfaces": collect_optional_interfaces(),
    }
    if args.json:
        json.dump(payload, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
        return 0

    print("Thermal zones:")
    if payload["thermal_zones"]:
        for zone in payload["thermal_zones"]:
            temp = zone["temp"]
            temp_text = f" temp={temp}" if temp is not None else ""
            zone_type = zone["type"] or "unknown"
            print(f"  {zone['name']}: {zone_type}{temp_text}")
            if zone["device"]:
                print(f"    device={zone['device']}")
            if zone["trip_points"]:
                for key, value in sorted(zone["trip_points"].items()):
                    print(f"    {key}={value}")
            else:
                print("    trip_points=none")
    else:
        print("  none")
    print("  Note: trip points are reported only from sysfs; this probe does not infer SCMI/DVFS policy.")

    print("")
    print("Cooling devices:")
    if payload["cooling_devices"]:
        for device in payload["cooling_devices"]:
            dev_type = device["type"] or "unknown"
            cur_state = device["cur_state"] or "?"
            max_state = device["max_state"] or "?"
            print(f"  {device['name']}: {dev_type} state={cur_state}/{max_state}")
            if device["device"]:
                print(f"    device={device['device']}")
    else:
        print("  none")

    print("")
    print("hwmon:")
    if payload["hwmon"]:
        for item in payload["hwmon"]:
            chip = item["chip_name"] or "unknown"
            print(f"  {item['name']}: {chip}")
            if item["device"]:
                print(f"    device={item['device']}")
            print(f"    files={', '.join(item['files'])}")
            if item["sensors"]:
                for key, value in sorted(item["sensors"].items()):
                    note = describe_hwmon_value(key, value)
                    if note:
                        print(f"    {key}={value} ({note})")
                    else:
                        print(f"    {key}={value}")
    else:
        print("  none")

    print("")
    print("Optional interfaces:")
    for key, value in payload["optional_interfaces"].items():
        print(f"  {key}: {'yes' if value else 'no'}")
    return 0


def run_self_test(_: argparse.Namespace) -> int:
    assert make_request(EC_CMD_GET_PVT_TEMP) == bytes.fromhex("da03b33e0c00000000")
    assert make_request(EC_CMD_PWM_GET_DUTY, b"\x00\x00") == bytes.fromhex("da03d50026000000020000")

    payload = b"V00.1-test\x00".ljust(19, b"\x00")
    inner = bytearray(8 + len(payload))
    inner[0] = 3
    inner[1] = 0
    inner[2:4] = (0).to_bytes(2, "big")
    inner[4:6] = len(payload).to_bytes(2, "big")
    inner[6:8] = b"\x00\x00"
    inner[8:] = payload
    inner[1] = checksum_with_hole(inner, 1)
    parsed = parse_response(bytes([0, len(inner) + 2]) + bytes(inner))
    assert decode_ec_version(parsed.data)["ec_version"] == "V00.1-test"
    assert CROS_EC_DEV_IOCXCMD == 0xC014EC00
    assert decode_acpi_pwm("0x64")["pwm_duty_percent"] == 100
    assert round(decode_acpi_tmp("0x0bd8")["celsius"], 1) == 30.0
    assert decode_acpi_power_resource_sta("0x1")["is_on"] is True
    assert decode_acpi_device_sta("0x3")["enabled"] is True
    assert classify_acpi_call_output("0x64")["type"] == "integer"

    print("self-test passed")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Read-only userspace probe for the Orion O6 ITE5570 EC. "
            "This can use Linux i2c-dev, a cros_ec character device, or curated ACPI calls."
        )
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    list_i2c = subparsers.add_parser("list-i2c", help="List visible Linux I2C adapters.")
    list_i2c.add_argument("--json", action="store_true", help="Emit JSON instead of plain text.")
    list_i2c.set_defaults(func=run_list_i2c)

    list_cros_ec = subparsers.add_parser(
        "list-cros-ec",
        help="List visible Chrome EC chardev/sysfs endpoints.",
    )
    list_cros_ec.add_argument("--json", action="store_true", help="Emit JSON instead of plain text.")
    list_cros_ec.set_defaults(func=run_list_cros_ec)

    list_acpi_call = subparsers.add_parser(
        "list-acpi-call",
        help="Inspect the acpi_call interface and show curated read-only method expressions.",
    )
    list_acpi_call.add_argument(
        "--procnode",
        default="/proc/acpi/call",
        help="Path to the acpi_call proc node. Default: /proc/acpi/call.",
    )
    list_acpi_call.add_argument("--json", action="store_true", help="Emit JSON instead of plain text.")
    list_acpi_call.set_defaults(func=run_list_acpi_call)

    scan_acpi = subparsers.add_parser(
        "scan-acpi",
        help="Inspect ACPI/sysfs exposure for the custom EC wrapper and generic fan/thermal objects.",
    )
    scan_acpi.add_argument("--json", action="store_true", help="Emit JSON instead of plain text.")
    scan_acpi.set_defaults(func=run_scan_acpi)

    scan_sysfs = subparsers.add_parser(
        "scan-sysfs",
        help="Inspect thermal, cooling, hwmon, and optional ACPI/debug interfaces in sysfs.",
    )
    scan_sysfs.add_argument("--json", action="store_true", help="Emit JSON instead of plain text.")
    scan_sysfs.set_defaults(func=run_scan_sysfs)

    survey = subparsers.add_parser(
        "survey",
        help="Run the curated read-only command set against the EC over i2c-dev or cros_ec.",
    )
    survey.add_argument(
        "--backend",
        choices=("auto", "i2c", "cros-ec"),
        default="auto",
        help="Transport backend to use. Default: auto.",
    )
    survey.add_argument("--device", help="Path to the Linux i2c-dev node, for example /dev/i2c-6.")
    survey.add_argument("--bus", type=int, help="Linux I2C bus number, for example 6.")
    survey.add_argument("--address", default="0x76", help="7-bit EC I2C address. Default: 0x76.")
    survey.add_argument(
        "--ec-device",
        help="Path to a cros_ec character device, for example /dev/cros_ec.",
    )
    survey.add_argument(
        "--include-extra",
        action="store_true",
        help="Also query Farm ID and power-off reason. These appear read-only but are less proven than the core survey.",
    )
    survey.add_argument("--json", action="store_true", help="Emit JSON instead of plain text.")
    survey.set_defaults(func=run_survey)

    survey_acpi = subparsers.add_parser(
        "survey-acpi",
        help="Run the curated read-only ACPI method set through /proc/acpi/call.",
    )
    survey_acpi.add_argument(
        "--procnode",
        default="/proc/acpi/call",
        help="Path to the acpi_call proc node. Default: /proc/acpi/call.",
    )
    survey_acpi.add_argument("--json", action="store_true", help="Emit JSON instead of plain text.")
    survey_acpi.set_defaults(func=run_survey_acpi)

    call_acpi = subparsers.add_parser(
        "call-acpi",
        help="Invoke an arbitrary ACPI method through /proc/acpi/call. Use with care.",
    )
    call_acpi.add_argument("method", help=r"ACPI method path, for example \_SB.HWMN.GFPW")
    call_acpi.add_argument(
        "arg",
        nargs="*",
        help=(
            "Arguments to pass after the method, using acpi_call syntax such as 0, 0x64, "
            '"hello", or b0102.'
        ),
    )
    call_acpi.add_argument(
        "--procnode",
        default="/proc/acpi/call",
        help="Path to the acpi_call proc node. Default: /proc/acpi/call.",
    )
    call_acpi.add_argument("--json", action="store_true", help="Emit JSON instead of plain text.")
    call_acpi.set_defaults(func=run_call_acpi)

    self_test = subparsers.add_parser("self-test", help="Run protocol encoder/parser self-checks.")
    self_test.set_defaults(func=run_self_test)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except ProbeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
