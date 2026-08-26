# Deployment scope

This repository builds and stages firmware payloads. `make install` copies a
staged payload to a mounted filesystem, normally beneath
`/boot/efi/edk2/radxa/`; it does not execute an EFI utility or modify the
board's firmware.

The currently documented deployment path is manual: boot the UEFI Shell and
run the `startup.nsh` supplied with the selected board payload. See
[Stage and manually deploy a firmware payload](./install.md) for the exact
filesystem layout and command form.
