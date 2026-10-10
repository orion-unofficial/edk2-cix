# Firmware build validation

The primary custom targets are the vendor baseline EDK2 `202208` and the latest
EDK2 release selected in `config/policies.json`. EDK2 `202605` is an ordinary
historical release. Run `make help-source-targets` to inspect available targets and
the maintained qualification matrix. CI derives that matrix from the live
source manifests and policy, rather than a stored inventory of past builds.

A successful build applies to its exact source, board, toolchain and options.
It does not establish that another source revision or configuration compiles,
or that an image boots and operates correctly on hardware. In particular,
source-built CIX trusted firmware and StandaloneMM require a hardware boot and
variable-persistence check after build validation.

Each generated image must pass its image-size, flash-layout, BL1 payload and
signature-chain checks. Keep the image hash and generated validation reports
with the output being tested. Source-policy and rendered-tree checks establish
that the selected inputs match the requested target; they do not replace a
firmware build or hardware testing.

Stock byte-identical replay uses the EDK2 `202208` vendor source, pinned replay
toolchain, timestamps and signing inputs. Compare the generated artifacts with
the published Radxa package. A successful compile alone does not establish a
byte-identical replay. The supported replay workflow obtains its reference
artifacts from the vendor package rather than historical qualification reports.

See [building firmware](build.md), [BL1 validation](bootloader1-validation.md),
[firmware-chain validation](firmware-chain-validation.md) and
[StandaloneMM](standalone-mm.md) for the current commands and constraints.
