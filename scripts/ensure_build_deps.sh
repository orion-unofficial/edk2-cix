#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
dep_profile="${EDK2_CIX_DEP_PROFILE:-packaging}"
verbose="${EDK2_CIX_VERBOSE:-${V:-0}}"

status() {
    printf '[deps] %s\n' "$*"
}

as_root() {
    if [[ "${EUID}" -eq 0 ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

apt_env=(
    env
    DEBIAN_FRONTEND=noninteractive
    DEBCONF_NONINTERACTIVE_SEEN=true
    DEBCONF_NOWARNINGS=yes
    DEBIAN_PRIORITY=critical
    TERM=dumb
)

apt_get() {
    if [[ "$verbose" == "1" ]]; then
        as_root "${apt_env[@]}" apt-get \
            -o APT::Install-Recommends=false \
            -o APT::Install-Suggests=false \
            -o APT::Color=0 \
            -o Dpkg::Use-Pty=0 \
            "$@"
        return 0
    fi

    local apt_log
    apt_log="$(mktemp "${TMPDIR:-/tmp}/edk2-cix-apt.XXXXXX")"
    if ! as_root "${apt_env[@]}" apt-get \
        -o APT::Install-Recommends=false \
        -o APT::Install-Suggests=false \
        -o APT::Color=0 \
        -o Dpkg::Use-Pty=0 \
        "$@" >"$apt_log" 2>&1; then
        cat "$apt_log" >&2
        rm -f "$apt_log"
        return 1
    fi
    rm -f "$apt_log"
}

usage() {
    cat <<'EOF'
usage: scripts/ensure_build_deps.sh [--profile firmware|packaging]
EOF
}

describe_host() {
    if [[ -r /etc/os-release ]]; then
        local id="" id_like="" pretty_name="" name=""
        # shellcheck disable=SC1091
        . /etc/os-release
        id="${ID:-}"
        id_like="${ID_LIKE:-}"
        pretty_name="${PRETTY_NAME:-}"
        name="${NAME:-}"
        if [[ -n "$pretty_name" ]]; then
            if [[ -n "$id" || -n "$id_like" ]]; then
                printf '%s (ID=%s%s)\n' \
                    "$pretty_name" \
                    "${id:-unknown}" \
                    "${id_like:+, ID_LIKE=${id_like}}"
            else
                printf '%s\n' "$pretty_name"
            fi
            return 0
        fi
        if [[ -n "$name" ]]; then
            printf '%s\n' "$name"
            return 0
        fi
    fi
    printf '%s %s %s\n' "$(uname -s)" "$(uname -r)" "$(uname -m)"
}

is_debian_family_host() {
    if [[ ! -r /etc/os-release ]]; then
        return 1
    fi

    local id="" id_like=""
    # shellcheck disable=SC1091
    . /etc/os-release
    id="${ID:-}"
    id_like="${ID_LIKE:-}"
    case " ${id} ${id_like} " in
        *" debian "*|*" ubuntu "*)
            return 0
            ;;
    esac
    return 1
}

ensure_supported_host() {
    if is_debian_family_host; then
        return 0
    fi

    cat >&2 <<EOF
[deps] scripts/ensure_build_deps.sh bootstraps packages with apt/dpkg and only supports Debian/Ubuntu-family hosts.
[deps] Detected host: $(describe_host)
[deps] Use scripts/run_in_buildbox.sh, a devcontainer, or install the equivalent host tools manually on this platform.
EOF
    exit 1
}

while (( $# > 0 )); do
    case "$1" in
        --profile)
            shift
            if (( $# == 0 )); then
                usage >&2
                exit 2
            fi
            dep_profile="$1"
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

case "$dep_profile" in
    firmware|packaging)
        ;;
    *)
        printf '[deps] Unsupported dependency profile: %s\n' "$dep_profile" >&2
        exit 2
        ;;
esac

ensure_supported_host

bash "${script_dir}/require_host_tools.sh" \
    "Debian dependency bootstrap" \
    apt-get dpkg dpkg-query

if [[ "${EUID}" -ne 0 ]] && ! command -v sudo >/dev/null 2>&1; then
    cat >&2 <<'EOF'
[deps] This dependency bootstrap needs root privileges, but sudo is not available.
[deps] Re-run as root or install sudo first.
EOF
    exit 1
fi

if [[ "${EUID}" -ne 0 ]]; then
    sudo -v
fi

host_dpkg_arch="$(dpkg --print-architecture)"

package_installed() {
    local package="$1"
    dpkg-query -W -f='${Status}\n' "$package" 2>/dev/null | grep -qx 'install ok installed'
}

toolchain_packages=()
need_arm64_arch=0
case "$host_dpkg_arch" in
    arm64)
        toolchain_packages+=(
            build-essential
        )
        ;;
    *)
        toolchain_packages+=(
            build-essential
            crossbuild-essential-arm64
        )
        if ! dpkg --print-foreign-architectures | grep -qx 'arm64'; then
            need_arm64_arch=1
        fi
        ;;
esac

common_packages=(
    git
    pkg-config
    "${toolchain_packages[@]}"
    binfmt-support
    qemu-user-static
    dpkg-dev
    dos2unix
    acpica-tools
    uuid-dev
    nasm
    bison
    flex
    curl
    libssl-dev
    perl
    python3
)

packaging_packages=(
    devscripts
    lintian
    dh-exec
    pandoc
    shellcheck
)

required_packages=("${common_packages[@]}")
if [[ "$dep_profile" == "packaging" ]]; then
    required_packages+=("${packaging_packages[@]}")
fi

missing_packages=()
for package in "${required_packages[@]}"; do
    if ! package_installed "$package"; then
        missing_packages+=("$package")
    fi
done

if (( need_arm64_arch == 0 )) && (( ${#missing_packages[@]} == 0 )); then
    status "Build dependencies already installed for profile: ${dep_profile}."
    exit 0
fi

if (( need_arm64_arch != 0 )); then
    status "Adding foreign architecture: arm64"
    as_root dpkg --add-architecture arm64
fi

status "Refreshing apt metadata for profile: ${dep_profile}"
apt_get update

bootstrap_packages=()
for package in "${toolchain_packages[@]}" binfmt-support qemu-user-static; do
    if ! package_installed "$package"; then
        bootstrap_packages+=("$package")
    fi
done

if (( ${#bootstrap_packages[@]} > 0 )); then
    status "Installing bootstrap packages: ${bootstrap_packages[*]}"
    apt_get install -y --no-install-recommends "${bootstrap_packages[@]}"
fi

if [[ "$dep_profile" == "packaging" ]]; then
    status "Installing Debian packaging build dependencies"
    (
        cd "$repo_root"
        apt_get build-dep . -y --no-install-recommends
    )
else
    status "Skipping Debian packaging build-deps for firmware profile"
fi

remaining_packages=()
for package in "${required_packages[@]}"; do
    if ! package_installed "$package"; then
        remaining_packages+=("$package")
    fi
done

if (( ${#remaining_packages[@]} > 0 )); then
    status "Installing required host tools: ${remaining_packages[*]}"
    apt_get install -y --no-install-recommends "${remaining_packages[@]}"
fi

status "Build dependencies ready for profile: ${dep_profile}."
