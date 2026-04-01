# Custom Overlay

Files under this tree shadow selected imported upstream files when
`ARTEFACT_MODE=custom`.

The build prepends any matching package roots here to `PACKAGES_PATH`, and the
custom board-asset lookup in `src/Makefile` searches this tree before the
imported `src/edk2-*` sources.

Keep overlays narrow and intentional:

- prefer Makefile or helper-script changes for variable-only behavior
- use full-file overlays only when the firmware source itself must diverge
- mirror the upstream package-root layout so diffs stay easy to review
- keep data-only custom assets out of existing upstream module directories when
  possible, so a partial overlay cannot accidentally mask an imported `.inf` or
  source file during EDK2 path resolution
- the custom build preflight now rejects this shadowing pattern explicitly:
  if a file sits inside an imported module directory in the overlay tree, the
  overlay must also carry that module root's `.inf`

Current overlay areas:

- `edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/`:
  custom `DEBUG_ON_UART3` and `DEBUG_PRINT_ERROR_LEVEL` handling for
  `ARTEFACT_MODE=custom`
- `edk2-platforms/Platform/Radxa/Orion/O6/Library/PlatformEnvHookLib/` and
  `edk2-platforms/Platform/Radxa/Orion/O6N/Library/PlatformEnvHookLib/`:
  board hook changes that keep the UART3 pinmux aligned with the custom debug
  routing choice
- `edk2-platforms/Platform/CIX/Sky1/Drivers/AcpiSocTables/`:
  Sky1 ACPI source overlay used to reduce ACPICA warnings and remarks under the
  custom path
- `edk2-platforms/Platform/Radxa/Orion/O6/Drivers/AcpiPlatfomTables/` plus the
  sibling `LinuxAcpiConfig.h`:
  O6 ACPI source overlay used to reduce ACPICA warnings and remarks under the
  custom path; the sibling header is mirrored here because this package-level
  overlay would otherwise hide the imported header during EDK2 path resolution
- `edk2/SecurityPkg/Library/SecureBootVariableProvisionLib/` and
  `edk2-platforms/Platform/Radxa/Platforms/CIX/Sky1/SecureBootDefaults/Microsoft/`:
  custom Secure Boot provisioning logic and Microsoft default key material for
  custom builds

Keep module-local warning suppressions scoped and documented. Today the custom
Sky1 ACPI overlay uses targeted ACPICA suppressions for `2184` and `2095`
because those remarks come from intentional vendor UUIDs and intentionally empty
dependency packages rather than from ambiguous or malformed AML.

For the ACPI module overlays specifically, `make -C src check-custom-acpi-overlays`
verifies that the mirrored module file lists still match the imported source
tree. See `docs/custom-acpi-overlays.md` for the maintenance rules and the
rationale behind carrying byte-identical mirrored files inside those overlay
modules.
