#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
act_bootstrap="${script_dir}/ensure_act.sh"
act_cache_home="${EDK2_CIX_ACT_XDG_CACHE_HOME:-${repo_root}/.buildbox/act-cache}"
default_container_arch="${EDK2_CIX_ACT_CONTAINER_ARCH:-linux/amd64}"
default_runner_image="${EDK2_CIX_ACT_RUNNER_IMAGE:-catthehacker/ubuntu:act-latest}"
use_defaults=1

usage() {
    cat <<'EOF'
usage: scripts/run_github_actions_with_act.sh [--no-defaults] [act-args...]

With no arguments, this defaults to `act -l`.

Repo defaults:
- bootstrap a pinned `act` binary under `.buildbox/tools/act/`
- keep `act` cache files under `.buildbox/act-cache/`
- map `ubuntu-latest` to `catthehacker/ubuntu:act-latest`
- default `--container-architecture` to `linux/amd64`

Use `--no-defaults` if you want to pass the runner image and architecture
options yourself.
EOF
}

status() {
    printf '[act-runner] %s\n' "$*"
}

while (( $# > 0 )); do
    case "$1" in
        --no-defaults)
            use_defaults=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            break
            ;;
    esac
done

mkdir -p "$act_cache_home"
export XDG_CACHE_HOME="$act_cache_home"

act_bin="$("$act_bootstrap")"

args=()
if (( use_defaults )); then
    args+=(
        --container-architecture "$default_container_arch"
        -P "ubuntu-latest=$default_runner_image"
    )
fi

if (( $# == 0 )); then
    args+=(-l)
else
    args+=("$@")
fi

status "Using ${act_bin}"
status "Cache root: ${XDG_CACHE_HOME}"

cd "$repo_root"
"$act_bin" "${args[@]}"
