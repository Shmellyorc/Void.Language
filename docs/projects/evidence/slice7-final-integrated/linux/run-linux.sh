#!/usr/bin/env bash
set -euo pipefail
profile="$1"
root="$PWD"
work="$root/build/slice7-final-integrated"
evidence="$root/docs/projects/evidence/slice7-final-integrated/linux"
build="$work/linux-$profile"
source="$root"
testtmp=$(mktemp -d "/tmp/void-s7-integrated-$profile.XXXXXX")
printf '%s\n' "$testtmp" > "$evidence/$profile-temp.txt"
export TMPDIR="$testtmp" PYTHONDONTWRITEBYTECODE=1
unset VOID_STDLIB_ROOT VOID_G4_EVIDENCE_ROOT VOID_G5_EVIDENCE_ROOT
if [[ "$profile" == strict ]]; then
    export ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
    export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
fi
cache="$root/build/llvm23-$profile/CMakeCache.txt"
args=()
while IFS= read -r line; do
    args+=("-D$line")
done < <(grep -E '^(CMAKE_BUILD_TYPE|CMAKE_AR|CMAKE_CXX_COMPILER|CMAKE_C_COMPILER|CMAKE_LINKER|CMAKE_NM|CMAKE_RANLIB|LLVM_DIR|VOID_CLANG|VOID_LINKER|VOID_DWARFDUMP|VOID_HOST_DEPS_ROOT|VOID_HTTP_DEPS_ROOT|VOID_LLVM_RESEARCH|VOID_SANITIZERS):' "$cache")
{
    printf 'ASAN_OPTIONS=%s UBSAN_OPTIONS=%s\n' "${ASAN_OPTIONS:-}" "${UBSAN_OPTIONS:-}"
    printf '%q ' cmake -S "$source" -B "$build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$root/.toolchains/ninja-1.13.2/ninja" -DBUILD_TESTING=ON "-DVOID_STDLIB_ROOT=$root/stdlib" "${args[@]}"
    printf '\n'
    cmake -S "$source" -B "$build" -G Ninja "-DCMAKE_MAKE_PROGRAM=$root/.toolchains/ninja-1.13.2/ninja" -DBUILD_TESTING=ON "-DVOID_STDLIB_ROOT=$root/stdlib" "${args[@]}"
} >"$evidence/$profile-configure.txt" 2>&1
cmake --build "$build" --parallel 3 >"$evidence/$profile-build.txt" 2>&1
ctest --test-dir "$build" --show-only=json-v1 >"$evidence/$profile-test-plan.json"
set +e
ctest --test-dir "$build" --parallel 2 --output-on-failure --output-junit "$evidence/$profile.xml" >"$evidence/$profile-ctest.txt" 2>&1
status=$?
set -e
cp "$build/Testing/Temporary/LastTest.log" "$evidence/$profile-LastTest.txt"
printf '%s\n' "$status" >"$evidence/$profile-ctest-exit.txt"
exit "$status"
