Microsoft Secure Boot defaults for `ARTEFACT_MODE=custom`.

These files are Secure Boot data blobs embedded into the firmware image, not
OS executables. `PK.bin`, `KEK.bin`, and `DB.bin` are EFI signature databases
containing X.509 certificates. `DBX.bin` is an EFI signature database
containing SHA-256 revocation entries. None of these blobs are "run", so there
is no x86-vs-AArch64 execution concern.

The payload set is pinned by `manifest.lock.json` to the public
`microsoft/secureboot_objects` firmware release tag `v1.6.5`, resolved to
commit `798cdc513e0c192fe90e99637105748ed3bb4ca5`, corresponding to the
Microsoft-managed "Most Compatible" template
(`Templates/MostCompatible.toml`).

The `v1.6.5` source updates the complete Microsoft DBX metadata file. Its
`aarch64` revocation list is byte-for-byte equivalent to `v1.6.4`, so the
generated `DBX.bin` payload is intentionally unchanged.

The manifest records:

- the exact upstream source file for every checked-in input under `Inputs/`
- the SHA-256 checksum for every checked-in input
- the SHA-256 checksum for each generated payload
- the target revocation architecture (`aarch64`) used when generating `DBX.bin`
- the Secure Boot signature owner GUID embedded in the generated signature lists

The generator script is:

```sh
python3 scripts/generate_microsoft_secure_boot_defaults.py
```

Its behaviour is:

- verify every checked-in input file against `manifest.lock.json`
- fetch any missing input file from the pinned upstream release tag
- regenerate `PK.bin`, `KEK.bin`, `DB.bin`, and `DBX.bin`
- verify the generated payload checksums against `manifest.lock.json`
- only rewrite files whose contents have changed

If you want checksum verification without rewriting anything:

```sh
python3 scripts/generate_microsoft_secure_boot_defaults.py --check
```

If you want an offline failure instead of downloading missing inputs:

```sh
python3 scripts/generate_microsoft_secure_boot_defaults.py --check --no-fetch
```

Custom firmware builds invoke this generator automatically, so if the
generated payloads are missing they will be recreated in place. The build still
commits both the Microsoft source inputs and the generated payloads so normal
builds do not rely on network access.

Custom firmware builds also run a release-freshness check against the upstream
tags and warn if a newer unsigned firmware release exists. You can run that
manually with:

```sh
make -C src check-microsoft-secure-boot-release
```

By default this is advisory. To make the check fail when a newer release exists
or when the remote tag query cannot be completed:

```sh
make -C src check-microsoft-secure-boot-release \
  MICROSOFT_SECURE_BOOT_RELEASE_CHECK_MODE=strict
```

To validate that a completed custom firmware build embeds these exact Microsoft
payloads rather than the original CIX defaults:

```sh
make -C src validate-secure-boot-defaults ARTEFACT_MODE=custom
```
