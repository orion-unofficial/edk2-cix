CIX EVB Platform
=======================================

# Summary

This is a port of ARM64 Tiano Core UEFI firmware for the CIX EVB platform based on the CIX P1 SoC.

CIX P1 edk2 code is base on as follows:
- [edk2](https://github.com/tianocore/edk2): `fb493ac84ebc6860e1690770fb88183effadebfb`
- [edk2-platforms](https://github.com/tianocore/edk2-platforms): `8ea6ec38da8812f0703e8845fe639b8845704f96`

# How to build (X86 & ARM64 Linux Environment)

This repo no longer uses the legacy upstream `build_and_package.sh` wrapper.
Build from the repo root with the supported top-level `make` targets instead.

  1. Install host dependencies.

    For a direct host build:

    ```bash
    make firmware_build_dep
    ```

    For the fuller packaging-capable dependency set:

    ```bash
    make devcontainer_setup
    ```

  2. Install the ACPI tool.

    Install `iasl` on the host and ensure it is in `PATH`.

  3. Build the firmware.

    ```bash
    make firmware-build
    ```

    This produces the raw build artefacts under `src/Build/O6/RELEASE_GCC5/`,
    including `cix_flash_all.bin` and `FV/SKY1_BL33_UEFI.fd`.

  4. If you want the deployable payload layout used by the package, stage it.

    ```bash
    make firmware-stage
    ```

    This writes the staged payload under `dist/firmware/orion-o6/<version>/`.

  5. Optional: override the cross-toolchain prefix if you need the old pinned
     Arm bare-metal toolchain layout.

    For example:

    ```bash
    make firmware-build \
      GCC5_AARCH64_PREFIX=/path/to/gcc-arm-10.2-2020.11-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
    ```

# How to Flash Firmware
  1. Use SPI Flash Programmer(like DediProg SF100) by flash file "cix_flash_all.bin"

  2. Run FlashUpdate.efi(edk2-non-osi/Platform/CIX/Sky1/FlashTool/FlashUpdate.efi) under UEFI shell
    For Example:

    FS0:\>FlashUpdate.efi -f cix_flash_all.bin
