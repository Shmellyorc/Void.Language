#!/usr/bin/env bash
set -euo pipefail

# Run from the repository root using the existing LLVM 23.1.4 build trees.
mkdir -p -- "${1:?supply an evidence directory}"
output=$(realpath -- "$1")
tests='^(numeric_enum_.*|numeric|numeric_cli|numeric_sprint2|frontend|modules|gc|reload|sessions|analysis|native|fixed_arrays_(source|cli)|async_ir|stage10_fields(_aot)?|project_discovery_cli|project_nested_cli)$'

for profile in release debug strict; do
    (
        if [ "$profile" = strict ]; then
            export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
            export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
        fi
        build="$PWD/build/llvm23-$profile"
        cmake --build "$build" -j2 --target voidc voidls void_numeric_enum_ir void_tests \
            void_fixed_arrays_tests void_async_tests void_stage10_fields > "$output/$profile-build.txt" 2>&1
        ctest --test-dir "$build" --output-on-failure -j2 -R "$tests" \
            --output-junit "$output/$profile-ctest.xml" > "$output/$profile-ctest.txt" 2>&1
        sha256sum "$build"/{voidc,voidls,void_numeric_enum_ir,void_tests} > "$output/$profile-binaries.sha256"
    )
done
