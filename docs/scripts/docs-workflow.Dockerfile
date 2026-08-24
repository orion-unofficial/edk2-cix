FROM nixos/nix:2.35.2

RUN nix --extra-experimental-features 'nix-command flakes' \
    profile install --impure --accept-flake-config \
    nixpkgs#gnumake \
    nixpkgs#devenv \
    nixpkgs#cargo \
    nixpkgs#rustc \
    nixpkgs#gcc

RUN mkdir -p /bin && ln -sf "$(command -v bash)" /bin/bash

ENV PATH=/nix/var/nix/profiles/default/bin:/root/.nix-profile/bin:$PATH

WORKDIR /work
