# Debug

## Serial connection

The following UARTs can be used to debug EDK2 on Radxa Orion O6:

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
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG DEBUG_ON_UART3=true`: firmware
  `DEBUG()` output moves to UART3.
- `ARTEFACT_MODE=custom FIRMWARE_TARGET=DEBUG DEBUG_PRINT_ERROR_LEVEL=0x8000004f`:
  firmware emits additional debug levels without changing the UART route.

The `DEBUG_ON_UART3` switch is only honored on the custom overlay path. When
enabled it consumes 40-pin header GPIO105 and GPIO106, so those lines are no
longer available as general GPIO while UART3 debug is active. The broader CIX
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

Follow [Installation guide](install.md#create-the-bios-update-disk) to continue.

If you enable `devenv`, then you can run
`edk2-install </dev/data_partition>` from the project root as a faster way to
copy those files to a prepared USB disk.
