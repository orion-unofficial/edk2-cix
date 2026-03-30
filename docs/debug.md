# Debug

## Serial connection

The following UARTs can be used to debug EDK2 on Radxa Orion O6:

- UART1: EC
- UART2: AP
- UART3: dedicated firmware debug channel
- UART4: PM
- UART5: SE

UART2 is the default management console for EDK2 and the operating system.

On the current O6 and O6N sources, `UEFI_TARGET=DEBUG` does not enable UART3.
The dedicated debug path is selected by the Radxa `DEBUG_ON_UART3` define,
which routes the firmware debug library to UART3 while leaving the normal
firmware and OS console on UART2. The dedicated O6/O6N UART3 pinmux entries
are currently compiled only when `DEBUG_MODE` is not set, so a `DEBUG` build
is not what enables the physical UART3 path today. The broader CIX runtime
`Debug Mode` menu exists on boards that enable `DEBUG_MODE_SUPPORT`, but O6
and O6N do not currently enable that path.

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
