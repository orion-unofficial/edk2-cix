# Debug

For the broader explanation of how the custom build variables interact, see
[`build-variables.md`](build-variables.md). This page focuses only on the
serial and firmware-debug side of those build choices.

## Serial connection

The following UARTs can be used to debug EDK2 on Radxa Orion O6 and O6N:

- UART1: EC (Embedded Controller)
- UART2: AP (Application Processor, the host system console)
- UART3: dedicated firmware debug channel
- UART4: PM (power-management controller)
- UART5: SE (Security Engine)

UART2 is the default management console for EDK2 and the operating system.

For O6 and O6N, the practical combinations are:

- `ARTEFACT_MODE=upstream FIRMWARE_TARGET=DEBUG`: keeps the imported upstream
  behaviour.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG`: firmware `DEBUG()` output is
  visible on UART2.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG UART3_ENABLE=true`: firmware
  `DEBUG()` output stays on UART2, but UART3 is exposed to ACPI as `COM3` and
  the header pins are muxed for UART use instead of GPIO.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG DEBUG_ON_UART3=true`: firmware
  `DEBUG()` output moves to UART3 and `UART3_ENABLE` is implied automatically.
- To emit additional debug levels without changing the UART route, set
  `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG` and
  `DEBUG_PRINT_ERROR_LEVEL=0x8000004f`.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=RELEASE DEBUG_VERBOSE=true`: RELEASE
  builds re-enable `DEBUG()` logging while retaining `MDEPKG_NDEBUG` and
  `NDEBUG`. Assertions, `DEBUG_CODE` blocks, debug memory filling, and other
  source guarded by those definitions remain disabled. Logs inside those
  excluded blocks are also omitted. Full `FIRMWARE_TARGET=DEBUG` semantics
  remain unchanged.

The custom path defaults `DEBUG_PRINT_ERROR_LEVEL` to `0x80000040`
(`DEBUG_INFO|DEBUG_ERROR`). If `DEBUG_VERBOSE=true` is set without an explicit
`DEBUG_PRINT_ERROR_LEVEL`, the build enables all available `DEBUG_*` message
levels by default. Run `make help-debug` for the derived bit list from
`DebugLib.h`.

Logging still adds format strings, argument calculations, and print calls.
Keeping the other RELEASE gates reduces that cost but does not guarantee that
the compressed firmware fits its reserved flash region. Known FD/layout conflicts and nonzero verbose RELEASE experiments require
`FORCE_DEBUG_BUILD=1`; undefined mask bits cannot be overridden. There is no
qualified deployable verbose default yet. The normal size and signature checks
still apply, including for forced builds. Full DEBUG on the 202608 layout has a
4 MiB FD but only a `0x1f9000` bootloader3 slot; lowering the print mask cannot
resolve that structural conflict. For compact BDS and setup-migration diagnostics,
use `DEBUG_VERBOSE=false DEBUG_PRINT_ERROR_LEVEL=0x80000001`.

### Historical 202608 measurements before the capacity adjustment

The comparison baseline is **EDK2 202608**. Custom 202608/1.3.1 now allocates
`0x1f4000` to the compressed FD using 8 KiB of existing BL33-slot slack; no flash
partition moves. The measurements below used the previous `0x1f2000` budget.
They do not qualify verbose logging against the updated source and capacity.

With Radxa 1.3.1, O6
RELEASE, firmware fixes, experimental menus, Trixie and GCC 14.2, the public
build path produced these results:

| Settings | Compressed FV result against `0x1f2000` |
| --- | --- |
| `DEBUG_VERBOSE=true DEBUG_PRINT_ERROR_LEVEL=0x80000040` | `0x2031a0`: rejected, 70,048 bytes too large |
| `DEBUG_VERBOSE=true DEBUG_PRINT_ERROR_LEVEL=0x00000001` | `0x1f3958`: rejected, 6,488 bytes too large |
| `DEBUG_VERBOSE=false DEBUG_PRINT_ERROR_LEVEL=0x80000001` | `0x1f1c88`: packaging and chain checks passed, 888 bytes spare |

The last option retains ordinary RELEASE gating and the targeted BDS/setup
diagnostics. These exact-input results are not guarantees for another
configuration or toolchain. Those measurements preceded the custom AutoGen library-class repairs. See the [qualification review](qualification-review-20260922.md)
for provenance, accepted mask bits, the implemented size preflight and remaining
validation work. No testing on hardware was performed.

### Historical 202605 measurements

In the September 2026 O6 qualification using EDK2 202605, Radxa 1.3.1,
firmware fixes, and the experimental menus, logging-only RELEASE with
`DEBUG_PRINT_ERROR_LEVEL=0x80000040` compiled cleanly but required a compressed
FV of `0x201510` bytes. Its reservation was `0x1f2000`, so the build correctly
failed with a 62,736-byte overflow. Compiled BDS output contained INFO messages,
and its compiler dependencies confirmed selection of the logging header while
retaining both RELEASE definitions. Removing other debug code is therefore
not sufficient to make the INFO/error mask fit this configuration.

The preceding implementation, built with the same options and fixed build
date, required `0x212358` bytes: the logging-only change saved 69,192 bytes
(about 67.6 KiB, or 3.2%). Source-revision metadata necessarily differs between
the two builds.

The same configuration with `DEBUG_VERBOSE=true` and
`DEBUG_PRINT_ERROR_LEVEL=0x00000001` passed full-flash and OTA packaging and
certificate-chain validation. Its compressed FV used `0x1f1bb8` bytes, leaving
1,096 bytes spare. This mask enables initialization messages only and excludes
error messages; the available space is specific to this tested configuration.
This was build qualification, without testing on hardware.

### UART routing

`DEBUG_ON_UART3`, `UART3_ENABLE`, `DEBUG_VERBOSE`, and
`DEBUG_PRINT_ERROR_LEVEL` are only honored on the custom overlay path. When
UART3 is enabled it consumes 40-pin header GPIO105 and GPIO106, so those lines
are no longer available as general GPIO while UART3 is active. The broader CIX
runtime `Debug Mode` menu exists on boards that enable `DEBUG_MODE_SUPPORT`,
but O6 and O6N do not currently enable that path.

All UARTs use 115200 baud.

````admonish info

If you use version `1.0.0-1` or earlier, EC UART can be viewed with:

```bash
picocom -b 460800 --imap lfcrlf /dev/ttyX
```

````

In general, the log output order after power is connected is as follows:

```text
EC ---Power On---> SE ---> AP ---> Debug
```

If you enable `devenv`, then you can run `edk2-console` to launch the above
four UART consoles at once. First create a local `devenv.local.nix` based on
`devenv.local.nix.example` so it defines the local UART devices before you use
this command.

## Installation

After `make firmware-stage` completes, you can find the staged deployable files
under `dist/firmware/<product>/<version>/`.

For the non-`deb` staged layout and direct UEFI Shell entry points, continue in
[Build](build.md#build).

If you enable `devenv`, then you can run `edk2-install </dev/data_partition>`
from the project root as a faster way to copy those files to a prepared USB
disk.
