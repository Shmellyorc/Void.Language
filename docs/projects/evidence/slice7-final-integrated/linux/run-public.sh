#!/usr/bin/env bash
set -euo pipefail
profile="$1"
root="$PWD"
work="$root/build/slice7-final-integrated"
ev="$root/docs/projects/evidence/slice7-final-integrated"
build="$work/linux-$profile"
vendor="$root/build/slice7-raylib/upstream/raylib-6.0_linux_amd64"
sdk="$root/.toolchains/llvm-23.1.4/bin"
flags=()
case "$profile" in
    release) config=Release;;
    debug) config=Debug;;
    strict) config=RelWithDebInfo; flags+=(--strict);;
esac
for attempt in $(seq 1 900); do
    if [[ -x "$build/voidc" && -x "$build/voidls" && -s "$build/void-native-resources.txt" ]]; then
        break
    fi
    if grep -q '^ninja: build stopped' "$ev/linux/$profile-build.txt"; then
        exit 2
    fi
    sleep 3
done
[[ -x "$build/voidc" && -x "$build/voidls" ]]
export TMPDIR
TMPDIR=$(mktemp -d "/tmp/void-s7-public-$profile.XXXXXX")
export PYTHONDONTWRITEBYTECODE=1
printf '%s\n' "$TMPDIR" > "$ev/linux/$profile-public-temp.txt"
python3 "$ev/public-workflow.py" --cmake "$(command -v cmake)" --build "$build" --source "$root" \
    --clang "$sdk/clang" --llvm-readobj "$sdk/llvm-readobj" --work "$work/linux-public-$profile" \
    --header "$vendor/include/raylib.h" --library "$vendor/lib/libraylib.so.6.0.0" \
    --license "$vendor/LICENSE" --config "$config" "${flags[@]}" > "$ev/linux/$profile-public.txt" 2>&1
