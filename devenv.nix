{ pkgs, lib, config, ... }:

{
  imports = lib.optional (builtins.pathExists ./.github/local/devenv.nix) ./.github/local/devenv.nix;

  # https://devenv.sh/packages/
  # Keep the docs toolchain on the mdBook 0.4-compatible plugin set pinned in
  # devenv.lock until the preprocessors catch up with mdBook 0.5.
  packages = with pkgs; [
    bash-completion
    mdbook
    mdbook-admonish
    mdbook-cmdrun
    mdbook-linkcheck
    mdbook-toc
    ncurses
  ];

  git-hooks = {
    hooks = {
      commitizen.enable = true;
      shellcheck = {
        enable = true;
        entry = lib.mkForce "${pkgs.shellcheck}/bin/shellcheck -x";
      };
      shfmt.enable = true;
      statix.enable = true;
      typos = {
        enable = true;
        excludes = [
          "theme/highlight.js"
        ];
        settings.ignored-words = [
          "Synopsys"
          "HSI"
        ];
      };
    };
  };

  starship.enable = true;
}
