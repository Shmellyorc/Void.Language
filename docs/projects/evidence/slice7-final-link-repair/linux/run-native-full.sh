#!/usr/bin/env bash
set -euo pipefail
profile=$1
repeat=()
if [[ "${2:-}" == --retry-failed-once ]]; then
    repeat=(--repeat until-pass:2)
fi
root=$PWD
work="$root/build/slice7-final-link-repair"
prior="$root/build/slice7-final-integrated/linux-$profile"
ev="$root/docs/projects/evidence/slice7-final-link-repair/linux"
build="$work/native-linux-$profile"
for mode in release debug strict; do
    [[ "$(cat "$ev/$mode-focused-exit.txt")" == 0 ]]
done
export PYTHONDONTWRITEBYTECODE=1
unset VOID_STDLIB_ROOT VOID_G4_EVIDENCE_ROOT VOID_G5_EVIDENCE_ROOT
if [[ "$profile" == strict ]]; then
    export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
    export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
fi
temp=$(mktemp -d "/tmp/void-s7-link-$profile-full.XXXXXX")
printf '%s\n' "$temp" > "$ev/$profile-full-temp.txt"
export TMPDIR="$temp"
printf 'parallel=2 repeat=%s ASAN_OPTIONS=%s UBSAN_OPTIONS=%s\n' "${repeat[*]:-none}" "${ASAN_OPTIONS:-}" "${UBSAN_OPTIONS:-}" > "$ev/$profile-full-environment.txt"
if [[ ! -e "$build/CTestTestfile-canonical.cmake" ]]; then
    python3 "$ev/prepare-build.py" "$profile" --native > "$ev/$profile-native-clone.json"
    python3 - "$build/CMakeCache.txt" "$prior" "$build" <<'PY'
from pathlib import Path
import sys
cache=Path(sys.argv[1])
source, target=sys.argv[2:]
text=cache.read_text()
assert f'CMAKE_CACHEFILE_DIR:INTERNAL={source}' in text
cache.write_text(text.replace(source,target))
PY
    cmake -S "$root" -B "$build" > "$ev/$profile-native-configure.txt" 2>&1
    cmake --build "$build" --parallel 4 > "$ev/$profile-native-build.txt" 2>&1
    ctest --test-dir "$build" --show-only=json-v1 > "$ev/$profile-native-plan.json"
    cp "$build/CTestTestfile.cmake" "$ev/$profile-native-registered.cmake.txt"
    python3 "$ev/isolate-full-tests.py" "$profile" --native > "$ev/$profile-native-inventory.json"
else
    cmp "$build/CTestTestfile-canonical.cmake" "$ev/$profile-native-registered.cmake.txt"
fi
ctest --test-dir "$build" --show-only=json-v1 > "$ev/$profile-full-plan.json"
set +e
ctest --test-dir "$build" --parallel 2 "${repeat[@]}" --output-on-failure \
    --output-junit "$ev/$profile-full.xml" > "$ev/$profile-full.txt" 2>&1
status=$?
set -e
cp "$build/Testing/Temporary/LastTest.log" "$ev/$profile-full-LastTest.txt"
printf '%s\n' "$status" > "$ev/$profile-full-exit.txt"
exit "$status"
