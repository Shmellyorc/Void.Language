#!/usr/bin/env bash
set -euo pipefail
profile=$1
root=$PWD
work="$root/build/slice7-final-link-repair"
build="$root/build/slice7-final-integrated/linux-$profile"
copy="$work/linux-$profile"
old_evidence="$root/docs/projects/evidence/slice7-final-integrated"
evidence="$root/docs/projects/evidence/slice7-final-link-repair/linux"
vendor="$root/build/slice7-raylib/upstream/raylib-6.0_linux_amd64"
sdk="$root/.toolchains/llvm-23.1.4/bin"
flags=()
case "$profile" in
    release) config=Release;;
    debug) config=Debug;;
    strict) config=RelWithDebInfo; flags+=(--strict);;
    *) exit 2;;
esac
[[ -f "$evidence/$profile-plan.json" ]]
export TMPDIR PYTHONDONTWRITEBYTECODE=1
TMPDIR=$(mktemp -d "/tmp/void-s7-link-$profile-public.XXXXXX")
printf '%s\n' "$TMPDIR" > "$evidence/$profile-public-temp.txt"
bwrap --bind / / --dev-bind /dev /dev --proc /proc --bind "$copy" "$build" --die-with-parent --chdir "$root" -- \
    python3 "$old_evidence/public-workflow.py" --cmake "$(command -v cmake)" --build "$build" --source "$root" \
        --clang "$sdk/clang" --llvm-readobj "$sdk/llvm-readobj" --work "$work/linux-public-$profile" \
        --header "$vendor/include/raylib.h" --library "$vendor/lib/libraylib.so.6.0.0" \
        --license "$vendor/LICENSE" --config "$config" "${flags[@]}" > "$evidence/$profile-public.txt" 2>&1
bash "$evidence/isolated-package.sh" "$profile" > "$evidence/$profile-isolated-package.txt" 2>&1
printf '0\n' > "$evidence/$profile-public-exit.txt"
