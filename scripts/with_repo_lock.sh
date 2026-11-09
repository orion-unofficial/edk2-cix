#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "$0")" && pwd -P)"
exec python3 "$script_dir/repo_lock.py" "$@"
