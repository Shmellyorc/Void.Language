#!/usr/bin/env bash
set -euo pipefail
mode=$1
root=$PWD
work="$root/build/slice7-final-link-repair/linux-public-$mode"
opts=(--unshare-all --die-with-parent --new-session
      --ro-bind /usr /usr --ro-bind /etc /etc --dev /dev --proc /proc --tmpfs /tmp
      --symlink usr/lib /lib --symlink usr/lib64 /lib64 --symlink usr/bin /bin
      --clearenv --setenv PATH /usr/bin:/bin --setenv VOID_STDLIB_ROOT /absent-stdlib
      --chdir /tmp)
if [[ "$mode" == strict ]]; then
    opts+=(--setenv ASAN_OPTIONS halt_on_error=1:abort_on_error=1:detect_leaks=1
           --setenv UBSAN_OPTIONS halt_on_error=1:print_stacktrace=1)
fi
invoke() {
    package=$1
    shift
    printf 'Isolated package %s\n' "$package"
    printf '%q ' bwrap "${opts[@]}" --ro-bind "$package" /app -- /usr/bin/sh -ec \
        'test ! -e "$1"; shift; exec "$@"' sh "$root" "$@"
    printf '\n'
    bwrap "${opts[@]}" --ro-bind "$package" /app -- /usr/bin/sh -ec \
        'test ! -e "$1"; shift; exec "$@"' sh "$root" "$@"
}
invoke "$work/relocated ordinary 雪" /app/ordinary
for layout in normal optimized; do
    invoke "$work/relocated gallery $layout 雪" /app/gallery --trace-only
done
