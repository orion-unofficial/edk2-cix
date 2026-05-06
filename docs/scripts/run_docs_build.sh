#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
docs_root="$(dirname -- "$script_dir")"
repo_root="$(dirname -- "$docs_root")"
mode="${DOCS_BUILD_MODE:-${EDK2_CIX_DOCS_BUILD_MODE:-auto}}"
in_container="${EDK2_CIX_DOCS_IN_CONTAINER:-0}"
docs_cache_root="${EDK2_CIX_DOCS_CACHE_ROOT:-${repo_root}/.cache/edk2-cix/docs}"
export EDK2_CIX_DOCS_TOOLS_DIR="${EDK2_CIX_DOCS_TOOLS_DIR:-${docs_cache_root}/tools}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-${docs_cache_root}/xdg-cache}"
export XDG_STATE_HOME="${XDG_STATE_HOME:-${docs_cache_root}/xdg-state}"
export CARGO_HOME="${CARGO_HOME:-${docs_cache_root}/cargo-home}"
export TMPDIR="${TMPDIR:-${docs_cache_root}/tmp}"
export TMP="${TMP:-${TMPDIR}}"
export TEMP="${TEMP:-${TMPDIR}}"
mkdir -p "$EDK2_CIX_DOCS_TOOLS_DIR" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" "$CARGO_HOME" "$TMPDIR"

join_by_comma() {
    local item output=""

    for item in "$@"; do
        if [[ -n "$output" ]]; then
            output+=", "
        fi
        output+="$item"
    done
    printf '%s' "$output"
}

mdbook_toc_needs_build() {
    local tool_root lockfile stamp_file binary lockfile_cksum lockfile_size
    local expected_stamp

    tool_root="$EDK2_CIX_DOCS_TOOLS_DIR"
    lockfile="${docs_root}/nix/mdbook-toc-0.15.3-mdbook-0.5.2.Cargo.lock"
    stamp_file="${tool_root}/mdbook-toc.lock.cksum"
    binary="${tool_root}/bin/mdbook-toc"

    if [[ ! -x "$binary" || ! -f "$stamp_file" ]]; then
        return 0
    fi

    read -r lockfile_cksum lockfile_size _ < <(cksum "$lockfile")
    expected_stamp="${lockfile_cksum}:${lockfile_size}"
    [[ "$(cat "$stamp_file")" != "$expected_stamp" ]]
}

missing_host_tools() {
    local missing=()

    if ! command -v devenv >/dev/null 2>&1; then
        missing+=(devenv)
    fi

    if ! command -v git >/dev/null 2>&1; then
        missing+=(git)
    fi

    if mdbook_toc_needs_build && ! command -v cargo >/dev/null 2>&1; then
        missing+=(cargo)
    fi

    if ((${#missing[@]} > 0)); then
        join_by_comma "${missing[@]}"
    fi
}

run_container_build() {
    if ! command -v docker >/dev/null 2>&1; then
        printf 'error: docs build requires Docker when host dependencies are missing\n' >&2
        printf 'missing host dependencies: %s\n' "$1" >&2
        exit 1
    fi

    if [[ -n "$1" ]]; then
        printf '[docs] Host docs build dependencies missing: %s\n' "$1"
    else
        printf '[docs] DOCS_BUILD_MODE=container selected\n'
    fi
    printf '[docs] Falling back to the documentation container\n'
    exec "${script_dir}/run_docs_workflow_local.sh"
}

case "$mode" in
    auto)
        missing_tools="$(missing_host_tools)"
        if [[ -n "$missing_tools" && "$in_container" != 1 ]]; then
            run_container_build "$missing_tools"
        elif [[ -n "$missing_tools" ]]; then
            printf 'error: docs container is missing required tools: %s\n' "$missing_tools" >&2
            exit 1
        fi
        ;;
    host)
        missing_tools="$(missing_host_tools)"
        if [[ -n "$missing_tools" ]]; then
            printf 'error: host docs build dependencies missing: %s\n' "$missing_tools" >&2
            printf 'hint: unset DOCS_BUILD_MODE or use DOCS_BUILD_MODE=container\n' >&2
            exit 1
        fi
        ;;
    container)
        if [[ "$in_container" == 1 ]]; then
            printf 'error: DOCS_BUILD_MODE=container was requested inside the docs container\n' >&2
            printf 'hint: run the container payload with DOCS_BUILD_MODE=host\n' >&2
            exit 1
        fi
        run_container_build ""
        ;;
    *)
        printf 'error: invalid DOCS_BUILD_MODE=%s; expected auto, host, or container\n' "$mode" >&2
        exit 1
        ;;
esac

"${script_dir}/install_mdbook_toc.sh"
cd "$docs_root"
devenv shell --option starship.enable:bool false --option devenv.latestVersion:string 2.0.7 make docs-build
