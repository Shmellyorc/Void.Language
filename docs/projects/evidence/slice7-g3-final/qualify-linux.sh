#!/usr/bin/env bash
set -euo pipefail
profile=$1
evidence=$(realpath docs/projects/evidence/slice7-g3-final/linux)
build="build/llvm23-$profile"
export ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=abort_on_error=1:halt_on_error=1:print_stacktrace=1
printf '%s\n' "ASAN_OPTIONS=$ASAN_OPTIONS" "UBSAN_OPTIONS=$UBSAN_OPTIONS" > "$evidence/$profile-sanitizers.txt"
cmake --build "$build" --parallel 2 --target void_tests \
    void_g1_tests void_g1_aot_normal void_g1_aot_optimized void_g1_aot_mismatch \
    void_g2_tests void_g2_aot_normal void_g2_aot_optimized \
    void_g3_tests void_g3_aot_normal void_g3_aot_optimized > "$evidence/$profile-build.txt" 2>&1
ctest --test-dir "$build" --output-on-failure -j 2 -R '^(ffi_g[123]_.*|gc|reload|analysis)$' \
    --output-junit "$evidence/$profile-ctest.xml" > "$evidence/$profile-ctest.txt" 2>&1
cp "$build/Testing/Temporary/LastTest.log" "$evidence/$profile-LastTest.txt"
sha256sum "$build"/{void_tests,void_g3_tests,void_g3_aot_normal,void_g3_aot_optimized} \
    "$build"/ffi-g3/{resources.descriptor,application-normal.o,application-optimized.o} \
    "$build"/CMakeFiles/{void_g3_fixture.dir/tests/ffi/resources_fixture.c.o,void_g3_thunk.dir/tests/ffi/resources_thunk.c.o} \
    > "$evidence/$profile-artifacts.sha256"
