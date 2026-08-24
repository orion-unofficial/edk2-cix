#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
docs_root="$(dirname -- "$script_dir")"
repo_root="$(dirname -- "$docs_root")"
dockerfile="${EDK2_CIX_DOCS_WORKFLOW_DOCKERFILE:-${script_dir}/docs-workflow.Dockerfile}"
repo_key="$(printf '%s' "$repo_root" | cksum | awk '{print $1}')"
image="${EDK2_CIX_DOCS_WORKFLOW_IMAGE:-edk2-cix-docs-workflow:20260330-${repo_key}}"
container_name="${EDK2_CIX_DOCS_WORKFLOW_CONTAINER_NAME:-edk2-cix-docs-workflow-${repo_key}-$$}"
platform="${EDK2_CIX_DOCS_WORKFLOW_PLATFORM:-}"
rebuild=0

cmd=(
    ./docs/scripts/run_docs_build.sh
)

if [[ "${1:-}" == "--rebuild" ]]; then
    rebuild=1
    shift
fi

if (( $# > 0 )); then
    cmd=("$@")
fi

build_args=()
run_args=()
if [[ -n "$platform" ]]; then
    build_args+=(--platform "$platform")
    run_args+=(--platform "$platform")
fi

if (( rebuild )) || ! docker image inspect "$image" >/dev/null 2>&1; then
    printf '[docs-repro] Building image %s from %s\n' "$image" "$dockerfile"
    docker build --progress plain "${build_args[@]}" -t "$image" -f "$dockerfile" "$repo_root"
fi

printf '[docs-repro] Running in %s:' "$image"
printf ' %q' "${cmd[@]}"
printf '\n'
printf '[docs-repro] Container logs: docker logs -f %s\n' "$container_name"

docker run --rm \
    --name "$container_name" \
    "${run_args[@]}" \
    -e DOCS_BUILD_MODE=host \
    -e EDK2_CIX_DOCS_BUILD_MODE=host \
    -e EDK2_CIX_DOCS_IN_CONTAINER=1 \
    -v "${repo_root}:/work" \
    -w /work \
    "$image" \
    bash -lc "$(printf '%q ' "${cmd[@]}")"
