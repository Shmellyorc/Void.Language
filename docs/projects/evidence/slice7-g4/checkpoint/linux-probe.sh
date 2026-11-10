#!/usr/bin/env bash
set -euo pipefail
evidence=$(realpath "$(dirname "$0")")
work=$1
cc=$2
mkdir "$work"
work=$(realpath "$work")
"$cc" --version
uname -m
ldd --version
"$cc" -fuse-ld=lld -nostdlib -fPIC -shared "$evidence/dependency.c" -Wl,-soname,libvoid_g4_probe_dep.so -o "$work/libvoid_g4_probe_dep.so"
"$cc" -fuse-ld=lld -nostdlib -fPIC -shared "$evidence/dependency.c" -DFIXTURE_VALUE=99 -Wl,-soname,libvoid_g4_probe_dep.so -o "$work/competitor.so"
"$cc" -fuse-ld=lld -nostdlib -fPIC -shared "$evidence/provider.c" "$work/libvoid_g4_probe_dep.so" -Wl,-soname,libvoid_g4_probe_main.so -o "$work/provider.so"
"$cc" -Wall -Wextra -Werror "$evidence/linux-mapping.c" -ldl -o "$work/probe"
readelf -d "$work/provider.so" "$work/libvoid_g4_probe_dep.so"
sha256sum "$work/"{provider.so,libvoid_g4_probe_dep.so,competitor.so,probe}
"$work/probe" "$work/provider.so" "$work/libvoid_g4_probe_dep.so" "$work/competitor.so"
sha256sum "$work/"{provider.so,libvoid_g4_probe_dep.so,probe}
