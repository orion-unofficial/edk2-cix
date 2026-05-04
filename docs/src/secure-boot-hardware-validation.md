# Secure Boot Hardware Validation

Use this runbook after building a `custom` firmware image that embeds the
Microsoft Secure Boot defaults.

This is the real-hardware complement to the repo-local checks:

- `make -C src check-microsoft-secure-boot-defaults`
- `make -C src validate-secure-boot-defaults-<board>`

Those build-time checks prove that the firmware image contains the expected
payloads. They do not prove that a flashed board has enrolled them, that the
board is in the expected Secure Boot mode, or that Microsoft-signed boot media
actually runs.

## Scope

This checklist is intentionally focused on the behaviour we care about for this
repo:

- first-boot default enrollment from the embedded Microsoft payloads
- recovery back to Setup Mode
- Microsoft-signed Linux shim boot
- Windows boot with Secure Boot enabled
- `dbx` update behaviour and regressions

It does not try to replace full UEFI-SCT, SystemReady, or FWTS coverage.

## Prerequisites

Before flashing a board:

1. Build the custom firmware for the target board.
2. Run `make -C src validate-secure-boot-defaults-<board>`.
3. Keep a recovery path ready before testing:
   - a known-good previous firmware image
   - the board's usual flash/recovery method
   - serial console capture if available
4. Prepare test media:
   - a removable device with a Microsoft-signed Linux shim
   - a Windows on Arm installer or known-good Windows boot medium if Windows
     boot is in scope
   - at least one unsigned or otherwise untrusted EFI binary for a negative
     control

Important operational note:

- flashing a new firmware image does not necessarily clear the existing Secure
  Boot variables
- if NVRAM is preserved, a board may stay in User Mode with previously enrolled
  keys until the keys or variable store are cleared explicitly

## Baseline Capture

On the first boot into the test firmware, record:

1. Firmware version and board.
2. Whether the setup UI reports Setup Mode or User Mode.
3. Whether Secure Boot is currently disabled or enabled.
4. Serial-console logs for the first boot after flashing, if available.

If the board is unexpectedly already in User Mode, do not assume the embedded
defaults were just enrolled. Clear the keys first and repeat the flow below.

## Test 1: Fresh Default Enrollment

Goal:

- prove that a board in Setup Mode enrolls the embedded Microsoft defaults
  without manual certificate upload

Recommended flow:

1. Start from a board with cleared Secure Boot keys or a cleared variable
   store.
2. Boot into firmware setup and confirm the platform is in Setup Mode.
3. Enable Secure Boot if the platform requires that setting to transition into
   User Mode.
4. Reboot once.
5. Re-enter setup or a UEFI shell and confirm the platform is now in User
   Mode.

Expected result:

- Secure Boot transitions cleanly into User Mode
- no manual key import step is required
- subsequent Microsoft-signed boot media is accepted

Useful follow-up checks if your setup exposes them:

- inspect `PK`, `KEK`, `db`, and `dbx` with `dmpstore` or an equivalent UEFI
  variable viewer
- confirm the variables exist after the first enrollment boot

## Test 2: Negative Control

Goal:

- prove that Secure Boot is actually enforcing policy rather than merely being
  enabled in the UI

Recommended flow:

1. Leave Secure Boot enabled after the fresh-enrollment step.
2. Attempt to boot an unsigned EFI binary or another binary that is not trusted
   by the enrolled database.

Expected result:

- the platform blocks that binary
- the failure is visible either in the setup UI, boot manager, or serial log

If the unsigned binary still boots, treat that as a Secure Boot failure even if
the platform claims Secure Boot is enabled.

## Test 3: Microsoft-Signed Shim Boot

Goal:

- prove that the embedded Microsoft trust chain is sufficient for a standard
  Linux shim path

Recommended flow:

1. Boot a removable device whose shim is signed by Microsoft.
2. Continue through the distro's normal boot path.

Expected result:

- the shim binary loads without manual key enrollment
- the normal distro boot chain continues

If the shim binary is blocked:

1. re-check that the board really transitioned into User Mode after default
   enrollment
2. re-run `validate-secure-boot-defaults-<board>` on the exact firmware image
   that was flashed
3. confirm the board is running the `custom` firmware rather than an
   `upstream` build

## Test 4: Windows Boot With Secure Boot Enabled

Goal:

- prove that the default Microsoft trust chain is sufficient for a Windows boot
  path without manual key upload

Recommended flow:

1. Boot a Windows on Arm installer or an existing Windows installation with
   Secure Boot still enabled.
2. Observe whether the Windows boot manager starts normally.

Expected result:

- Windows boot components signed by Microsoft load without requiring any manual
  certificate import

Important limitation:

- this only validates the Secure Boot trust path
- it does not prove full Windows-on-this-board support beyond boot-chain trust

## Test 5: Clear Back To Setup Mode

Goal:

- prove that the board can be returned to Setup Mode cleanly and re-enrolled

Recommended flow:

1. Use the firmware setup option that clears Secure Boot keys, if present.
2. If no UI path exists, use the board's documented variable-store reset or
   recovery method.
3. Reboot and confirm the platform returns to Setup Mode.
4. Re-enable Secure Boot and confirm the default-enrollment flow still works.

Expected result:

- the board returns to Setup Mode after key clearing
- the embedded defaults can be re-enrolled on the next enablement cycle

Important nuance:

- simply flashing a different firmware image may not clear the active Secure
  Boot variables if the variable store is preserved

## Test 6: `dbx` Update Behaviour

Goal:

- prove that the embedded `dbx` payload does not regress normal Microsoft boot
  media, and that later revocation updates do not break the expected path

Recommended flow:

1. Record the embedded `dbx` checksum from the build manifest before flashing.
2. Confirm that the known-good Microsoft-signed shim and Windows paths still
   boot with the embedded `dbx`.
3. If you have a later Microsoft `dbx` update path available, apply it through
   the normal OS or firmware tooling.
4. Repeat the shim and Windows boot checks after the update.
5. If you have a known revoked test binary, confirm it is blocked after the
   relevant `dbx` is active.

Expected result:

- current trusted Microsoft boot media continues to work
- known revoked binaries remain blocked when you have a suitable test sample

## Suggested Evidence To Capture

For each board and firmware revision, keep:

- flashed firmware commit or package version
- whether the board started in Setup Mode or User Mode
- serial logs for first enrollment and one successful Microsoft boot
- screenshots or notes from the setup UI showing Secure Boot state
- the exact shim or Windows media used
- the result of the unsigned-binary negative control
- any `dbx` update package or revision used during testing

## Failure Triage

If enrollment does not happen:

1. Confirm the board is really back in Setup Mode.
2. Confirm the flashed image was built with `ARTEFACT_MODE=custom`.
3. Re-run the repo-local Secure Boot validators on the exact image that was
   flashed.

If enrollment succeeds but shim or Windows still fails:

1. Treat that as a trust-chain or runtime-path problem, not an enrollment
   problem.
2. Re-check the test media signatures and boot path.
3. Capture serial logs and the exact failure point for later comparison.

If flashing a different firmware does not change Secure Boot behaviour:

1. Assume the variable store is being preserved.
2. Clear keys or reset the variable store explicitly before re-testing.
