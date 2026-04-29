# edk2-cix

[![Release](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml/badge.svg)](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml)

## Build

1. `git clone -b main-monorepo-edk2 https://github.com/radxa-pkg/edk2-cix.git`
2. Open in [`devcontainer`](https://code.visualstudio.com/docs/devcontainers/containers)
3. `make deb`

For reproducible metadata on `main-monorepo-edk2`, the build uses the
merge-base with `main-monorepo-upstream-edk2` as its default source
identity. In the default `ARTEFACT_MODE=custom`, that commit identity
also supplies the default timestamp used for reproducible metadata.
You can inspect the resolved values with
`make -C src print-build-metadata`.

For exact replay of a published O6 image, `main-monorepo-edk2` can
also reuse an extracted FIP cert bundle via
`SIGNING_CERT_SOURCE_DIR=<path>`. The directory may contain either
the build-tree filenames `trusted_key_no.crt`, `nt_fw_cert.crt`, and
`nt_fw_key.crt` or the extracted FIP filenames `trusted-key-cert.bin`,
`nt-fw-cert.bin`, and `nt-fw-key-cert.bin`.

Replay-compatible builds can also override the three historical
timestamp inputs independently:

- `BUILD_DATE=<iso8601>` for the displayed firmware build timestamp
- `SOURCE_DATE_EPOCH=<unix-seconds>` for compiler-provided `__DATE__`
  and `__TIME__` uses
- `PM_CONFIG_SOURCE_DATE_EPOCH=<unix-seconds>` for `csu_pm_config.bin`

To extract those values automatically from an upstream O6 release
artefact and generate ready-to-run replay wrappers, use:

```bash
python3 src/scripts/replay_o6_release.py <edk2-cix_*.deb>
```

If you only have `cix_flash_all.bin`, pair it with `BuildOptions`
when available:

```bash
python3 src/scripts/replay_o6_release.py \
  cix_flash_all.bin \
  --build-options BuildOptions
```

The generated `rebuild-o6-docker.sh` wrapper recreates the upstream
`/workspaces/edk2-cix` path layout so that `ARTEFACT_MODE=upstream`
can reproduce the vendor release payloads byte-for-byte.

Release builds default to `ARTEFACT_MODE=custom`, which strips
embedded PE/COFF debug path records from the final firmware images.
Use `ARTEFACT_MODE=upstream` when you specifically want
replay-compatible output instead.
