#!/usr/bin/env bash
set -euo pipefail
firmware_root="$(cd -- "$(dirname -- "$0")/.." && pwd -P)"
infra_root="${EDK2_CIX_BUILD_INFRA_ROOT:-}"
if [[ -z "$infra_root" ]]; then
    common="$(git -C "$firmware_root" rev-parse --path-format=absolute --git-common-dir)"
    infra_root="$(dirname -- "$common")"
fi
if [[ "$infra_root" == "$firmware_root" || ! -x "$infra_root/scripts/run_in_buildbox.sh" ]]; then
    printf '%s
' 'Set EDK2_CIX_BUILD_INFRA_ROOT to the build infrastructure checkout for container builds.' >&2
    exit 1
fi
export EDK2_CIX_FIRMWARE_ROOT="$firmware_root"
exec "$infra_root/scripts/run_in_buildbox.sh" "$@"
