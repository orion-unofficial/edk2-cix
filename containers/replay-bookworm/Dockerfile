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
    for archive in debian debian-security; do \
        mirrorlist="/etc/apt/snapshot-${archive}.mirrors"; \
        : > "${mirrorlist}"; \
        printf 'http://snapshot-cloudflare.debian.org/archive/%s/%s/\tpriority:1\n' "${archive}" "${SNAPSHOT_TIMESTAMP}" >> "${mirrorlist}"; \
        printf 'http://snapshot-fastly.debian.org/archive/%s/%s/\tpriority:2\n' "${archive}" "${SNAPSHOT_TIMESTAMP}" >> "${mirrorlist}"; \
        printf 'http://snapshot.debian.org/archive/%s/%s/\tpriority:3\n' "${archive}" "${SNAPSHOT_TIMESTAMP}" >> "${mirrorlist}"; \
    done; \
    printf 'deb [check-valid-until=no] mirror+file:/etc/apt/snapshot-debian.mirrors bookworm main\n' > /etc/apt/sources.list; \
    printf 'deb [check-valid-until=no] mirror+file:/etc/apt/snapshot-debian.mirrors bookworm-updates main\n' >> /etc/apt/sources.list; \
    printf 'deb [check-valid-until=no] mirror+file:/etc/apt/snapshot-debian-security.mirrors bookworm-security main\n' >> /etc/apt/sources.list; \
    printf 'Acquire::Check-Valid-Until "false";\n' > /etc/apt/apt.conf.d/99snapshot; \
    printf 'Acquire::Retries "1";\nAcquire::Retries::Delay "false";\nAcquire::http::Timeout "30";\nAcquire::https::Timeout "30";\n' > /etc/apt/apt.conf.d/80replay-network; \
    apt-get -o APT::Update::Error-Mode=any update; \
    grep -Ev '^\s*(#|$)' "${manifest}" | xargs -r apt-get install -y --no-install-recommends; \
    rm -rf /var/lib/apt/lists/* /tmp/replay-packages
