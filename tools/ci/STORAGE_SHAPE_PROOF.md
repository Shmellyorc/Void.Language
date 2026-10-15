# P23-2 storage-shape proof reproduction

LLVM 19.1.7 and its matching development SDK are required. Public AOT stays on its
original path. The explicit compiler-fixture entry shares the real AOT module and
CLI adapter producer, then invokes the staged service. It does not permit source
native arrays or activate ORC/LSP certification.

```sh
cmake --build build/ci-debug --target void_arrays_storage_audit voidc -j2
ctest --test-dir build/ci-debug -R '^arrays_storage_audit$' --output-on-failure
python3 tools/ci/storage_shape_probe.py --build build/ci-debug --root . \
  --cxx /usr/bin/clang++-19 --llvm-bin /usr/lib/llvm-19/bin \
  --output build/storage-shapes-native-example
```

The driver requires a fresh directory and native Linux x64. It links original and
certified objects independently against the same runtime archive, then compares
exit status, stdout and stderr for main, handoffs and null_failure, normal/O2,
eight argument cases, and inherited/1 MiB stacks. No unexpected signal is accepted.
It preserves commands, certificates, disassembly, input/normalized/optimized LLVM
IR, binaries and results. Preserve the C++ driver name: resolving a clang++ symlink
to clang changes automatic C++ library linking. `--cxx` intentionally retains it.

The CTest artifacts are in `build/ci-debug/storage shape fixtures`. The two exact
original managed modules without the CLI-home proof must be rejected without
replacing an existing output. Other negatives include unmarked dynamic homes,
cyclic/repeated CLI allocations, unsupported alignment/type/count/stack control,
untracked zero-size clones, missing inventories and over-budget homes. A physical
zero-size home is accepted only with exact original static-allocation provenance;
it receives zero subtraction credit. No guessed physical extent enters P.

The precise pinned LLVM rule is in
[FunctionLoweringInfo.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/CodeGen/SelectionDAG/FunctionLoweringInfo.cpp#L143-L159).
The installed SDK's real machine objects are checked by the compiler tests.

Controlled Debug interface investigation on Linux:

```sh
cmake --build build/ci-debug --target void_interfaces_tests -j2
python3 tools/ci/interfaces_probe.py --binary build/ci-debug/void_interfaces_tests \
  --root . --output build/interfaces-repeat-example \
  --serial 8 --parallel 24 --workers 4
```

The complete existing suite executes every time. Flushed phase/mode progress
identifies the last operation if a crash occurs. Each child has a separate working
directory and requests up to a 256 MiB core limit, subject to OS permissions.
A timeout or signal fails the batch; a later pass never overwrites a failure.
No special native optimization, assertion suppression or reduced test coverage is
used. The absence of a crash does not establish a fix or a regression attribution.

For a native debugger run, enable ASLR explicitly and preserve all-thread stacks
if the inferior stops abnormally:

```sh
gdb -nx -q -batch -ex 'set disable-randomization off' -ex run \
  -ex 'thread apply all bt full' --args build/ci-debug/void_interfaces_tests "$PWD"
```

GDB/LeakSanitizer may require execution outside a ptrace-restricted sandbox. Keep
leak detection and UBSan halt/stack reporting enabled; do not disable checks to
make a restricted run pass. No Windows/macOS native verification is implied by
these Linux drivers. See ARRAYS_PROPOSAL.md Section 16 for the proof boundary.
