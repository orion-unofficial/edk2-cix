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
the compressed firmware fits its reserved flash region. The custom builder now
measures compressed-FV requirements and adjusts a private FD definition before
signing. It then checks the actual FIP, including certificates and headers.
Undefined mask bits are rejected; categories without labelled source diagnostics
produce a warning without removing the requested bits.

`DEBUG_ALLOW_LARGE_IMAGE=1` permits an audited larger BL33 allocation only when
the signed FIP exceeds the original slot. Smaller images retain the original
layout even with this option. Enlarged images require the full-image updater;
no OTA image is emitted. Physical bounds, earlier vendor regions and signing
checks remain mandatory. `FORCE_DEBUG_BUILD` is retained as an inert compatibility
input; it cannot authorise a larger slot or bypass any check. See
[adaptive BL33 sizing and retained logging policy](debug-layout.md).

### Build qualification

The all-category logging-only RELEASE build passed offline packaging for O6,
EDK2 202608/Radxa 1.3.1, fixes enabled and experimental menus disabled, with
`DEBUG_ALLOW_LARGE_IMAGE=1`. BM-only logging used the original slot. The enlarged
layout has not yet been tested on hardware. See the
[measured build results](debug-layout.md#qualification-on-2026-09-23).

Use the actual signed FIP size to decide whether a mask fits. Historical
singleton-mask measurements are not a whitelist: source, options and toolchain
changes affect compressed size, and compression is not additive or monotonic.

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
[Build](build.md#stage-or-package-an-image).

If you enable `devenv`, then you can run `edk2-install </dev/data_partition>`
from the project root as a faster way to copy those files to a prepared USB
disk.
