# Build

We use devcontainer to maintain a consistent build environment.

To build all supported EDK2 variants, please run `make deb` within devcontainer.

Set `BUILD_TARGET` to `DEBUG` in `src/Makefile` to build for debug artifacts.

Edit `DSC` in `src/Makefile` to reduce amount of variants that will be built.
You should also edit `debian/edk2-cix.install` to exclude unbuild variants,
otherwise `debuild` will complain that those files are missing.

## Monorepo layout

On `main-monorepo-edk2`, the imported `edk2`, `edk2-platforms`, and
`edk2-non-osi` trees are regular directories inside this repo. There are
no Git submodules to initialize or update.

The monorepo-edk2 build resolves its displayed top-level source hash and
default timestamp from the merge-base with
`main-monorepo-upstream-edk2`, so curated overlay commits do not change
the firmware's reported source identity. In the default
`ARTEFACT_MODE=custom`, that source-model commit timestamp is used
instead of wall-clock time. Run `make -C src print-build-metadata` to
inspect the resolved values.

If you need to force a specific reproducible timestamp for a custom
build, export `SOURCE_DATE_EPOCH=<unix-seconds>` before running the
build. The same value is also passed into the O6 `pm_config`
generator unless `PM_CONFIG_SOURCE_DATE_EPOCH` is set explicitly, so
`csu_pm_config.bin` stops depending on wall-clock time.

For exact replay of a previously published O6 image, we found that the
vendor build embeds three independent timestamp domains. Set them
explicitly and point `SIGNING_CERT_SOURCE_DIR=<path-to-cert-bundle>`
at either:

- `BUILD_DATE=<iso8601>` for the displayed firmware build timestamp
- `SOURCE_DATE_EPOCH=<unix-seconds>` for compiler-provided `__DATE__`
  and `__TIME__` uses
- `PM_CONFIG_SOURCE_DATE_EPOCH=<unix-seconds>` for the O6 PM-config
  blob

- a build tree `certs/` directory containing `trusted_key_no.crt`,
  `nt_fw_cert.crt`, and `nt_fw_key.crt`
- an extracted FIP cert bundle containing `trusted-key-cert.bin`,
  `nt-fw-cert.bin`, and `nt-fw-key-cert.bin`

The build also supports two output modes:

- `ARTEFACT_MODE=custom` is the default on `main-monorepo-edk2` and
  strips embedded PE/COFF debug path records from release firmware
  images
- `ARTEFACT_MODE=upstream` keeps the historical output behavior for
  replay and byte-for-byte comparison work

## Replay published O6 firmware

To recover the replay settings from a published O6 release artefact
and write helper files under a fresh `/private/tmp/o6-replay-*`
directory, run:

```bash
python3 src/scripts/replay_o6_release.py <edk2-cix_*.deb>
```

The helper extracts:

- `BUILD_DATE`
- `SOURCE_DATE_EPOCH`
- `PM_CONFIG_SOURCE_DATE_EPOCH`
- `SOURCE_COMMIT_HASH` plus the recorded sub-component hashes from
  `BuildOptions` when they are available
- a reusable FIP cert bundle for `SIGNING_CERT_SOURCE_DIR`

It also writes:

- `replay.env`
- `rebuild-o6.sh`
- `rebuild-o6-docker.sh`

If you only have a standalone `cix_flash_all.bin`, the helper can
still recover the compiler and PM-config timestamps plus the cert
bundle. Supply a matching `BuildOptions` file, or pass
`--build-date <iso8601>`, if you want a complete replay build:

```bash
python3 src/scripts/replay_o6_release.py \
  cix_flash_all.bin \
  --build-options BuildOptions
```

To start the replay build immediately in the current shell, add
`--run-build`. If you are not already in a working build environment,
run the generated `rebuild-o6-docker.sh` wrapper instead. That
wrapper reuses the persistent `edk2-cix-buildbox` container, mounts
the checkout at `/workspaces/edk2-cix`, and therefore preserves the
same embedded build paths as the upstream release.

## Reuse the build container

For repeat local builds, keep a prepared amd64 build container around
instead of paying the full dependency bootstrap cost every time:

```bash
make buildbox-up
make buildbox-metadata
make buildbox-o6
make buildbox-deb
```

`make devcontainer_setup` is now idempotent: it checks for the
required Debian packages first and skips the `apt` work when the
environment is already ready.

If you need to refresh the monorepo from the authoritative uplifted source-model,
use the automation and runbooks on the separate `main-monorepo-meta`
branch rather than running `git submodule` commands in this checkout.
