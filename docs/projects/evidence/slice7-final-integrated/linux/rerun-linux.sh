#!/usr/bin/env bash
set -euo pipefail
profile="$1"
root="$PWD"
work="$root/build/slice7-final-integrated"
ev="$root/docs/projects/evidence/slice7-final-integrated/linux"
build="$work/linux-$profile"
for attempt in $(seq 1 1800); do test ! -s "$ev/$profile-ctest-exit.txt" || break; sleep 4; done
status=$(cat "$ev/$profile-ctest-exit.txt")
if [[ "$status" == 0 ]]; then
    printf '%s\n' 'Initial suite passed. No failed cases to retry.'
    exit 0
fi
export TMPDIR PYTHONDONTWRITEBYTECODE=1
TMPDIR=$(cat "$ev/$profile-temp.txt")
unset VOID_STDLIB_ROOT VOID_G4_EVIDENCE_ROOT VOID_G5_EVIDENCE_ROOT
if [[ "$profile" == strict ]]; then
    export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
    export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
fi
set +e
ctest --test-dir "$build" --parallel 2 --rerun-failed --output-on-failure \
    --output-junit "$ev/$profile-retry.xml" > "$ev/$profile-retry.txt" 2>&1
status=$?
set -e
cp "$build/Testing/Temporary/LastTest.log" "$ev/$profile-retry-LastTest.txt"
printf '%s\n' "$status" > "$ev/$profile-retry-exit.txt"
exit "$status"
