#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
docs_root="$(dirname -- "$script_dir")"
repo_root="$(dirname -- "$docs_root")"
tool_root="${EDK2_CIX_DOCS_TOOLS_DIR:-${repo_root}/.cache/edk2-cix/docs/tools}"
binary="${tool_root}/bin/mdbook-toc"

if [[ ! -x "$binary" ]]; then
    printf 'error: %s is missing; run %s first\n' "$binary" "${script_dir}/install_mdbook_toc.sh" >&2
    exit 1
fi

exec "$binary" "$@"
