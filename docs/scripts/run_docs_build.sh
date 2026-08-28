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
export PYTHONPYCACHEPREFIX="${PYTHONPYCACHEPREFIX:-${docs_cache_root}/pycache}"
export TMPDIR="${TMPDIR:-${docs_cache_root}/tmp}"
export TMP="${TMP:-${TMPDIR}}"
export TEMP="${TEMP:-${TMPDIR}}"
mkdir -p "$EDK2_CIX_DOCS_TOOLS_DIR" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" "$CARGO_HOME" "$PYTHONPYCACHEPREFIX" "$TMPDIR"

docs_devenv_dir="${docs_root}/.devenv"
devenv_dotfile_cache="${EDK2_CIX_DOCS_DEVENV_CACHE_DIR:-${docs_cache_root}/devenv-dotfile}"
devenv_dotfile_linked=0

keep_devenv_dotfile() {
    case "${EDK2_CIX_DOCS_KEEP_DEVENV_DOTFILE:-0}" in
        1 | true | TRUE | yes | YES)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

prepare_devenv_dotfile() {
    if keep_devenv_dotfile; then
        return 0
    fi

    # devenv writes .devenv under the project root. Point it at the docs
    # cache during the build so normal exits leave the source tree clean.
    mkdir -p "$devenv_dotfile_cache"
    if [[ -L "$docs_devenv_dir" ]]; then
        rm -f -- "$docs_devenv_dir"
    elif [[ -e "$docs_devenv_dir" ]]; then
        rm -rf -- "$docs_devenv_dir"
    fi
    ln -s "$devenv_dotfile_cache" "$docs_devenv_dir"
    devenv_dotfile_linked=1
    trap cleanup_devenv_dotfile EXIT
}

cleanup_devenv_dotfile() {
    local status=$?

    trap - EXIT
    if keep_devenv_dotfile; then
        return "$status"
    fi

    if [[ "$devenv_dotfile_linked" == 1 && -L "$docs_devenv_dir" ]]; then
        rm -f -- "$docs_devenv_dir"
    elif [[ -e "$docs_devenv_dir" || -L "$docs_devenv_dir" ]]; then
        rm -rf -- "$docs_devenv_dir"
    fi
    return "$status"
}

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
    [[ "$(cat "$stamp_file")" != "$expected_stamp" ]] \
        || ! "$binary" --version >/dev/null 2>&1
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
prepare_devenv_dotfile
cd "$docs_root"
devenv_cache_options=()
if [[ "$in_container" == 1 ]]; then
    devenv_cache_options=(--option cachix.enable:bool false)
fi
devenv shell "${devenv_cache_options[@]}" \
    --option starship.enable:bool false \
    --option devenv.latestVersion:string 2.2.2 \
    make docs-build
