#!/usr/bin/env bash
set -euo pipefail

printf '%s\n' '[deterministic] Byte-accurate replay and validation are disabled on source/unofficial/current.' >&2
printf '%s\n' '[deterministic] Deterministic builds are not valid when the EDK2 implementation has been rebased onto a newer upstream tree.' >&2
printf '%s\n' '[deterministic] Use source/release/custom/edk2-202208/radxa-1.2.1/local for byte-accurate vendor replay, or use ordinary trixie buildbox builds on source/unofficial/current.' >&2
exit 2
