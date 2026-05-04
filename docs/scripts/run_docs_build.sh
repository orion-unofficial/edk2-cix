#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
docs_root="$(dirname -- "$script_dir")"

"${script_dir}/install_mdbook_toc.sh"
cd "$docs_root"
devenv shell --option starship.enable:bool false --option devenv.latestVersion:string 2.0.7 make docs-build
