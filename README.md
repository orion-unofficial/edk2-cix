# edk2-cix

[![Release](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml/badge.svg)](https://github.com/radxa-pkg/edk2-cix/actions/workflows/release.yaml)

## Build

1. `git clone -b main-monorepo-edk2 https://github.com/radxa-pkg/edk2-cix.git`
2. Open in [`devcontainer`](https://code.visualstudio.com/docs/devcontainers/containers)
3. `make deb`

For reproducible metadata on `main-monorepo-edk2`, the build uses the
merge-base with `main-monorepo-upstream-edk2` as its default source
identity. You can inspect the resolved values with
`make -C src print-build-metadata` and override the timestamp
explicitly with `SOURCE_DATE_EPOCH=<unix-seconds>`.
