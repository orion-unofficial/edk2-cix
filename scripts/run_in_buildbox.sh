#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
host_os="${EDK2_CIX_HOST_OS:-$(uname -s 2>/dev/null || printf 'unknown\n')}"
workspace_parent="${EDK2_CIX_WORKSPACE_PARENT:-/workspaces}"
workspace_path="${EDK2_CIX_WORKSPACE_ROOT:-${workspace_parent}/$(basename -- "$repo_root")}"
host_workspace_root="$repo_root"
container_tmpdir="${EDK2_CIX_CONTAINER_TMPDIR:-/hosttmp}"
host_tmpdir="${EDK2_CIX_HOST_TMPDIR:-${repo_root}/.buildbox/tmp}"
host_tmpdir="${host_tmpdir%/}"
if [[ -z "$host_tmpdir" ]]; then
    host_tmpdir="${repo_root}/.buildbox/tmp"
fi

container_name="${EDK2_CIX_BUILDBOX_NAME:-edk2-cix-buildbox}"
container_image="${EDK2_CIX_BUILDBOX_IMAGE:-}"
container_runtime="${EDK2_CIX_CONTAINER_RUNTIME:-}"
container_platform="${EDK2_CIX_BUILDBOX_PLATFORM:-}"
dep_profile="${EDK2_CIX_DEP_PROFILE:-firmware}"
buildbox_image_label="edk2-cix.buildbox.image"
buildbox_platform_label="edk2-cix.buildbox.platform"
container_mount_args=()
expected_mounts=()
host_git_repo_usable=0

if [[ "${EDK2_CIX_REPO_LOCK_HELD:-0}" != "1" ]]; then
    exec "${script_dir}/with_repo_lock.sh" "${script_dir}/run_in_buildbox.sh" "$@"
fi

status() {
    printf '[buildbox] %s\n' "$*"
}

