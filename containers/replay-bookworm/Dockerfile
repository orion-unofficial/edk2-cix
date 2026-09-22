ARG BASE_IMAGE=debian:bookworm@sha256:1d6cd964917a13b547d1ea392dff9a000c3f36070686ebc5c8755d53fb374435
FROM ${BASE_IMAGE}

ARG SNAPSHOT_TIMESTAMP=20260406T235959Z
ARG REPLAY_HOST_ARCH=amd64

COPY packages.bookworm-amd64.txt /tmp/replay-packages/packages.bookworm-amd64.txt
COPY packages.bookworm-arm64.txt /tmp/replay-packages/packages.bookworm-arm64.txt

RUN set -eux; \
    case "${REPLAY_HOST_ARCH}" in \
        amd64) \
            manifest="/tmp/replay-packages/packages.bookworm-amd64.txt"; \
            dpkg --add-architecture arm64; \
            ;; \
        arm64) \
            manifest="/tmp/replay-packages/packages.bookworm-arm64.txt"; \
            ;; \
        *) \
            echo "Unsupported REPLAY_HOST_ARCH: ${REPLAY_HOST_ARCH}" >&2; \
            exit 2; \
            ;; \
    esac; \
    rm -f /etc/apt/sources.list; \
    find /etc/apt/sources.list.d -mindepth 1 -maxdepth 1 -type f -delete; \
    printf 'deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/%s/ bookworm main\n' "${SNAPSHOT_TIMESTAMP}" > /etc/apt/sources.list; \
    printf 'deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/%s/ bookworm-updates main\n' "${SNAPSHOT_TIMESTAMP}" >> /etc/apt/sources.list; \
    printf 'deb [check-valid-until=no] http://snapshot.debian.org/archive/debian-security/%s/ bookworm-security main\n' "${SNAPSHOT_TIMESTAMP}" >> /etc/apt/sources.list; \
    printf 'Acquire::Check-Valid-Until "false";\n' > /etc/apt/apt.conf.d/99snapshot; \
    apt-get update; \
    grep -Ev '^\s*(#|$)' "${manifest}" | xargs -r apt-get install -y --no-install-recommends; \
    rm -rf /var/lib/apt/lists/* /tmp/replay-packages
