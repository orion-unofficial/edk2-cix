# Custom ACPI Overlays

`ARTEFACT_MODE=custom` carries a small set of ACPI source overlays on top of the
imported Radxa/CIX trees. These overlays exist to keep the shipped firmware
closer to mainline expectations without rewriting the imported vendor baseline.

The current ACPI overlay areas are:

- `custom/overlay/edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/`
- `custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/AcpiPlatfomTables/`
- `custom/overlay/edk2-platforms/Platform/Radxa/Orion/O6/Drivers/LinuxAcpiConfig.h`

These are module-level mirrors, not patch fragments. Once EDK2 resolves a
module from the overlay tree, its relative include and source paths are also
resolved from that tree. That means some files stay byte-for-byte identical to
the imported source on purpose, simply to keep the module complete.

The custom ACPI overlays should follow three rules:

- keep the overlay scope limited to these tracked module roots and the sibling
  `LinuxAcpiConfig.h` header they require
- keep warning suppressions targeted and documented, rather than widening them
  across unrelated tables
- keep the mirrored module file lists aligned with the imported source so a
  vendor import cannot silently add or remove ACPI sources behind the overlay

Today the main custom differences are:

- targeted ACPICA warning/remark cleanup in the Sky1 and O6 ACPI tables
- custom UART3 debug routing reflected in the ACPI debug-port description
- the custom `ParseIomuxTemplate.py` helper path needed by the sparse custom
  overlay workspace

Use the host-side checker to verify that the overlay modules still mirror the
imported file lists cleanly:

```sh
make -C src check-custom-acpi-overlays
```

The checker is also part of `make -C src preflight ARTEFACT_MODE=custom`, so a
custom build now fails early if one of these overlay modules drifts out of sync
with the imported source tree.
