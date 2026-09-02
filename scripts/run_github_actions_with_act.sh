#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
git_common_dir="$(git -C "$repo_root" rev-parse --path-format=absolute --git-common-dir)"
repository_root="$(dirname -- "$git_common_dir")"
act_bootstrap="${script_dir}/ensure_act.sh"
act_cache_home="${EDK2_CIX_ACT_XDG_CACHE_HOME:-${repository_root}/.cache/edk2-cix/act-cache}"
act_host_cache_root="${EDK2_CIX_ACT_HOST_CACHE_ROOT:-${repository_root}/.cache/edk2-cix}"
default_runner_image="${ACT_RUNNER_IMAGE:-${EDK2_CIX_ACT_RUNNER_IMAGE:-ghcr.io/catthehacker/ubuntu:act-24.04-20260815}}"
concurrent_jobs="${ACT_CONCURRENT_JOBS:-${EDK2_CIX_ACT_CONCURRENT_JOBS:-1}}"
allow_remote_ref_drift="${ACT_ALLOW_REMOTE_REF_DRIFT:-${EDK2_CIX_ACT_ALLOW_REMOTE_REF_DRIFT:-0}}"

case "$concurrent_jobs" in
    ""|0|*[!0-9]*)
        printf '[act-runner] ACT_CONCURRENT_JOBS must be a positive integer, got: %s\n' "$concurrent_jobs" >&2
        exit 2
        ;;
esac

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
act_workdir="$repo_root"
act_workspace=""

cleanup_act_workspace() {
    local exit_status=$? cleanup_status=0 container_id mount_source

    trap - EXIT
    if [[ -z "$act_workspace" || ! -d "$act_workspace" ]]; then
        return "$exit_status"
    fi
    case "$act_workspace" in
        "${repo_root}/.cache/edk2-cix/act-workspaces/run."*) ;;
        *)
            printf '[act-runner] Refusing to remove unexpected act workspace: %s\n' "$act_workspace" >&2
            return 1
            ;;
    esac

    while IFS= read -r container_id; do
        [[ -n "$container_id" ]] || continue
        while IFS= read -r mount_source; do
            case "${mount_source}/" in
                "${act_workspace}/"*)
                    printf '[act-runner] Removing task container %s before isolated-workspace cleanup.\n' "$container_id" >&2
                    if ! docker rm -f "$container_id" >/dev/null; then
                        cleanup_status=1
                        printf '[act-runner] Failed to remove task container %s.\n' "$container_id" >&2
                    fi
                    break
                    ;;
            esac
        done < <(docker inspect --format '{{range .Mounts}}{{println .Source}}{{end}}' "$container_id" 2>/dev/null || true)
    done < <(docker ps -aq 2>/dev/null || true)

    if ! rm -rf -- "$act_workspace"; then
        printf '[act-runner] Retrying isolated-workspace cleanup through Docker to remove container-owned files.\n' >&2
        if docker run --rm \
            --platform "$default_container_arch" \
            --volume "${act_workspace}:${act_workspace}" \
            --entrypoint find \
            "$default_runner_image" \
            "$act_workspace" -mindepth 1 -depth -delete && \
            rmdir -- "$act_workspace"; then
            :
        else
            cleanup_status=1
            printf '[act-runner] Cleanup failed; isolated workspace remains at %s\n' "$act_workspace" >&2
        fi
    fi

    if (( exit_status != 0 )); then
        return "$exit_status"
    fi
    return "$cleanup_status"
}

