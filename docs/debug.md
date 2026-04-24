# Debug

## Serial connection

The following UARTs can be used to debug EDK2 on Radxa Orion O6 and O6N:

- UART1: EC
- UART2: AP
- UART3: dedicated firmware debug channel
- UART4: PM
- UART5: SE

UART2 is the default management console for EDK2 and the operating system.

For O6 and O6N, the practical combinations are:

- `ARTEFACT_MODE=upstream FIRMWARE_TARGET=DEBUG`: keeps the imported upstream
  behavior.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG`: firmware `DEBUG()` output is
  visible on UART2.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG UART3_ENABLE=true`: firmware
  `DEBUG()` output stays on UART2, but UART3 is exposed to ACPI as `COM3` and
  the header pins are muxed for UART use instead of GPIO.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG DEBUG_ON_UART3=true`: firmware
  `DEBUG()` output moves to UART3 and `UART3_ENABLE` is implied automatically.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG DEBUG_PRINT_ERROR_LEVEL=0x8000004f`:
  firmware emits additional debug levels without changing the UART route.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=RELEASE DEBUG_VERBOSE=true`: RELEASE
  builds re-enable `DEBUG()` logging with a narrow debug-property mask while
  leaving asserts and related low-level debug behavior disabled.

The custom path defaults `DEBUG_PRINT_ERROR_LEVEL` to `0x80000040`
(`DEBUG_INFO|DEBUG_ERROR`). If `DEBUG_VERBOSE=true` is set without an explicit
`DEBUG_PRINT_ERROR_LEVEL`, the build enables all available `DEBUG_*` message
levels by default. Run `make help-debug` for the derived bit list from
`DebugLib.h`.

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

If you enable `devenv`, then you can run
`edk2-install </dev/data_partition>` from the project root as a faster way to
copy those files to a prepared USB disk.
