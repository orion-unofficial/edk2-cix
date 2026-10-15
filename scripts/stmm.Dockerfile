# Build-only environment for scripts/build_stmm_qualification.py.
FROM debian:trixie-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu \
        python3 uuid-dev nasm acpica-tools \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
