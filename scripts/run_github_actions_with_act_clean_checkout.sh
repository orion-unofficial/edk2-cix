#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(dirname -- "$script_dir")"
keep_checkout="${EDK2_CIX_ACT_CLEAN_KEEP:-0}"
temp_root=""

status() {
    printf '[act-clean] %s\n' "$*"
}

should_keep_checkout() {
    case "${keep_checkout}" in
        1|true|TRUE|yes|YES)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

cleanup() {
    if [[ -z "${temp_root}" ]] || [[ ! -d "${temp_root}" ]]; then
        return
    fi

    if should_keep_checkout; then
        status "Preserving clean checkout at ${temp_root}"
        return
    fi

    rm -rf "${temp_root}"
}

trap 'status_code=$?; trap - EXIT; cleanup; exit "$status_code"' EXIT

temp_root="$(mktemp -d "${TMPDIR:-/tmp}/edk2-cix-act-clean.XXXXXX")"
clone_root="${temp_root}/repo"
patch_path="${temp_root}/working-tree.patch"
untracked_count=0

status "Creating clean checkout at ${clone_root}"
git clone --quiet "${repo_root}" "${clone_root}"

git -C "${repo_root}" diff --binary HEAD -- > "${patch_path}"
if [[ -s "${patch_path}" ]]; then
    status "Applying tracked working tree changes"
    git -C "${clone_root}" apply --allow-empty "${patch_path}"
fi

while IFS= read -r -d '' relative_path; do
    mkdir -p "${clone_root}/$(dirname -- "${relative_path}")"
    rm -rf "${clone_root}/${relative_path}"
    cp -pR "${repo_root}/${relative_path}" "${clone_root}/${relative_path}"
    untracked_count=$((untracked_count + 1))
done < <(git -C "${repo_root}" ls-files --others --exclude-standard -z)

if (( untracked_count > 0 )); then
    status "Copied ${untracked_count} untracked non-ignored path(s)"
fi

status "Running act from clean checkout"
cd "${clone_root}"
"${clone_root}/scripts/run_github_actions_with_act.sh" "$@"
