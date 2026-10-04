#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
tool_root="${EDK2_CIX_DOCS_TOOLS_DIR:-${repo_root}/.docs-tools}"
lockfile="${repo_root}/nix/mdbook-toc-0.15.3-mdbook-0.5.2.Cargo.lock"
stamp_file="${tool_root}/mdbook-toc.lock.cksum"
binary="${tool_root}/bin/mdbook-toc"
version="0.15.3"
temp_root="$("${script_dir}/resolve_temp_root.sh")"
read -r lockfile_cksum lockfile_size _ < <(cksum "$lockfile")
expected_stamp="${lockfile_cksum}:${lockfile_size}"

if [[ -x "$binary" ]] && [[ -f "$stamp_file" ]] && [[ "$(cat "$stamp_file")" == "$expected_stamp" ]]; then
    exit 0
fi

if ! command -v cargo >/dev/null 2>&1; then
    printf 'error: cargo is required to build mdbook-toc %s\n' "$version" >&2
    exit 1
fi

if ! command -v git >/dev/null 2>&1; then
    printf 'error: git is required to fetch mdbook-toc %s\n' "$version" >&2
    exit 1
fi

workdir="$(mktemp -d "${temp_root}/mdbook-toc.XXXXXX")"
trap 'rm -rf "$workdir"' EXIT

git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$version" https://github.com/badboy/mdbook-toc.git "$workdir/src"
cp "$lockfile" "$workdir/src/Cargo.lock"
mkdir -p "$tool_root"
cargo install --locked --path "$workdir/src" --root "$tool_root"
printf '%s\n' "$expected_stamp" > "$stamp_file"
