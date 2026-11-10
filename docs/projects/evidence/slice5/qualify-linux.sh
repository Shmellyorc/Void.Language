#!/bin/bash
set -euo pipefail
profile=$1
build="$PWD/build/llvm23-$profile"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
targets=(voidc voidls void_project_creation void_project_manifest void_project_header void_project_selection void_project_sessions void_project_application void_native_inventory voidup)
cmake --build "$build" -j2 --target "${targets[@]}" >"build/slice5/linux-$profile-final-build.log" 2>&1
ctest --test-dir "$build" --output-on-failure -j3 -R '^(project_|cli_contract$|cli_install$)' --output-junit "$PWD/build/slice5/linux-$profile-final-ctest.xml" >"build/slice5/linux-$profile-final-ctest.log" 2>&1
