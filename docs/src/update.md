# Deployment scope

This repository builds and stages firmware payloads. `make install` copies a
staged payload to a mounted filesystem, normally beneath
`/boot/efi/edk2/radxa/`; it does not execute an EFI utility or modify the
board's firmware.

The currently documented deployment path is manual: boot the UEFI Shell and
run the `startup.nsh` supplied with the selected board payload. See
[Stage and manually deploy a firmware payload](./install.md) for the exact
filesystem layout and command form.

## Custom updater checks

Custom firmware validates image-table lengths, payload ranges and the physical
8 MiB flash boundary before writing through its source-built update protocol.
It stops on a failed write or readback, and translates the vendor protocol's
status word into an EFI error for its FMP and flash-library callers. Where the
selected vendor release preserves NVRAM during FMP updates, a failed backup
prevents flashing and restore/readback failures are reported.

The source-built OTA path requires each incoming entry's address **and length**
to match the on-board table, because it does not rewrite that table. A changed
layout or payload length needs the full-image path; do not use OTA to install an
enlarged BL33 image. The documented deployment path remains `startup.nsh`.

These checks do not make interrupted flashing recoverable. They also do not
establish how the supplied binary flashing utility handles errors or whether it
uses this protocol. Keep programmer-based recovery available for qualification
on hardware. The existing packaging and signing-chain checks remain independent
of these runtime bounds checks. Upstream reproducible builds retain the vendor
implementation.
