#!/usr/bin/env bash
set -euo pipefail

# No SDK inputs. The link definitions below cover only the public C adapter
# entry point and the dyld binder needed by LLD for this executable.
output=$1
clang=$2
linker=$3
mkdir -p "$output"
cd "$output"
cat > main.c <<'SOURCE'
extern int void_adapter_probe(void);
int main(void) { return void_adapter_probe() != 42; }
SOURCE

for architecture in x86_64 arm64; do
    mkdir "$architecture"
    cat > "$architecture/libVoidProbe.tbd" <<STUB
--- !tapi-tbd
tbd-version: 4
targets: [ $architecture-macos ]
install-name: '@rpath/libVoidProbe.dylib'
exports:
  - targets: [ $architecture-macos ]
    symbols: [ _void_adapter_probe ]
...
STUB
    cat > "$architecture/libSystem.tbd" <<STUB
--- !tapi-tbd
tbd-version: 4
targets: [ $architecture-macos ]
install-name: '/usr/lib/libSystem.B.dylib'
exports:
  - targets: [ $architecture-macos ]
    symbols: [ dyld_stub_binder ]
...
STUB
    "$clang" --target="$architecture-apple-macosx14.0" -ffreestanding -c main.c -o "$architecture/main.o"
    "$linker" -arch "$architecture" -platform_version macos 14.0 14.0 -adhoc_codesign \
        -rpath '@executable_path' -o "$architecture/app" "$architecture/main.o" \
        "$architecture/libVoidProbe.tbd" "$architecture/libSystem.tbd"
done
