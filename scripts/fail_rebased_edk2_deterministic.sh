#!/usr/bin/env bash
set -euo pipefail

printf '%s\n' '[deterministic] Byte-accurate replay and validation are disabled on main-monorepo-edk2.' >&2
printf '%s\n' '[deterministic] Deterministic builds are not valid when the EDK2 implementation has been rebased onto a newer upstream tree.' >&2
printf '%s\n' '[deterministic] Use main-monorepo for byte-accurate vendor replay, or use ordinary trixie buildbox builds on main-monorepo-edk2.' >&2
exit 2
