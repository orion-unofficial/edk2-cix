FROM docker.io/library/debian@sha256:7378260c0a098d27ac206225db5e7d92b8c7c84ea7afccc86eeb513756bbd392

ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8 LC_ALL=C.UTF-8 TZ=UTC

# The snapshot is immutable. APT still verifies Debian's signature and package hashes.
RUN set -eu; \
    test "$(dpkg --print-architecture)" = amd64; \
    rm -f /etc/apt/sources.list /etc/apt/sources.list.d/debian.sources /etc/apt/apt.conf.d/docker-clean; \
    printf '%s\n' 'deb [check-valid-until=no signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] http://snapshot.debian.org/archive/debian/20261105T082236Z/ trixie main' > /etc/apt/sources.list; \
    apt-get -o APT::Update::Error-Mode=any -o Acquire::Retries=1 -o Acquire::http::Timeout=30 update; \
    printf '%s  %s\n' 0584fba32e13e0ab8285fb16c27adea1ec03a73669c18702821094fd6ca86675 /var/lib/apt/lists/snapshot.debian.org_archive_debian_20261105T082236Z_dists_trixie_InRelease | sha256sum --check -; \
    apt-get -o Acquire::Retries=1 -o Acquire::http::Timeout=30 install -y --no-install-recommends \
        bash build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
        python3 uuid-dev nasm acpica-tools git ca-certificates gzip tar xz-utils; \
    evidence=/usr/share/stmm-compiler; \
    mkdir -p "$evidence"; \
    dpkg-query -W -f='${binary:Package}\t${Version}\t${Architecture}\n' > "$evidence/packages.tsv"; \
    sha256sum /var/cache/apt/archives/*.deb > "$evidence/debs.sha256"; \
    cp /var/lib/apt/lists/*_InRelease "$evidence/InRelease"; \
    apt-get clean; \
    rm -rf /var/lib/apt/lists/*
