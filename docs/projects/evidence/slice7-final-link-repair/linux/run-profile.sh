#!/usr/bin/env bash
set -euo pipefail
profile=$1
phase=$2
root=$PWD
work="$root/build/slice7-final-link-repair"
prior="$root/build/slice7-final-integrated"
evidence="$root/docs/projects/evidence/slice7-final-link-repair/linux"
build="$prior/linux-$profile"
copy="$work/linux-$profile"
if [[ "${VOID_REPAIR_MOUNTED:-}" != 1 ]]; then
    exec bwrap --bind / / --dev-bind /dev /dev --proc /proc --bind "$copy" "$build" --die-with-parent --chdir "$root" \
        --setenv VOID_REPAIR_MOUNTED 1 -- bash "$evidence/run-profile.sh" "$profile" "$phase"
fi
# The clone retains the original configured absolute path only in this namespace.
[[ -f "$build/.void-repair-copy" ]]
export PYTHONDONTWRITEBYTECODE=1
unset VOID_STDLIB_ROOT VOID_G4_EVIDENCE_ROOT VOID_G5_EVIDENCE_ROOT
if [[ "$profile" == strict ]]; then
    export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
    export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
fi
temp=$(mktemp -d "/tmp/void-s7-link-$profile-$phase.XXXXXX")
printf '%s\n' "$temp" > "$evidence/$profile-$phase-temp.txt"
export TMPDIR="$temp"
tests='^(ergonomics_sprint6|stage10_(fields|temporal|console|operators|conversion)_aot|stage11_globals_aot|application_native_sprint4|text_collections_sprint3)$'
if [[ "$phase" == focused ]]; then
    printf 'ASAN_OPTIONS=%s UBSAN_OPTIONS=%s\n' "${ASAN_OPTIONS:-}" "${UBSAN_OPTIONS:-}" > "$evidence/$profile-environment.txt"
    cmake -S "$root" -B "$build" > "$evidence/$profile-configure.txt" 2>&1
    cmake --build "$build" --parallel 3 > "$evidence/$profile-build.txt" 2>&1
    ctest --test-dir "$build" --show-only=json-v1 > "$evidence/$profile-plan.json"
    cp "$build/CTestTestfile.cmake" "$evidence/$profile-registered.cmake.txt"
    args=(--parallel 3 -R "$tests")
elif [[ "$phase" == full ]]; then
    args=(--parallel 5)
else
    exit 2
fi
set +e
ctest --test-dir "$build" "${args[@]}" --output-on-failure \
    --output-junit "$evidence/$profile-$phase.xml" > "$evidence/$profile-$phase.txt" 2>&1
status=$?
set -e
cp "$build/Testing/Temporary/LastTest.log" "$evidence/$profile-$phase-LastTest.txt"
printf '%s\n' "$status" > "$evidence/$profile-$phase-exit.txt"
exit "$status"