resolve_container_runtime() {
    if [[ -n "$container_runtime" ]]; then
        printf '%s\n' "$container_runtime"
        return 0
    fi

    local host_os preferred_runtime fallback_runtime candidate
    local -a probe_failures=()

    host_os="$(uname -s)"
    case "$host_os" in
        Darwin)
            preferred_runtime="docker"
            fallback_runtime="podman"
            ;;
        Linux)
            preferred_runtime="podman"
            fallback_runtime="docker"
            ;;
        *)
            preferred_runtime="podman"
            fallback_runtime="docker"
            ;;
    esac

    for candidate in "$preferred_runtime" "$fallback_runtime"; do
        if ! command -v "$candidate" >/dev/null 2>&1; then
            continue
        fi
        if "$candidate" info >/dev/null 2>&1; then
            if [[ "$candidate" != "$preferred_runtime" ]] && command -v "$preferred_runtime" >/dev/null 2>&1; then
                status "Preferred runtime ${preferred_runtime} was not usable; falling back to ${candidate}"
            fi
            printf '%s\n' "$candidate"
            return 0
        fi
        probe_failures+=("$candidate")
    done

    if (( ${#probe_failures[@]} > 0 )); then
        cat >&2 <<EOF
[buildbox] Tried the available container runtimes in this order: ${preferred_runtime}, ${fallback_runtime}
[buildbox] None of the installed runtimes responded successfully to 'info': ${probe_failures[*]}
[buildbox] Start the preferred runtime or set EDK2_CIX_CONTAINER_RUNTIME explicitly if you want to force one.
EOF
        exit 1
    fi

    cat >&2 <<'EOF'
[buildbox] Neither podman nor docker is available on PATH.
EOF
    exit 1
}

runtime() {
    "$container_runtime" "$@"
}

record_bind_mount() {
    local source="$1"
    local target="$2"
    local options="${3:-}"
    local mount_spec existing

    if [[ ! -e "$source" ]]; then
        return 0
    fi

    mount_spec="${target}=${source}"
    for existing in "${expected_mounts[@]:-}"; do
        if [[ "$existing" == "$mount_spec" ]]; then
            return 0
        fi
    done

    expected_mounts+=("$mount_spec")
    if [[ -n "$options" ]]; then
        container_mount_args+=(-v "${source}:${target}:${options}")
    else
        container_mount_args+=(-v "${source}:${target}")
    fi
}

resolve_git_common_dir() {
    local common_dir

    common_dir="$(git -C "$repo_root" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"
    if [[ -n "$common_dir" ]]; then
        printf '%s\n' "$common_dir"
        return 0
    fi

    common_dir="$(git -C "$repo_root" rev-parse --git-common-dir 2>/dev/null || true)"
    if [[ -z "$common_dir" ]]; then
        return 1
    fi
    if [[ "$common_dir" == /* ]]; then
        printf '%s\n' "$common_dir"
        return 0
    fi

    (
        cd "$repo_root"
        cd "$common_dir" 2>/dev/null && pwd -P
    )
}

prepare_container_mounts() {
    local workspace_root_real git_path git_path_real
    local -a extra_git_paths=()

    container_mount_args=()
    expected_mounts=()
    host_git_repo_usable=0
    record_bind_mount "$host_workspace_root" "$workspace_path"
    record_bind_mount "$host_tmpdir" "$container_tmpdir"

    if ! git -C "$repo_root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        return 0
    fi
    host_git_repo_usable=1

    workspace_root_real="$(cd "$host_workspace_root" && pwd -P)"

    while IFS= read -r git_path; do
        [[ -n "$git_path" ]] || continue
        [[ -d "$git_path" ]] || continue
        git_path_real="$(cd "$git_path" && pwd -P)"
        case "${git_path_real}/" in
            "${workspace_root_real}/"*)
                continue
                ;;
        esac
        extra_git_paths+=("$git_path")
    done < <(
        git -C "$repo_root" rev-parse --absolute-git-dir 2>/dev/null || true
        resolve_git_common_dir 2>/dev/null || true
    )

    if (( ${#extra_git_paths[@]} > 0 )); then
        status "Exposing external git metadata inside the buildbox so source metadata can resolve"
    fi

    for git_path in "${extra_git_paths[@]}"; do
        record_bind_mount "$git_path" "$git_path" ro
    done
}

normalize_arch() {
    case "$1" in
        x86_64|amd64)
            printf 'amd64\n'
            ;;
        aarch64|arm64)
            printf 'arm64\n'
            ;;
        *)
            printf '%s\n' "$1"
            ;;
    esac
}

default_container_platform() {
    case "$(normalize_arch "$(uname -m)")" in
        amd64)
            printf 'linux/amd64\n'
            ;;
        arm64)
            printf 'linux/arm64\n'
            ;;
        *)
            cat >&2 <<EOF
[buildbox] Unsupported host architecture: $(uname -m)
[buildbox] Set EDK2_CIX_BUILDBOX_PLATFORM explicitly if your runtime supports a different target platform.
EOF
            exit 1
            ;;
    esac
}

default_container_image() {
    case "$1" in
        linux/arm64)
            printf 'mcr.microsoft.com/devcontainers/base:trixie\n'
            ;;
        *)
            printf 'mcr.microsoft.com/devcontainers/base:bookworm\n'
            ;;
    esac
}

container_status() {
    runtime inspect -f '{{.State.Status}}' "$container_name" 2>/dev/null || printf 'missing\n'
}

container_running() {
    [[ "$(runtime inspect -f '{{.State.Running}}' "$container_name" 2>/dev/null || printf 'false\n')" == "true" ]]
}

container_label() {
    local label_key="$1"
    runtime inspect -f "{{index .Config.Labels \"$label_key\"}}" "$container_name" 2>/dev/null || true
}

report_container_start_failure() {
    local current_status exit_code runtime_error recent_logs host_arch
    current_status="$(container_status)"
    exit_code="$(runtime inspect -f '{{.State.ExitCode}}' "$container_name" 2>/dev/null || printf 'unknown\n')"
    runtime_error="$(runtime inspect -f '{{.State.Error}}' "$container_name" 2>/dev/null || true)"
    recent_logs="$(runtime logs --tail 20 "$container_name" 2>&1 || true)"
    host_arch="$(normalize_arch "$(uname -m)")"

    cat >&2 <<EOF
[buildbox] Container ${container_name} failed to stay running.
[buildbox] image: ${container_image}
[buildbox] platform: ${container_platform}
[buildbox] state: ${current_status}
[buildbox] exit code: ${exit_code}
EOF

    if [[ -n "$runtime_error" && "$runtime_error" != "<no value>" ]]; then
        printf '[buildbox] runtime error: %s\n' "$runtime_error" >&2
    fi

    if [[ -n "${recent_logs//[$'\t\r\n ']}" ]]; then
        printf '[buildbox] recent container logs:\n%s\n' "$recent_logs" >&2
    fi

    if [[ "$container_platform" == "linux/amd64" && "$host_arch" == "arm64" ]]; then
        cat >&2 <<'EOF'
[buildbox] This usually means the host cannot execute amd64 containers yet.
[buildbox] Leave EDK2_CIX_BUILDBOX_PLATFORM unset for the native arm64/Trixie buildbox path.
[buildbox] For exact upstream replay on arm64 hosts, keep EDK2_CIX_BUILDBOX_PLATFORM=linux/amd64 and ensure x86_64 emulation/binfmt support is configured for podman or docker.
EOF
    fi

    exit 1
}

wait_for_container_running() {
    local current_status
    for _attempt in {1..20}; do
        current_status="$(container_status)"
        case "$current_status" in
            running)
                return 0
                ;;
            created|configured|starting)
                sleep 0.25
                ;;
            *)
                break
                ;;
        esac
    done

    if container_running; then
        return 0
    fi

    report_container_start_failure
}

verify_workspace() {
    if ! runtime exec -w "$workspace_path" "$container_name" test -f Makefile; then
        cat >&2 <<EOF
[buildbox] Expected the repo root to be mounted at ${workspace_path}, but Makefile is missing there.
[buildbox] Check EDK2_CIX_WORKSPACE_ROOT/EDK2_CIX_WORKSPACE_PARENT, the host workspace bind mount, and the checkout path.
EOF
        exit 1
    fi

    if ! runtime exec -w "$workspace_path" "$container_name" test -f src/Makefile; then
        cat >&2 <<EOF
[buildbox] Expected src/Makefile inside ${workspace_path}, but it was not found.
[buildbox] This usually means the wrong host directory was mounted into the build container.
EOF
        exit 1
    fi
}

container_git_repo_usable() {
    runtime exec -w "$workspace_path" "$container_name" \
        git -C "$workspace_path" rev-parse --is-inside-work-tree >/dev/null 2>&1
}

ensure_git_safe_directory() {
    local git_error

    if [[ "$host_git_repo_usable" != "1" ]]; then
        return 0
    fi

    if container_git_repo_usable; then
        return 0
    fi

    status "Marking ${workspace_path} as a safe Git directory inside ${container_name}"
    if ! runtime exec -w "$workspace_path" "$container_name" \
        git config --global --add safe.directory "$workspace_path" >/dev/null 2>&1; then
        git_error="$(runtime exec -w "$workspace_path" "$container_name" \
            git config --global --add safe.directory "$workspace_path" 2>&1 || true)"
        cat >&2 <<EOF
[buildbox] Failed to mark ${workspace_path} as a safe Git directory inside ${container_name}.
${git_error}
EOF
        exit 1
    fi

    if container_git_repo_usable; then
        return 0
    fi

    git_error="$(runtime exec -w "$workspace_path" "$container_name" \
        git -C "$workspace_path" rev-parse --is-inside-work-tree 2>&1 || true)"
    cat >&2 <<EOF
[buildbox] Git still cannot access the mounted repo at ${workspace_path} inside ${container_name}.
[buildbox] This prevents build metadata from resolving correctly.
${git_error}
EOF
    exit 1
}

ensure_container() {
    if runtime container inspect "$container_name" >/dev/null 2>&1; then
        local mounts existing_image existing_platform expected_mount
        mounts="$(runtime inspect -f '{{range .Mounts}}{{printf "%s=%s\n" .Destination .Source}}{{end}}' "$container_name")"
        existing_image="$(container_label "$buildbox_image_label")"
        existing_platform="$(container_label "$buildbox_platform_label")"
        for expected_mount in "${expected_mounts[@]}"; do
            if ! grep -Fxq "$expected_mount" <<<"$mounts"; then
                status "Recreating ${container_name} with the expected workspace mounts and buildbox settings"
                runtime rm -f "$container_name" >/dev/null
                break
            fi
        done
        if ! runtime container inspect "$container_name" >/dev/null 2>&1; then
            :
        elif [[ "$existing_image" != "$container_image" ]] || \
            [[ "$existing_platform" != "$container_platform" ]]; then
            status "Recreating ${container_name} with the expected workspace mounts and buildbox settings"
            runtime rm -f "$container_name" >/dev/null
        elif ! container_running; then
            status "Starting existing container ${container_name}"
            if ! runtime start "$container_name" >/dev/null; then
                report_container_start_failure
            fi
            wait_for_container_running
            return 0
        else
            return 0
        fi
    fi

    status "Creating container ${container_name}"
    runtime run -d \
        --name "$container_name" \
        --label "${buildbox_image_label}=${container_image}" \
        --label "${buildbox_platform_label}=${container_platform}" \
        --platform "$container_platform" \
        --ulimit nofile=1024:524288 \
        "${container_mount_args[@]}" \
        -w "$workspace_path" \
        "$container_image" \
        sleep infinity >/dev/null
    wait_for_container_running
}

while (( $# > 0 )); do
    case "$1" in
        --dep-profile)
            shift
            if (( $# == 0 )); then
                cat >&2 <<'EOF'
usage: scripts/run_in_buildbox.sh [--dep-profile firmware|packaging] <command> [args...]
EOF
                exit 2
            fi
            dep_profile="$1"
            shift
            ;;
        --)
            shift
            break
            ;;
        *)
            break
            ;;
    esac
done

case "$dep_profile" in
    firmware|packaging)
        ;;
    *)
        cat >&2 <<EOF
[buildbox] Unsupported dependency profile: ${dep_profile}
EOF
        exit 2
        ;;
esac

if (( $# == 0 )); then
    cat >&2 <<'EOF'
usage: scripts/run_in_buildbox.sh [--dep-profile firmware|packaging] <command> [args...]
EOF
    exit 2
fi

container_runtime="$(resolve_container_runtime)"
container_platform="${container_platform:-$(default_container_platform)}"
container_image="${container_image:-$(default_container_image "$container_platform")}"
status "Using container runtime: ${container_runtime}"
status "Using container platform: ${container_platform}"
status "Using container image: ${container_image}"
mkdir -p "$host_tmpdir"
host_tmpdir="$(cd "$host_tmpdir" && pwd -P)"
prepare_container_mounts
ensure_container
verify_workspace

status "Ensuring ${dep_profile} build dependencies are present"
runtime exec -w "$workspace_path" "$container_name" bash -lc "./scripts/ensure_build_deps.sh --profile ${dep_profile}"
ensure_git_safe_directory

status "Running in ${container_name}: $*"
runtime exec \
    -e "EDK2_CIX_REPO_LOCK_HELD=${EDK2_CIX_REPO_LOCK_HELD:-}" \
    -e "EDK2_CIX_HOST_OS=${host_os}" \
    -w "$workspace_path" \
    "$container_name" \
    "$@"
