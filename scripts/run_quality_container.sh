#!/bin/sh
set -eu

mode="${1:-test}"
repo="$(git rev-parse --show-toplevel)"
image="${QUALITY_IMAGE:-edk2-cix-build-quality:latest}"
git_common="$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)"
git_objects="$(git -C "$repo" rev-parse --path-format=absolute --git-path objects)"
shared_temp_root="$(dirname -- "$git_common")/.worktrees/edk2-cix-tmp"
pycache_prefix="${PYTHONPYCACHEPREFIX:-$repo/.cache/edk2-cix/pycache}"

printf '[quality] Building quality container image: %s\n' "$image" >&2

set -- docker build \
    --file "$repo/scripts/quality.Dockerfile" \
    --progress=plain \
    --tag "$image"

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
            --volume "$git_common:$git_common:ro" \
            --volume "$shared_temp_root:$shared_temp_root"
        ;;
esac

if [ -f "$git_objects/info/alternates" ]; then
    while IFS= read -r alternate; do
        case "$alternate" in
            /*) ;;
            *) alternate="$git_objects/$alternate" ;;
        esac
        alternate="$(cd -- "$alternate" && pwd -P)"
        set -- "$@" --volume "$alternate:$alternate:ro"
    done < "$git_objects/info/alternates"
fi

printf '[quality] Running %s checks in %s\n' "$mode" "$image" >&2
exec "$@" "$image" "$mode"
