#!/usr/bin/env bash

set -euo pipefail

host_os="${EDK2_CIX_HOST_OS:-$(uname -s 2>/dev/null || printf 'unknown\n')}"

docker_reports_real_engine() {
    local docker_info docker_version

    command -v docker >/dev/null 2>&1 || return 1

    docker_info="$(docker info --format '{{json .}}' 2>/dev/null || true)"
    if [[ -z "${docker_info//[$'\t\r\n ']}" ]]; then
        return 1
    fi

    docker_version="$(docker version --format '{{json .}}' 2>/dev/null || true)"

    if grep -qi 'podman' <<<"$docker_info"; then
        return 1
    fi

    if [[ -n "${docker_version//[$'\t\r\n ']}" ]] && grep -qi 'podman' <<<"$docker_version"; then
        return 1
    fi

    return 0
}

if docker_reports_real_engine; then
    printf 'docker\npodman\n'
elif [[ "$host_os" == "Darwin" ]]; then
    printf 'docker\npodman\n'
else
    printf 'podman\ndocker\n'
fi
