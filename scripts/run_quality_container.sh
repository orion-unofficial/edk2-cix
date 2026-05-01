#!/bin/sh
set -eu

mode="${1:-test}"
repo="$(git rev-parse --show-toplevel)"
image="${QUALITY_IMAGE:-edk2-cix-build-quality:latest}"
verbose="${V:-0}"
git_common="$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)"

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
    --volume "$repo:$repo" \
    --workdir "$repo"

case "$git_common" in
    "$repo"/*) ;;
    *) set -- "$@" --volume "$git_common:$git_common" ;;
esac

printf '[quality] Running %s checks in %s\n' "$mode" "$image" >&2
exec "$@" "$image" "$mode"
