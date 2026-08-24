# Recorded Radxa replay inputs

Each `<version>/<board>/` directory records the non-source inputs required to
rebuild a published Radxa firmware release from the `edk2-stable202208` source
model:

- `replay.env`: build, compiler, PM-config, and displayed source metadata
- `certs/`: the three certificate payloads extracted from the published FIP
- `manifest.json`: hashes and sizes for the certificates and published output
  artefacts

`index.json` records the public package URL and package SHA-256 for every
release. The corresponding exact output hashes live in
`validation/expected-hashes.json` under
`upstream-<version>-bookworm`.

The published BL33 is recorded by hash only. It is not stored or fed back into
packaging: `make deterministic-replay` must rebuild BL33 from source, and strict
validation rejects any byte difference before the final images can be treated
as reproduced.

To regenerate an entry after extracting both boards with
`replay_o6_release.py`, run `record_release_replay_inputs.py` with the version,
published package, extraction root, output root, and validation profile file.
