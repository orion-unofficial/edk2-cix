# edk2-cix

[![Release](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml/badge.svg)](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml)

## Build

1. `git clone -b main-monorepo https://github.com/radxa-pkg/edk2-cix.git`
2. Open in [`devcontainer`](https://code.visualstudio.com/docs/devcontainers/containers)
3. `make deb`

Before a longer build, run `make -C src preflight` to fail early if the
expected package-tool binaries or cross-compiler are missing.

To capture a full build transcript plus a warning summary under `build-logs/`,
use `make buildbox-o6-log` or wrap any command with
`./scripts/capture_build_log.sh build-logs <command ...>`.

For reproducible metadata on `main-monorepo`, the build uses the nearest
mapped upstream `main` commit as its default source identity. In the default
`ARTEFACT_MODE=custom`, that commit identity also supplies the default
timestamp used for reproducible metadata. You can inspect the resolved values
with `make -C src print-build-metadata`.

For exact replay of a published O6 image, `main-monorepo` can also reuse an
extracted FIP cert bundle via `SIGNING_CERT_SOURCE_DIR=<path>`. The directory
may contain either the build-tree filenames `trusted_key_no.crt`,
`nt_fw_cert.crt`, and `nt_fw_key.crt` or the extracted FIP filenames
`trusted-key-cert.bin`, `nt-fw-cert.bin`, and `nt-fw-key-cert.bin`.

Replay-compatible builds can also override the three historical timestamp
inputs independently:

- `BUILD_DATE=<iso8601>` for the displayed firmware build timestamp
- `SOURCE_DATE_EPOCH=<unix-seconds>` for compiler-provided `__DATE__` and
  `__TIME__` uses
- `PM_CONFIG_SOURCE_DATE_EPOCH=<unix-seconds>` for `csu_pm_config.bin`

To extract those values automatically from an upstream O6 release artefact and
generate ready-to-run replay wrappers, use:

```bash
python3 src/scripts/replay_o6_release.py <edk2-cix_*.deb>
```

If you only have `cix_flash_all.bin`, pair it with `BuildOptions` when
available:

```bash
python3 src/scripts/replay_o6_release.py \
  cix_flash_all.bin \
  --build-options BuildOptions
```

The generated `rebuild-o6-docker.sh` wrapper recreates the upstream
`/workspaces/edk2-cix` path layout so that `ARTEFACT_MODE=upstream` can
reproduce the vendor release payloads byte-for-byte. By default it writes its
helper directory under the current system temp root and mounts that directory
into the build container automatically. If you want to stage those helper
files somewhere else, set `EDK2_CIX_HOST_TMPDIR` and, if needed,
`EDK2_CIX_CONTAINER_TMPDIR` when running the wrapper.

For host-side Python maintenance checks without entering the devcontainer, run:

```bash
python3 -W default -m compileall -q -f \
  src/edk2/BaseTools/Source/Python \
  src/edk2/BaseTools/Scripts \
  scripts
python3 -W default -m py_compile \
  src/edk2/ArmPlatformPkg/Scripts/Ds5/build_report.py
```

Release builds default to `ARTEFACT_MODE=custom`, which strips embedded
PE/COFF debug path records from the final firmware images. Use
`ARTEFACT_MODE=upstream` when you specifically want replay-compatible output
instead.
