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

The initial custom overlay makes `DEBUG_ON_UART3` opt-in on O6/O6N, aligns the
UART3 pinmux with that setting, and lets custom builds override
`DEBUG_PRINT_ERROR_LEVEL` without modifying the imported upstream sources in
place.
