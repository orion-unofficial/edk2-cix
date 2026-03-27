#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"

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

package_installed() {
    local package="$1"
    dpkg-query -W -f='${Status}\n' "$package" 2>/dev/null | grep -qx 'install ok installed'
}

required_packages=(
    crossbuild-essential-arm64
    binfmt-support
    qemu-user-static
    devscripts
    lintian
    dh-exec
    dos2unix
    pandoc
    shellcheck
    acpica-tools
    uuid-dev
    nasm
    bison
    flex
    curl
)

missing_packages=()
for package in "${required_packages[@]}"; do
    if ! package_installed "$package"; then
        missing_packages+=("$package")
    fi
done

need_arm64_arch=0
if ! dpkg --print-foreign-architectures | grep -qx 'arm64'; then
    need_arm64_arch=1
fi

if (( need_arm64_arch == 0 )) && (( ${#missing_packages[@]} == 0 )); then
    status "Build dependencies already installed."
    exit 0
fi

if (( need_arm64_arch != 0 )); then
    status "Adding foreign architecture: arm64"
    as_root dpkg --add-architecture arm64
fi

status "Refreshing apt metadata"
as_root apt-get update

bootstrap_packages=()
for package in crossbuild-essential-arm64 binfmt-support qemu-user-static; do
    if ! package_installed "$package"; then
        bootstrap_packages+=("$package")
    fi
done

if (( ${#bootstrap_packages[@]} > 0 )); then
    status "Installing bootstrap packages: ${bootstrap_packages[*]}"
    as_root apt-get install -y "${bootstrap_packages[@]}"
fi

status "Installing Debian build dependencies"
(
    cd "$repo_root"
    as_root apt-get build-dep . -y
)

status "Build dependencies ready."
