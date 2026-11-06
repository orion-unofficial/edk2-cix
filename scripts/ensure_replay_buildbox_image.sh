#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
runtime_helper="${script_dir}/default_container_runtimes.sh"
verbose="${EDK2_CIX_VERBOSE:-0}"

base_image="${EDK2_CIX_REPLAY_BASE_IMAGE:-debian:bookworm@sha256:1d6cd964917a13b547d1ea392dff9a000c3f36070686ebc5c8755d53fb374435}"
snapshot_timestamp="${EDK2_CIX_REPLAY_SNAPSHOT_TIMESTAMP:-20260406T235959Z}"
image_name="${EDK2_CIX_REPLAY_IMAGE_NAME:-edk2-cix-replay-bookworm:20260406}"
container_runtime="${EDK2_CIX_CONTAINER_RUNTIME:-}"
container_platform="${EDK2_CIX_BUILDBOX_PLATFORM:-}"
dockerfile_path="${repo_root}/containers/replay-bookworm/Dockerfile"

usage() {
    cat <<'EOF'
usage: scripts/ensure_replay_buildbox_image.sh [--image <tag>] [--platform linux/amd64|linux/arm64]
EOF
}

while (( $# > 0 )); do
    case "$1" in
        --image)
            shift
            image_name="$1"
            ;;
        --platform)
            shift
            container_platform="$1"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf '[replay-image] Unsupported argument: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

normalize_arch() {
    case "$1" in
        amd64|x86_64)
            printf 'amd64\n'
            ;;
        arm64|aarch64)
            printf 'arm64\n'
            ;;
        *)
            printf '%s\n' "$1"
            ;;
    esac
}

if [[ -z "$container_platform" ]]; then
    case "$(normalize_arch "$(uname -m)")" in
        amd64)
            container_platform="linux/amd64"
            ;;
        arm64)
            container_platform="linux/arm64"
            ;;
        *)
            printf '[replay-image] Unsupported host architecture: %s\n' "$(uname -m)" >&2
            exit 2
            ;;
    esac
fi

case "$container_platform" in
    linux/amd64)
        replay_host_arch="amd64"
        manifest_path="${repo_root}/containers/replay-bookworm/packages.bookworm-amd64.txt"
        ;;
    linux/arm64)
        replay_host_arch="arm64"
        manifest_path="${repo_root}/containers/replay-bookworm/packages.bookworm-arm64.txt"
        ;;
    *)
        printf '[replay-image] Unsupported platform: %s\n' "$container_platform" >&2
        exit 2
        ;;
esac

if [[ -z "$container_runtime" ]]; then
    while IFS= read -r candidate; do
        [[ -n "$candidate" ]] || continue
        if command -v "$candidate" >/dev/null 2>&1 && "$candidate" info >/dev/null 2>&1; then
            container_runtime="$candidate"
            break
        fi
    done < <(bash "$runtime_helper")
fi

if [[ -z "$container_runtime" ]]; then
    printf '[replay-image] No usable container runtime found.\n' >&2
    exit 1
fi

status() {
    printf '[replay-image] %s\n' "$*"
}

fingerprint="$(
    {
        printf 'base=%s\n' "$base_image"
        printf 'snapshot=%s\n' "$snapshot_timestamp"
        printf 'platform=%s\n' "$container_platform"
        printf 'arch=%s\n' "$replay_host_arch"
        shasum -a 256 "$dockerfile_path" "$manifest_path"
    } | shasum -a 256 | awk '{print $1}'
)"

label_key="edk2-cix.replay-image.fingerprint"
current_fingerprint="$("$container_runtime" image inspect "$image_name" --format "{{index .Config.Labels \"$label_key\"}}" 2>/dev/null || true)"

if [[ "$current_fingerprint" == "$fingerprint" ]]; then
    status "Pinned replay image already up to date: ${image_name} (${container_platform})"
    exit 0
fi

status "Building pinned replay image ${image_name} for ${container_platform}"

build_cmd=(
    "$container_runtime" build
    --platform "$container_platform"
    --tag "$image_name"
    --label "${label_key}=${fingerprint}"
    --build-arg "BASE_IMAGE=${base_image}"
    --build-arg "SNAPSHOT_TIMESTAMP=${snapshot_timestamp}"
    --build-arg "REPLAY_HOST_ARCH=${replay_host_arch}"
    --file "$dockerfile_path"
    "${repo_root}/containers/replay-bookworm"
)

if [[ "$verbose" == "1" ]]; then
    "${build_cmd[@]}"
else
    "${build_cmd[@]}" >/dev/null
fi

status "Pinned replay image ready: ${image_name}"
