#!/usr/bin/env bash
set -euo pipefail

# Run from the repository root with existing LLVM 23.1.4 build directories.
mkdir -p -- "${1:?supply an evidence directory}"
output=$(realpath -- "$1")
tests='^(numeric_enum_.*|numeric|numeric_cli|numeric_sprint2|frontend|modules|gc|reload|sessions|analysis|project_(creation_process|creation|profiles|sessions|protocol|application|application_process|selection|discovery_cli|manifest_legacy|nested_cli|manifest))$'

for profile in release debug strict; do
    build="$PWD/build/llvm23-$profile"
    (
        if [ "$profile" = strict ]; then
            export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
            export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
        fi
        set -x
        cmake --build "$build" -j2 --target voidc voidls void_numeric_enum_ir void_tests \
            void_native_inventory void_project_creation void_project_application void_project_sessions \
            void_project_selection void_project_manifest > "$output/$profile-build.txt" 2>&1
        ctest --test-dir "$build" --output-on-failure -j2 -R "$tests" \
            --output-junit "$output/$profile-ctest.xml" > "$output/$profile-ctest.txt" 2>&1
        cp "$build/numeric-enum-process.json" "$output/$profile-process.json"
        cp "$build/numeric-enum-lsp.json" "$output/$profile-lsp.json"
        sha256sum "$build"/{voidc,voidls,void_tests,void_numeric_enum_ir} > "$output/$profile-binaries.sha256"
    )
done
