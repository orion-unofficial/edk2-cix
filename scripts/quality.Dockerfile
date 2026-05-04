FROM debian:trixie-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        flake8 \
        git \
        jq \
        make \
        markdownlint \
        python3 \
        shellcheck \
        yamllint \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
ENTRYPOINT ["python3", "scripts/quality_checks.py"]
