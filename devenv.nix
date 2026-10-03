{ pkgs, lib, ... }:

{
  imports =
    lib.optional (builtins.pathExists ./devenv.local.nix) ./devenv.local.nix
    ++ lib.optional (builtins.pathExists ./.github/local/devenv.nix) ./.github/local/devenv.nix;

  # https://devenv.sh/packages/
  packages = with pkgs; [
    bash-completion
    commitizen
    mdbook
    mdbook-cmdrun
    mdbook-linkcheck2
    ncurses
    python3
    shellcheck
    shfmt
    statix
    typos
  ];

  starship.enable = true;
}
