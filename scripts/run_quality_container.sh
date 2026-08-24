#!/bin/sh
set -eu

mode="${1:-test}"
repo="$(git rev-parse --show-toplevel)"
image="${QUALITY_IMAGE:-edk2-cix-build-quality:latest}"
verbose="${V:-0}"
git_common="$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)"
shared_temp_root="$(dirname -- "$git_common")/.worktrees/edk2-cix-tmp"
pycache_prefix="${PYTHONPYCACHEPREFIX:-$repo/.cache/edk2-cix/pycache}"

printf '[quality] Building quality container image: %s\n' "$image" >&2

set -- docker build \
    --file "$repo/scripts/quality.Dockerfile" \
    --tag "$image"

if [ "$verbose" = "1" ]; then
    set -- "$@" --progress=plain
fi

"$@" "$repo"

set -- docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env GIT_CONFIG_COUNT=1 \
    --env GIT_CONFIG_KEY_0=safe.directory \
    --env "GIT_CONFIG_VALUE_0=$repo" \
    --env "PYTHONPYCACHEPREFIX=$pycache_prefix" \
    --volume "$repo:$repo" \
    --workdir "$repo"

case "$git_common" in
    "$repo"/*) ;;
    *)
        mkdir -p "$shared_temp_root"
        set -- "$@" \
            --volume "$git_common:$git_common" \
            --volume "$shared_temp_root:$shared_temp_root"
        ;;
esac

printf '[quality] Running %s checks in %s\n' "$mode" "$image" >&2
exec "$@" "$image" "$mode"
