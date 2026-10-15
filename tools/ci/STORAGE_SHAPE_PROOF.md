# P23-2 storage-shape proof reproduction

LLVM 19.1.7 and its matching development SDK are required. Public AOT stays on its
original path. The explicit compiler-fixture entry shares the real AOT module and
CLI adapter producer, then invokes the staged service. It does not permit source
native arrays or activate ORC/LSP certification.

```sh
cmake --build build/ci-debug -j4
ctest --test-dir build/ci-debug -R '^arrays_storage_audit$' --output-on-failure
python3 tools/ci/aot_certificate_corpus.py --build build/ci-debug --root . \
  --cxx /usr/bin/clang++-19 --output build/aot-certificate-corpus-example
```

The corpus driver requires a fresh directory and native Linux x64. It reads the
configured integration commands from CTest JSON and the seven source families
from CMake. Integration scripts keep their original compiler outputs and assertions;
a fixture wrapper separately attempts certification at the exact source snapshot,
including edited/imported/generated sources. Original `emit` failures are not native
fixtures and remain source-diagnostic tests. Architecture and edited-source fixtures
are additionally attempted in normal mode because their ordinary scripts emit O2
only. Both profiles are otherwise already covered by the original corpus.

Library/reload producers accept an explicit final `--storage-proof` test argument.
The generated-runtime fixture uses the same option. Their original modes still call
the public emitter. No environment switch or public CLI option activates the service.
Accepted objects are linked separately with the same native callers/runtime. Failed
certificates must preserve an existing sentinel output, and are never linked or run.
The driver compares CLI exit/stdout/stderr and executes the original library harness
assertions where every required object certifies. A skipped harness is recorded as
rejected, never as a pass. `complete.json` means investigation coverage completed;
its accepted/rejected counts report certification separately. `records.jsonl` retains
all commands, source routes, profiles and rejection reasons. Objects, three IR
snapshots, certificates, native binaries and harness logs remain in that directory.
SHA-256 checks independently bind each accepted final object, normalized frozen IR,
optimized IR and serialized inventory to its report. Preserve the C++ driver name:
resolving a clang++ symlink to clang changes automatic C++ library linking.

The older `storage_shape_probe.py` records the Section 16 historical experiment.
The stronger unknown-home rule now rejects its optimized managed cases, so it is
not an all-pass acceptance driver for the present service. No prior 1 MiB result is
presented as validation of a newly rejected object. Use the corpus driver above.

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
these Linux drivers. See ARRAYS_PROPOSAL.md Sections 16 and 17 for the proof boundaries.

## Part 3: incoming storage and the narrow count rewrite

The production audit now records post-pass storage observations and observes the
actual InstCombine replacement edge for declared Frame/staging byte-count homes.
Its canonical_byte_array_homes retain the original P and grant zero R credit.
Unknown origins, inline clones and incoming ownership still reject. The PUSH/tail
normalizer is unchanged. These observations do not activate public certification.

After generating a fresh corpus as above, native original-control witnesses are:

```sh
python3 tools/ci/aot_incoming_probe.py --corpus build/aot-certificate-corpus-example \
  --llvm-bin /usr/lib/llvm-19/bin --output build/aot-incoming-example
```

This requires native Linux x64, GDB and permitted ptrace. It executes only distinct
ordinary control executables produced by the integration scripts. GDB checks entry,
call, callee-entry, return and cleanup SP; actual argument addresses/bytes/alignment;
and numeric's measured hidden-result pointer and written result fields. Failed
certificate outputs are never executed. The normal-profile witnesses do not imply
an O2/native Windows/macOS ABI proof or a complete caller/callee charging rule.

Independent pinned MIR observation for the numeric normal fixture is reproducible
with the installed LLVM 19.1.7 tools (find its exact record/object path in
records.jsonl, then use that object's .optimized.ll snapshot):

```sh
/usr/lib/llvm-19/bin/llc -O2 -relocation-model=pic -mcpu=generic \
  -stop-after=finalize-isel input.o.optimized.ll -o numeric-finalize-isel.mir
```

Inspect the call's LEA from stack.47 to RDI and post-call loads from that slot,
with size=64/alignment=8 and no AllocaInst identity. This is independent compile
inspection, not certified delivery or native execution of a rejected object.

For the newly reconciled managed main/handoffs cases, an explicit accepted subset
can be tested at inherited and 1 MiB stack limits:

```sh
python3 tools/ci/storage_shape_probe.py --build build/ci-debug --root . \
  --cxx /usr/bin/clang++-19 --llvm-bin /usr/lib/llvm-19/bin \
  --programs main handoffs --output build/aot-canonical-small-stack-example
```

The default still selects all three historical programs, including null_failure,
and fails if any selected object rejects. Part 3 leaves its optimized inline homes
unproved; the full corpus records that rejection. The selector never silently
skips failed candidates or relaxes exit/crash classification. See proposal Section
18 for the proof limits and actual evidence.
