This overlay is only active for:

- `ARTEFACT_MODE=custom`
- `ENABLE_EXPERIMENTAL_UEFI_SETTINGS=true`

It intentionally keeps the blast radius small by extending the existing Radxa
UI path instead of enabling the broader CIX `TOKEN_SETUP_SUPPORT` menu stack.

Scope:

- `O6`: `RTC Wakeup`, `On-Board LAN Power`, `WLAN Power`, `GFX Device Power`,
  `M.2 SSD Power`, PCIe, USB, and CPU thermal ACPI model controls when
  combined with `ENABLE_FIRMWARE_FIXES=true`, USB Type-C controls, and
  `SR-IOV Support`
- `O6N`: `RTC Wakeup`, `On-Board LAN Power`, `WLAN Power`, PCIe, USB, and CPU
  thermal ACPI model controls when combined with `ENABLE_FIRMWARE_FIXES=true`,
  and USB Type-C controls

Explicitly not exposed here:

- `PCIe X2 Slot Power`
- camera, light-sensor, watchdog, GMAC, and other broader CIX-only settings
