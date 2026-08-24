#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"

acpica_release="${EDK2_CIX_IASL_RELEASE:-20260408}"
case "$acpica_release" in
    20200925)
        source_name=""
        source_url=""
        source_sha256=""
        ;;
    20260408)
        source_name="acpica-unix-${acpica_release}.tar.gz"
        source_url="https://github.com/acpica/acpica/releases/download/${acpica_release}/${source_name}"
        source_sha256="e66ceb26d6d514ce164fe22f5a4f7ca165cc38349d7a97f41a21f19b364647a2"
        ;;
    *)
        printf '[iasl] Unsupported ACPICA release: %s\n' "$acpica_release" >&2
        exit 2
        ;;
esac
cache_root="${EDK2_CIX_ACPICA_CACHE_ROOT:-${repo_root}/build-cache/acpica}"
platform_key="$(uname -s)-$(uname -m)"
install_root="${cache_root}/${acpica_release}/${platform_key}"
cached_iasl="${install_root}/bin/iasl"
provision_build_root=""
provision_lock_dir=""

usage() {
    cat <<'EOF'
usage: scripts/ensure_iasl.sh [--print-path] [--verify <iasl-path>]

Resolve or provision the repository-pinned ACPICA iasl compiler.
Set EDK2_CIX_IASL_RELEASE=20200925 for historical vendor replay.
EOF
}

iasl_version() {
    "$1" -v 2>&1 | sed -n 's/.*version[[:space:]]\+\([0-9][0-9]*\).*/\1/p' | head -n 1
}

verify_iasl() {
    local candidate="$1"
    local version

    if [[ ! -x "$candidate" ]]; then
        printf '[iasl] Compiler is not executable: %s\n' "$candidate" >&2
        return 1
    fi
    version="$(iasl_version "$candidate")"
    if [[ "$version" != "$acpica_release" ]]; then
        printf '[iasl] Compiler %s is ACPICA %s; this source requires %s.\n' \
            "$candidate" "${version:-unknown}" "$acpica_release" >&2
        return 1
    fi
}

print_verified_path() {
    verify_iasl "$1"
    (
        cd -- "$(dirname -- "$1")"
        printf '%s/%s\n' "$PWD" "$(basename -- "$1")"
    )
}

resolve_path_candidate() {
    local candidate="$1"

    if [[ "$candidate" == */* ]]; then
        printf '%s\n' "$candidate"
    else
        command -v "$candidate" 2>/dev/null
    fi
}

cleanup_provisioning() {
    if [[ -n "$provision_build_root" ]]; then
        rm -rf -- "$provision_build_root"
    fi
    if [[ -n "$provision_lock_dir" ]]; then
        rmdir -- "$provision_lock_dir" 2>/dev/null || true
    fi
}

download_source() {
    local url="$1"
    local destination="$2"

    if command -v curl >/dev/null 2>&1; then
        curl --fail --location --silent --show-error "$url" --output "$destination"
        return 0
    fi
    python3 -c \
        'import pathlib, sys, urllib.request; pathlib.Path(sys.argv[2]).write_bytes(urllib.request.urlopen(sys.argv[1]).read())' \
        "$url" "$destination"
}

provision_iasl() {
    local lock_dir="${cache_root}/${acpica_release}/.provision-lock-${platform_key}"
    local acquired_lock=0
    local attempt
    local build_root
    local archive
    local source_root

    if [[ "$(uname -s)" != "Linux" ]]; then
        if path_iasl="$(command -v iasl 2>/dev/null)"; then
            printf '[iasl] Detected %s as ACPICA %s, but this source requires %s.\n' \
                "$path_iasl" "$(iasl_version "$path_iasl")" "$acpica_release" >&2
        fi
        cat >&2 <<EOF
[iasl] ACPICA ${acpica_release} is not installed for ${platform_key}.
[iasl] Automatic source provisioning is supported in the Linux buildbox.
[iasl] Use scripts/run_in_buildbox.sh, or set IASL to an executable ACPICA ${acpica_release} compiler.
EOF
        return 1
    fi

    if [[ "$acpica_release" == "20200925" ]]; then
        cat >&2 <<EOF
[iasl] Exact vendor replay requires Debian Bookworm's ACPICA 20200925 package.
[iasl] Run inside the recorded Bookworm replay buildbox, where acpica-tools supplies it.
EOF
        return 1
    fi

    mkdir -p "${cache_root}/${acpica_release}"
    for (( attempt = 0; attempt < 120; attempt++ )); do
        if mkdir "$lock_dir" 2>/dev/null; then
            acquired_lock=1
            break
        fi
        if [[ -x "$cached_iasl" ]] && verify_iasl "$cached_iasl" 2>/dev/null; then
            return 0
        fi
        sleep 1
    done
    if (( acquired_lock == 0 )); then
        printf '[iasl] Timed out waiting for ACPICA provisioning lock: %s\n' "$lock_dir" >&2
        return 1
    fi

    provision_lock_dir="$lock_dir"
    trap cleanup_provisioning EXIT INT TERM
    if [[ -x "$cached_iasl" ]] && verify_iasl "$cached_iasl" 2>/dev/null; then
        return 0
    fi

    for required_cmd in make cc bison flex tar sha256sum python3; do
        if ! command -v "$required_cmd" >/dev/null 2>&1; then
            printf '[iasl] Missing tool required to build ACPICA: %s\n' "$required_cmd" >&2
            return 1
        fi
    done

    build_root="$(mktemp -d "${cache_root}/${acpica_release}/.build-${platform_key}.XXXXXX")"
    provision_build_root="$build_root"
    archive="${build_root}/${source_name}"
    printf '[iasl] Provisioning ACPICA %s for %s\n' "$acpica_release" "$platform_key" >&2
    download_source "$source_url" "$archive"
    printf '%s  %s\n' "$source_sha256" "$archive" | sha256sum --check --status
    tar -xzf "$archive" -C "$build_root"
    source_root="${build_root}/acpica-unix-${acpica_release}"
    make -C "${source_root}/generate/unix" iasl >&2
    verify_iasl "${source_root}/generate/unix/bin/iasl"

    mkdir -p "${install_root}/bin"
    install -m 0755 "${source_root}/generate/unix/bin/iasl" "${cached_iasl}.new"
    mv -f "${cached_iasl}.new" "$cached_iasl"
}

mode="print"
verify_path=""
while (( $# > 0 )); do
    case "$1" in
        --print-path)
            mode="print"
            ;;
        --verify)
            shift
            if (( $# == 0 )); then
                usage >&2
                exit 2
            fi
            mode="verify"
            verify_path="$1"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if [[ "$mode" == "verify" ]]; then
    print_verified_path "$(resolve_path_candidate "$verify_path")"
    exit 0
fi

if [[ -n "${IASL:-}" ]]; then
    print_verified_path "$(resolve_path_candidate "$IASL")"
    exit 0
fi
if [[ -x "$cached_iasl" ]] && verify_iasl "$cached_iasl" 2>/dev/null; then
    print_verified_path "$cached_iasl"
    exit 0
fi
if path_iasl="$(command -v iasl 2>/dev/null)" && verify_iasl "$path_iasl" 2>/dev/null; then
    print_verified_path "$path_iasl"
    exit 0
fi

provision_iasl
print_verified_path "$cached_iasl"