if [[ "${1:-list}" == run || "$git_common_dir" != "$repo_root"/* ]]; then
    if [[ -n "$(git -C "$repo_root" status --porcelain --untracked-files=normal)" ]]; then
        printf '[act-runner] Isolated act runs require a clean checkout so the CI snapshot cannot omit local changes.\n' >&2
        exit 2
    fi
    mkdir -p "${repo_root}/.cache/edk2-cix/act-workspaces" "$act_host_cache_root"
    act_workspace="$(mktemp -d "${repo_root}/.cache/edk2-cix/act-workspaces/run.XXXXXX")"
    trap cleanup_act_workspace EXIT
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
  ACT_CONCURRENT_JOBS=<count>
      Maximum number of top-level concurrent local jobs. Default: 1.
      Workflows can add their own matrix concurrency, so increase cautiously.
  ACT_RUNNER_IMAGE=<image>
      Runner image for ubuntu-latest. Default: ghcr.io/catthehacker/ubuntu:act-24.04-20260815.
  ACT_ALLOW_REMOTE_REF_DRIFT=0|1
      Permit a deliberately non-equivalent run when source metadata is ahead
      of the real remote. Default: 0 (verify remote coherence before act).
  ACT_EXTRA_ARGS=<args>
      Additional raw flags appended to act.
EOF
}

status() {
    printf '[act-runner] %s\n' "$*"
}

prepare_action_cache() {
    local action_spec action_repository action_ref cache_dir

    mkdir -p "${act_cache_home}/act"
    while IFS= read -r action_spec; do
        case "$action_spec" in
            ./*|docker://*) continue ;;
        esac
        action_repository="${action_spec%@*}"
        action_ref="${action_spec##*@}"
        cache_dir="${act_cache_home}/act/${action_repository//\//-}@${action_ref}"
        if [[ -d "${cache_dir}/.git" ]]; then
            status "Refreshing action ${action_spec} with system Git"
            git -C "$cache_dir" update-ref -d refs/heads/HEAD
            if git -C "$cache_dir" fetch --quiet --depth 1 origin "$action_ref"; then
                git -C "$cache_dir" checkout --quiet --detach FETCH_HEAD
            elif git -C "$cache_dir" rev-parse --verify HEAD >/dev/null 2>&1; then
                status "Action refresh failed; using cached ${action_spec}"
            else
                printf '[act-runner] Action cache is incomplete and refresh failed: %s\n' "$cache_dir" >&2
                return 1
            fi
        elif [[ -e "$cache_dir" ]]; then
            printf '[act-runner] Refusing to replace unexpected action-cache path: %s\n' "$cache_dir" >&2
            return 1
        else
            status "Caching action ${action_spec} with system Git"
            git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$action_ref" \
                "https://github.com/${action_repository}.git" "$cache_dir"
        fi
    done < <(
        sed -nE 's/^[[:space:]]*uses:[[:space:]]+([^#[:space:]]+).*/\1/p' \
            "$act_workdir"/.github/workflows/*.yaml | sort -u
    )
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

if [[ "$mode" != list ]]; then
    case "$allow_remote_ref_drift" in
        1|true|TRUE|yes|YES|on|ON)
            printf '[act-runner] warning: remote source-ref coherence check explicitly bypassed; this run is not proof of GitHub equivalence.\n' >&2
            ;;
        0|false|FALSE|no|NO|off|OFF|'')
            status "Checking real remote source refs before isolated act execution"
            python3 "$script_dir/check_remote_source_coherence.py" --remote origin
            ;;
        *)
            printf '[act-runner] ACT_ALLOW_REMOTE_REF_DRIFT must be a boolean, got: %s\n' "$allow_remote_ref_drift" >&2
            exit 2
            ;;
    esac
    prepare_action_cache
fi

args=(
    --rm
    --concurrent-jobs "$concurrent_jobs"
    --container-architecture "$default_container_arch"
    -P "ubuntu-latest=$default_runner_image"
)
if [[ "$mode" != list ]]; then
    args+=(--action-offline-mode)
fi
if [[ -n "$act_workspace" ]]; then
    args+=(--bind)
    args+=(--env "EDK2_CIX_ACT_HOST_CACHE_ROOT=${act_host_cache_root}")
fi

container_options=""
if [[ -n "$act_workspace" ]]; then
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
status "Concurrent jobs: ${concurrent_jobs}"
if [[ -n "$act_workspace" ]]; then
    status "Isolated CI snapshot: ${act_workspace}"
    status "Shared object store mount: ${git_common_dir} (read-only)"
    status "Persistent local cache: ${act_host_cache_root}"
fi
if [[ -n "$local_origin" && "$local_origin" != "$act_workdir"/* && "$local_origin" != "$git_common_dir" && "$local_origin" != "$git_common_dir"/* ]]; then
    status "Local origin mount: ${local_origin} (read-only)"
fi

cd "$act_workdir"
"$act_bin" "${args[@]}"
