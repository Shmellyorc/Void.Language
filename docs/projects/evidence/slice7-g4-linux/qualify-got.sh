#!/usr/bin/env bash
set -euo pipefail
profile=$1
repo=$(pwd -P)
build="$repo/build/llvm23-$profile"
bundle="$build/ffi-g4"
work="$build/ffi-g4-got"
mkdir -p "$work/native"
cc="$repo/.toolchains/llvm-23.1.4/bin/clang"
src="$repo/tests/ffi"
read -r -a cflags <<< "$(sed -n 's/^C_FLAGS = //p' "$build/CMakeFiles/void_g4_main.dir/flags.make")"
"$cc" "${cflags[@]}" -fno-plt -c "$src/dynamic_fixture.c" -o "$work/provider-got.o"
cp "$bundle/native/libvoid_g4_dep.so" "$work/native/libvoid_g4_dep.so"
"$cc" -fuse-ld=lld -nostdlib -shared -Wl,-soname,libvoid_g4_main.so \
    -Wl,--hash-style=sysv,-z,now,-z,relro,-z,defs "$work/provider-got.o" \
    "$build/CMakeFiles/void_g4_main.dir/tests/ffi/dynamic_thunk.c.o" \
    "$work/native/libvoid_g4_dep.so" -o "$work/native/libvoid_g4_main.so"
readelf -Wr "$work/native/libvoid_g4_main.so"
readelf -Wr "$work/native/libvoid_g4_main.so" | grep 'R_X86_64_GLOB_DAT.*g4_bias'
readelf -Wr "$work/native/libvoid_g4_main.so" | grep 'R_X86_64_JUMP_SLOT.*g4_close'
python3 "$src/dynamic_plan.py" "$bundle/dynamic.descriptor" "$work/native/libvoid_g4_main.so" \
    "$work/native/libvoid_g4_dep.so" "$work/closure.txt"
export ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=1
export UBSAN_OPTIONS=abort_on_error=1:halt_on_error=1:print_stacktrace=1
"$build/void_g4_tests" "$work/closure.txt" "$src/dynamic.void" "$work/native" "$work/probes" \
    "$bundle/outside/libvoid_g4_dep.so" "$bundle/initializer/libvoid_g4_dep.so"
sha256sum "$work/"{closure.txt,provider-got.o,native/libvoid_g4_main.so,native/libvoid_g4_dep.so}
