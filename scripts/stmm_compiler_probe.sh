#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
test "$(uname -m)" = x86_64
test "$(dpkg --print-architecture)" = amd64
test "$(cc -dumpmachine)" = x86_64-linux-gnu
test "$(aarch64-linux-gnu-gcc -dumpmachine)" = aarch64-linux-gnu

probe_dir=$(mktemp -d)
trap 'rm -rf "$probe_dir"' EXIT
printf 'int main(void) { return 0; }\n' > "$probe_dir/probe.c"
cc -o "$probe_dir/host" "$probe_dir/probe.c"
"$probe_dir/host"
aarch64-linux-gnu-gcc -c -o "$probe_dir/target.o" "$probe_dir/probe.c"
readelf -h "$probe_dir/host" | grep -E 'Machine:.*X86-64$'
aarch64-linux-gnu-readelf -h "$probe_dir/target.o" | grep -E 'Machine:.*AArch64$'

evidence=/usr/share/stmm-compiler
sha256sum "$evidence/InRelease" "$evidence/packages.tsv" "$evidence/debs.sha256"
dpkg-query -W -f='${binary:Package}\t${Version}\t${Architecture}\n' > "$probe_dir/packages.tsv"
cmp "$probe_dir/packages.tsv" "$evidence/packages.tsv"
cc --version
aarch64-linux-gnu-gcc --version
aarch64-linux-gnu-ld --version
python3 --version
make --version
git --version
nasm -v
iasl -v
