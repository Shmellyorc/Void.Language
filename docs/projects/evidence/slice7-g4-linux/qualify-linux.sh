#!/usr/bin/env bash
set -euo pipefail
profile=$1
output=$(realpath docs/projects/evidence/slice7-g4-linux/linux)
build="build/llvm23-$profile"
mkdir -p "$output"
export ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=abort_on_error=1:halt_on_error=1:print_stacktrace=1
printf '%s\n' "ASAN_OPTIONS=$ASAN_OPTIONS" "UBSAN_OPTIONS=$UBSAN_OPTIONS" > "$output/$profile-sanitizers.txt"
targets=(voidc voidls void_tests void_numeric_enum_ir void_native_inventory
  void_project_application void_project_sessions void_project_selection void_project_manifest void_project_header
  void_project_creation void_fixed_arrays_tests void_async_tests void_stage10_fields void_native_tests
  void_g1_tests void_g1_aot_normal void_g1_aot_optimized void_g1_aot_mismatch
  void_g2_tests void_g2_aot_normal void_g2_aot_optimized
  void_g3_tests void_g3_aot_normal void_g3_aot_optimized
  void_g4_tests void_g4_aot_normal void_g4_aot_optimized void_g4_aot_startup
  void_native_aot_main_normal void_native_aot_main_optimized void_native_aot_terminated_nul_normal
  void_native_aot_closed_optimized void_native_aot_duplicate_normal)
selection='^(ffi_g[1234]_.*|native_source|native_aot_(main_(normal|optimized)|terminated_nul_normal|closed_optimized|duplicate_normal)|stage6_integrated|stage6_cache_processes|numeric|numeric_cli|numeric_enum_.*|fixed_arrays_source|async_ir|stage10_fields|frontend|modules|gc|reload|sessions|analysis|project_(manifest_header|manifest_no_authority|manifest_provenance|application|application_process|creation|creation_process|sessions|selection|profiles|protocol|manifest|manifest_legacy|discovery_cli|nested_cli))$'
cmake -S . -B "$build" > "$output/$profile-configure.txt" 2>&1
cmake --build "$build" --parallel 2 --target "${targets[@]}" > "$output/$profile-build.txt" 2>&1
ctest --test-dir "$build" --output-on-failure --output-junit "$(realpath "$output")/$profile-ctest.xml" \
  -j 2 -R "$selection" > "$output/$profile-ctest.txt" 2>&1

cp "$build/Testing/Temporary/LastTest.log" "$output/$profile-LastTest.txt"
