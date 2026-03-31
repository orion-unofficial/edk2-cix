#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
act_version="${EDK2_CIX_ACT_VERSION:-0.2.86}"
tool_root="${EDK2_CIX_ACT_TOOL_ROOT:-${repo_root}/.buildbox/tools/act}"
download_root="${tool_root}/downloads"
temp_root="$("${script_dir}/resolve_temp_root.sh")"

status() {
    printf '[act-bootstrap] %s\n' "$*" >&2
}

fail() {
    printf '[act-bootstrap] %s\n' "$*" >&2
    exit 1
}

normalize_os() {
    case "$(uname -s)" in
        Darwin)
            printf 'Darwin\n'
            ;;
        Linux)
            printf 'Linux\n'
            ;;
        *)
            fail "Unsupported host OS: $(uname -s)"
            ;;
    esac
}

normalize_arch() {
    case "$(uname -m)" in
        arm64|aarch64)
            printf 'arm64\n'
            ;;
        x86_64|amd64)
            printf 'x86_64\n'
            ;;
        *)
            fail "Unsupported host architecture: $(uname -m)"
            ;;
    esac
}

sha256_file() {
    local path="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$path" | awk '{print $1}'
        return 0
    fi
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$path" | awk '{print $1}'
        return 0
    fi
    fail "Need sha256sum or shasum to verify downloaded act archives."
}

download() {
    local url="$1"
    local output="$2"
    curl --fail --location --silent --show-error "$url" --output "$output"
}

host_os="$(normalize_os)"
host_arch="$(normalize_arch)"
asset_name="act_${host_os}_${host_arch}.tar.gz"
release_tag="v${act_version}"
install_dir="${tool_root}/${release_tag}/${host_os}_${host_arch}"
act_bin="${install_dir}/act"
checksums_url="https://github.com/nektos/act/releases/download/${release_tag}/checksums.txt"
asset_url="https://github.com/nektos/act/releases/download/${release_tag}/${asset_name}"

if [[ -x "$act_bin" ]]; then
    printf '%s\n' "$act_bin"
    exit 0
fi

mkdir -p "$download_root" "$tool_root"

tmpdir="$(mktemp -d "${temp_root}/edk2-cix-act.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT

checksums_path="${tmpdir}/checksums.txt"
archive_path="${tmpdir}/${asset_name}"
extract_dir="${tmpdir}/extract"

status "Downloading checksums for act ${release_tag}"
download "$checksums_url" "$checksums_path"
expected_sha256="$(awk -v asset="$asset_name" '$2 == asset { print $1 }' "$checksums_path")"
if [[ -z "$expected_sha256" ]]; then
    fail "Could not find ${asset_name} in ${checksums_url}"
fi

status "Downloading ${asset_name}"
download "$asset_url" "$archive_path"
actual_sha256="$(sha256_file "$archive_path")"
if [[ "$actual_sha256" != "$expected_sha256" ]]; then
    fail "Checksum mismatch for ${asset_name}: expected ${expected_sha256}, got ${actual_sha256}"
fi

mkdir -p "$extract_dir"
tar -xzf "$archive_path" -C "$extract_dir"
if [[ ! -x "${extract_dir}/act" ]]; then
    fail "Downloaded archive did not contain an executable act binary."
fi

mkdir -p "$(dirname "$install_dir")"
rm -rf "$install_dir"
mv "$extract_dir" "$install_dir"

status "Installed act ${release_tag} at ${act_bin}"
printf '%s\n' "$act_bin"
