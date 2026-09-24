# CIX V1.2 signing-key snapshot

These files are copied byte-for-byte from
[`cixtech/edk2-non-osi` commit `94368c7c2f254528634fdb518e21545bf6c8f0b1`](https://github.com/cixtech/edk2-non-osi/tree/94368c7c2f254528634fdb518e21545bf6c8f0b1/Platform/CIX/Sky1/PackageTool/Keys).
They are used only by the custom `CIX_RELEASE=1.2` TF-A/OP-TEE build.
Stock Radxa UEFI signing and `ARTEFACT_MODE=upstream` retain their existing inputs.

The source-build preflight pins the private-key file hashes and requires the
CIX root's public key to match the selected stock firmware's trusted-world
anchor. The resulting FIP is checked against that same external reference.
The 2026Q1 BL1 input remains a byte-identical CIX release payload.

These are publicly disclosed private keys. Their presence supports
interoperability experiments but means signatures under these keys alone
cannot establish that code was produced exclusively by CIX. An offline-valid
certificate chain does not prove a particular board's eFuse state, rollback
state, or successful boot.
