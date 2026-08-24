#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
act_bootstrap="${script_dir}/ensure_act.sh"
act_cache_home="${EDK2_CIX_ACT_XDG_CACHE_HOME:-${repo_root}/.cache/edk2-cix/act-cache}"
default_runner_image="${ACT_RUNNER_IMAGE:-${EDK2_CIX_ACT_RUNNER_IMAGE:-ghcr.io/catthehacker/ubuntu:act-24.04-20260815}}"

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
git_common_dir="$(git -C "$repo_root" rev-parse --path-format=absolute --git-common-dir)"
act_workdir="$repo_root"
act_workspace=""
if [[ "$git_common_dir" != "$repo_root"/* ]]; then
    if [[ -n "$(git -C "$repo_root" status --porcelain --untracked-files=normal)" ]]; then
        printf '[act-runner] Linked-worktree runs require a clean checkout so the isolated CI snapshot cannot omit local changes.\n' >&2
        exit 2
    fi
    mkdir -p "${repo_root}/.cache/edk2-cix/act-workspaces"
    act_workspace="$(mktemp -d "${repo_root}/.cache/edk2-cix/act-workspaces/run.XXXXXX")"
    trap 'rm -rf -- "$act_workspace"' EXIT
    git clone --quiet --shared --no-checkout "$git_common_dir" "$act_workspace"
    current_branch="$(git -C "$repo_root" symbolic-ref --quiet --short HEAD || printf '%s' act-local)"
    git -C "$act_workspace" checkout --quiet --force -B "$current_branch" "$(git -C "$repo_root" rev-parse HEAD)"
    while read -r object_id ref; do
        git -C "$act_workspace" update-ref "refs/heads/${ref#refs/remotes/origin/}" "$object_id"
    done < <(git -C "$repo_root" for-each-ref --format='%(objectname) %(refname)' refs/remotes/origin/source)
    while read -r object_id ref; do
        git -C "$act_workspace" update-ref "$ref" "$object_id"
    done < <(git -C "$repo_root" for-each-ref --format='%(objectname) %(refname)' refs/heads/source)
    git -C "$act_workspace" remote set-url origin "${act_workspace}/.git"
    act_workdir="$act_workspace"
fi

origin_url="$(git -C "$act_workdir" remote get-url origin 2>/dev/null || true)"
local_origin=""
case "$origin_url" in
    file://*) local_origin="${origin_url#file://}" ;;
    /*) local_origin="$origin_url" ;;
    *://*|*@*:*|*:*) ;;
    ?*) local_origin="${act_workdir}/${origin_url}" ;;
esac
if [[ -n "$local_origin" && -d "$local_origin" ]]; then
    local_origin="$(cd -- "$local_origin" && pwd -P)"
else
    local_origin=""
fi

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
      Runner image for ubuntu-latest. Default: ghcr.io/catthehacker/ubuntu:act-24.04-20260815.
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
if [[ -n "$act_workspace" ]]; then
    args+=(--bind)
fi

container_options=""
if [[ "$git_common_dir" != "$repo_root"/* ]]; then
    container_options="--volume=${git_common_dir}:${git_common_dir}:ro"
fi
if [[ -n "$local_origin" && "$local_origin" != "$act_workdir"/* && "$local_origin" != "$git_common_dir" && "$local_origin" != "$git_common_dir"/* ]]; then
    container_options="${container_options:+${container_options} }--volume=${local_origin}:${local_origin}:ro"
fi
if [[ -n "$container_options" ]]; then
    args+=(--container-options "$container_options")
fi

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
if [[ -n "$act_workspace" ]]; then
    status "Isolated linked-worktree snapshot: ${act_workspace}"
    status "Shared object store mount: ${git_common_dir} (read-only)"
fi
if [[ -n "$local_origin" && "$local_origin" != "$act_workdir"/* && "$local_origin" != "$git_common_dir" && "$local_origin" != "$git_common_dir"/* ]]; then
    status "Local origin mount: ${local_origin} (read-only)"
fi

cd "$act_workdir"
"$act_bin" "${args[@]}"
