# Arm Trusted Firmware `fiptool`

This directory vendors the minimum Linux-relevant source needed to build
`fiptool` for this source tree.

The authoritative upstream provenance for the current snapshot lives in
[upstream.json](upstream.json). That file records the canonical upstream repo,
release tag, commit, and imported path list.

Imported files:

- `tools/fiptool/fiptool.c`
- `tools/fiptool/fiptool.h`
- `tools/fiptool/fiptool_platform.h`
- `tools/fiptool/tbbr_config.c`
- `tools/fiptool/tbbr_config.h`
- `include/tools_share/firmware_image_package.h`
- `include/tools_share/uuid.h`
- `license.rst`

This snapshot intentionally excludes the rest of TF-A and the upstream Windows
build support, because this source tree only supports Linux `x86_64` and Linux
`aarch64`/`arm64` build hosts.

Build the tool with:

```bash
make -C src/tools/arm-trusted-firmware-fiptool
```

The resulting host binary is written under `build/<host-arch>/fiptool`.

The maintainer check/update workflow for future TF-A releases lives on
`build`.
