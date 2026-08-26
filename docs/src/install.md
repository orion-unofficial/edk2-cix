# Stage and manually deploy a firmware payload

`make install` builds and stages a selected payload, then copies it to a mounted
filesystem. With the default `INSTALL_ROOT=/boot/efi`, the destination is:

```text
/boot/efi/edk2/radxa/<product>/<version>/
```

For example:

```bash
make install FIRMWARE_BOARD=O6
```

This filesystem copy does not execute `BurnImage.efi`, `FlashUpdate.efi`, or
`startup.nsh`, and does not modify the board's firmware. Deployment remains a
separate manual action from the UEFI Shell, described below. Existing staged
files are not replaced unless `FORCE=1` is supplied.

## Full demo

Below is the full process of upgrading a `0.1.0` debug build to a `0.1.1-1`
release build.

Ventoy was used and an HDMI display was connected. The actual process happened
over serial.

[![asciicast](https://asciinema.org/a/O7YsPjUyLIa2174oFgPPdGKyt.svg)](https://asciinema.org/a/O7YsPjUyLIa2174oFgPPdGKyt)

---

## Where to find releases?

You can find all released Debian packages on the upstream Radxa GitHub
[Releases](https://github.com/radxa-pkg/edk2-cix/releases) page.

Similar to the upstream firmware packaging flow, each release contains the main
binary package as well as small metapackages.

Taking release
[`1.3.1`](https://github.com/radxa-pkg/edk2-cix/releases/tag/1.3.1) as an
example, you can expect:

- `edk2-cix_1.3.1_all.deb`: main binary package
- board-specific lightweight metapackages where published by Radxa

As usual, the main binary package is the large one, and the metapackages are
usually only a few KB.

The main binary package usually shares the repo name, while the metapackages
are named after specific products.

## Download and extract the release

To prepare a BIOS update disk, first, download and extract the package:

```bash
mkdir extract
cd extract
wget https://github.com/radxa-pkg/edk2-cix/releases/download/1.3.1/edk2-cix_1.3.1_all.deb
ar vx *.deb
tar xvf data.tar.xz
```

## Create the BIOS update disk

You should now have firmware payloads for the supported platforms, along with
the EFI utilities and `startup.nsh`. A current staged board directory includes
files such as:

```bash
BuildOptions
BurnImage.efi
FlashUpdate.efi
Shell.efi
VariableInfo.efi
cix_flash_all.bin
cix_flash_ota.bin
startup.nsh
```

Copy them to a USB disk formatted with a FAT filesystem, then connect it to the
target board.

Optionally, you can use `Ventoy` to create a BIOS update disk that can also be
used to load Linux ISOs, as long as the files stay under 4 GiB because of the
FAT32 limit.

[`Ventoy`](https://www.ventoy.net/) is a popular ISO multi-boot tool. It also
supports booting EFI applications on ARM64.

First, use `Ventoy` to create a bootable USB disk. Make sure you select the GPT
partition table option.

Then reformat the first partition with a FAT filesystem, because the default
exFAT filesystem is not supported by EDK2.

You can then copy the EDK2 build artefacts to the first partition as usual.

## Enter UEFI Shell

You will need to enter UEFI Shell to run the BIOS update script. There are two
ways to do that:

### Via UEFI Setup

Press `Escape` when prompted on the console. Then enter the `Boot Manager` menu
and select `UEFI Shell`.

### Via `Ventoy`

If you miss the `Escape` prompt when the system boots and there is no other
bootable media, EDK2 should boot into `Ventoy`.

```admonish caution
If a screen is connected, Ventoy will boot into graphical mode. In this mode
you will have no output on UART2, but you can still control the menu with it.

You can enable
[`Force Text Mode`](https://github.com/ventoy/Ventoy/issues/2983#issuecomment-2367411817)
under the display menu to allow output on UART2.
```

Inside the `Ventoy` UI, `Shell.efi` should appear as an option if you copied
everything. Run it to enter UEFI Shell.

```admonish caution
If you built for multiple EDK2 releases, you may have multiple `Shell.efi`
entries in `Ventoy`. They are generally compatible with different platforms,
but when in doubt, only use the one that came with your target platform and
only copy the EDK2 release output for that platform.
```

## Run the vendor deployment script from UEFI Shell

Once inside the UEFI Shell, you should first see a list of available storage
devices and their physical paths.

If you only have the USB disk connected, it should be listed as `fs0`.

You can rescan the storage devices with the `map -r` command, which will also
reprint the available mappings.

You can now run the supplied vendor script from the UEFI Shell. It uses Windows
path conventions, so an example command for a package copied beneath `radxa`
would be:

```cmd
fs0:\radxa\orion-o6\startup.nsh
```

For a payload staged by `make install` at its default destination, include the
repository staging prefix and version, for example:

```cmd
fs0:\edk2\radxa\orion-o6\1.3.1\startup.nsh
```

Use `orion-o6n` instead when flashing the O6N payload.

```admonish info
Before `0.3.0-1`, `startup.nsh` was called `setup.nsh`.
```

The first backslash is not mandatory:

```cmd
fs0:radxa\orion-o6\startup.nsh
```

The UEFI Shell provides limited auto-completion when you press `Tab`.

The script invokes the supplied EFI utilities. Review its prompt and the
selected board directory before continuing.
