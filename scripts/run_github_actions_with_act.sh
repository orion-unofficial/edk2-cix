#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
act_bootstrap="${script_dir}/ensure_act.sh"
act_cache_home="${EDK2_CIX_ACT_XDG_CACHE_HOME:-${repo_root}/.cache/edk2-cix/act-cache}"
default_runner_image="${ACT_RUNNER_IMAGE:-${EDK2_CIX_ACT_RUNNER_IMAGE:-catthehacker/ubuntu:act-latest@sha256:5aae110fc7ae93fb2b4b1a07d8acde4574ffc7235325f7acc13197f66f334c68}}"

detect_container_arch() {
    case "$(uname -m)" in
        arm64|aarch64)
            printf '%s\n' "linux/arm64"
            ;;
        x86_64|amd64)
            printf '%s\n' "linux/amd64"
            ;;
        *)
            printf '%s\n' "linux/amd64"
            ;;
    esac
}

resolve_container_arch() {
    local requested="$1"

    case "$requested" in
        ""|auto)
            detect_container_arch
            ;;
        *)
            printf '%s\n' "$requested"
            ;;
    esac
}

default_container_arch="$(resolve_container_arch "${ACT_CONTAINER_ARCH:-${EDK2_CIX_ACT_CONTAINER_ARCH:-auto}}")"

usage() {
    cat <<'EOF'
usage: scripts/run_github_actions_with_act.sh list|dry-run|run [act-args...]

Environment:
  ACT_WORKFLOW=.github/workflows/<file>.yaml
      Workflow file to list, dry-run, or execute. Optional for list.
  ACT_EVENT=workflow_dispatch|push|pull_request|schedule
      Event passed to act for dry-run/run. Default: workflow_dispatch.
  ACT_JOB=<job-id>
      Optional job filter.
  ACT_MATRIX=<name:value>
      Optional single matrix filter, for example board:O6.
  ACT_SECRET_FILE=<path>
      Optional act --secret-file path.
  ACT_CONTAINER_ARCH=auto|<platform>
      Container architecture. Default: auto-detected from the host.
  ACT_RUNNER_IMAGE=<image>
      Runner image for ubuntu-latest. Default: catthehacker/ubuntu:act-latest@sha256:5aae110fc7ae93fb2b4b1a07d8acde4574ffc7235325f7acc13197f66f334c68.
  ACT_EXTRA_ARGS=<args>
      Additional raw flags appended to act.
EOF
}

status() {
    printf '[act-runner] %s\n' "$*"
}

mode="${1:-list}"
case "$mode" in
    -h|--help)
        usage
        exit 0
        ;;
    list|dry-run|run)
        shift || true
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

mkdir -p "$act_cache_home"
export XDG_CACHE_HOME="$act_cache_home"

act_bin="$("$act_bootstrap")"

args=(
    --container-architecture "$default_container_arch"
    -P "ubuntu-latest=$default_runner_image"
)

workflow="${ACT_WORKFLOW:-}"
event="${ACT_EVENT:-workflow_dispatch}"
job="${ACT_JOB:-}"
matrix="${ACT_MATRIX:-}"
secret_file="${ACT_SECRET_FILE:-}"

if [[ -n "$workflow" ]]; then
    args+=(-W "$workflow")
fi
if [[ -n "$job" ]]; then
    args+=(-j "$job")
fi
if [[ -n "$matrix" ]]; then
    args+=(--matrix "$matrix")
fi
if [[ -n "$secret_file" ]]; then
    args+=(--secret-file "$secret_file")
fi
if [[ -n "${ACT_EXTRA_ARGS:-}" ]]; then
    # shellcheck disable=SC2206
    extra_args=(${ACT_EXTRA_ARGS})
    args+=("${extra_args[@]}")
fi

case "$mode" in
    list)
        args+=(-l)
        ;;
    dry-run)
        if [[ -z "$workflow" ]]; then
            printf '[act-runner] Set ACT_WORKFLOW=.github/workflows/<file>.yaml for dry-run.\n' >&2
            exit 2
        fi
        args+=(--dryrun "$event")
        ;;
    run)
        if [[ -z "$workflow" ]]; then
            printf '[act-runner] Set ACT_WORKFLOW=.github/workflows/<file>.yaml for run.\n' >&2
            exit 2
        fi
        args+=("$event")
        ;;
esac

args+=("$@")

status "Using ${act_bin}"
status "Cache root: ${XDG_CACHE_HOME}"
status "Container architecture: ${default_container_arch}"

cd "$repo_root"
"$act_bin" "${args[@]}"
