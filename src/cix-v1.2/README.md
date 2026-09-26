This directory materialises the curated CIX V1.2 source set used by
`CIX_RELEASE=v1.2` custom builds.

It intentionally combines:

- the public CIX `bios` V1.2 source snapshot:
  - repo: `https://github.com/cixtech/bios`
  - commit: `90f39f4469d39b3cd135ca8c5ae6400aca75b292`
- the `tf-a` and `tee` submodule revisions pinned by that snapshot:
  - `tf-a`: `114fb20577bcc4038025de4e12bca60e04dd5212`
  - `tee`: `cc66640f3815da4defc50f72b66ae3bac97cd48a`
- the later public CIX release-repo `bootloader1.img` payload that matches
  community-release hardware logs:
  - repo: `https://github.com/cixtech/cix_opensource__release__edk2-non-osi`
  - commit: `3140811e6fd4f08fa064858168309f098381335b`
  - file: `Platform/CIX/Sky1/PackageTool/Firmwares/bootloader1.img`

This is a curated mode, not an exact historical replay of a single public
superproject commit. It is designed to keep the selector surface simple while
remaining explicit about the mixed public provenance of the payload and source
inputs.
