"""Strict schema and append-only comparison for embedded firmware trust catalogues."""

from __future__ import annotations

import json
import re

from bl33_layout import FLASH_SIZE
from reconstruction_common import ReconstructionError
from validate_firmware_chain import PACKAGE


IDENTITY_FIELDS = ("bl1_sha256", "trusted_fip_sha256", "uefi_certificate_sha256")
ROOT_FIELDS = ("trusted_root_spki_sha256", "uefi_root_spki_sha256", "uefi_oem_spki_sha256")
COUNTER_FIELDS = ("trusted_counter", "uefi_counter")
PIN_FIELDS = (*IDENTITY_FIELDS, *ROOT_FIELDS, *COUNTER_FIELDS, "reboot_scratch_layout")
PACKAGE_INPUTS = ("Firmwares/bootloader1.img", "Firmwares/bootloader2.img",
                  "certs/trusted_key_no.crt", "spi_flash_config_all.json")


def require(condition, message):
    if not condition:
        raise ReconstructionError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def fields(value, names, label):
    require(isinstance(value, dict) and value.keys() == set(names), f"invalid {label} fields")


def records(value, label):
    require(isinstance(value, list) and bool(value), f"invalid {label} list")
    return value


def fingerprint(value, length=64):
    return isinstance(value, str) and re.fullmatch(f"[0-9a-f]{{{length}}}", value) is not None


def integer(value, minimum, maximum):
    return type(value) is int and minimum <= value <= maximum


def parse_catalogue(data):
    try:
        document = json.loads(data.decode("utf-8"), object_pairs_hook=unique_object)
    except (ValueError, RecursionError) as exc:
        raise ReconstructionError(f"malformed catalogue JSON: {exc}") from exc
    fields(document, ("schema_version", "package_path", "purpose", "references"), "catalogue")
    require(type(document["schema_version"]) is int and document["schema_version"] == 1,
            "unsupported catalogue schema version")
    require(document["package_path"] == f"{PACKAGE}/", "invalid catalogue package path")
    require(isinstance(document["purpose"], str) and bool(document["purpose"]), "invalid catalogue purpose")
    references = {}
    provenance_refs = set()
    known_layouts = {}
    for row in records(document["references"], "reference"):
        fields(row, (*PIN_FIELDS, "flash_layouts", "provenance"), "reference")
        require(all(fingerprint(row[key]) for key in (*IDENTITY_FIELDS, *ROOT_FIELDS)),
                "invalid reference fingerprint")
        require(all(integer(row[key], 0, 0xffffffff) for key in COUNTER_FIELDS), "invalid rollback counter")
        require(integer(row["reboot_scratch_layout"], 1, 2), "invalid reboot scratch layout")
        identity = tuple(row[key] for key in IDENTITY_FIELDS)
        require(identity not in references, f"duplicate trust reference {identity}")
        provenance = {}
        for origin in records(row["provenance"], "provenance"):
            fields(origin, ("ref", "commit"), "provenance")
            ref = origin["ref"]
            require(isinstance(ref, str) and re.fullmatch(r"source/vendor/radxa/[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9]+)?/edk2-stable202208", ref)
                    and fingerprint(origin["commit"], 40), "invalid vendor provenance")
            require(ref not in provenance_refs, f"duplicate vendor provenance {ref}")
            provenance_refs.add(ref)
            provenance[ref] = origin["commit"]
        layouts = {}
        for layout in records(row["flash_layouts"], "flash layout"):
            fields(layout, ("config_sha256", "header", "size", "entries"), "flash layout")
            digest = layout["config_sha256"]
            require(fingerprint(digest), "invalid flash layout digest")
            require(digest not in layouts, f"duplicate flash layout {digest}")
            require(type(layout["size"]) is int and layout["size"] == FLASH_SIZE
                    and type(layout["header"]) is int and layout["header"] in (0x100000, 0x200000),
                    "invalid flash size or header address")
            entries = layout["entries"]
            require(isinstance(entries, dict) and {"1", "2", "7"} <= entries.keys(),
                    "missing required flash entries")
            for kind, entry in entries.items():
                require(re.fullmatch(r"[1-9][0-9]*", kind) is not None, "invalid flash entry kind")
                fields(entry, ("address", "size"), "flash entry")
                require(integer(entry["address"], 0, layout["size"] - 1)
                        and integer(entry["size"], 1, layout["size"] - entry["address"]),
                        "invalid flash entry allocation")
            require(digest not in known_layouts or known_layouts[digest] == layout,
                    f"contradictory flash layout {digest}")
            known_layouts[digest] = layouts[digest] = layout
        references[identity] = ({key: row[key] for key in PIN_FIELDS}, layouts, provenance)
    return document["purpose"], references


def compatible_reference(embedded, current, package_fingerprints):
    """Preserve embedded pins and return provenance for the source's selected reference."""
    purpose, references = embedded
    current_purpose, current_references = current
    require(purpose == current_purpose, "catalogue purpose differs from build branch")
    for identity, (pins, layouts, provenance) in references.items():
        require(identity in current_references, f"missing historical trust reference {identity}")
        current_pins, current_layouts, current_provenance = current_references[identity]
        require(pins == current_pins, f"changed historical trust pins for {identity}")
        for digest, layout in layouts.items():
            require(current_layouts.get(digest) == layout, f"missing or changed historical flash layout {digest}")
        for ref, commit in provenance.items():
            require(current_provenance.get(ref) == commit, f"missing or changed historical provenance {ref}")

    *identity_fields, layout_digest = package_fingerprints
    identity = tuple(identity_fields)
    require(identity in references, "embedded catalogue lacks source BL1/BL2/certificate reference")
    _, layouts, provenance = references[identity]
    require(layout_digest in layouts, "embedded catalogue lacks source flash layout")
    return provenance


def require_provenance(provenance, ref, commit):
    require(provenance.get(ref) == commit, f"embedded catalogue lacks pinned source provenance {ref}")
