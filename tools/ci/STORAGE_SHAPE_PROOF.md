> Active tools now require LLVM/Clang/LLD **23.1.4**. Historical measurements
> and LLVM 19 source references below remain evidence from their original runs;
> they do not certify LLVM 23 results. See [migration](../../docs/llvm23/COMPATIBILITY.md).

# P23-2 storage-shape proof reproduction

LLVM 23.1.4 and its matching development SDK are required. Public AOT stays on its
original path. The explicit compiler-fixture entry shares the real AOT module and
CLI adapter producer, then invokes the staged service. It does not permit source
native arrays or activate ORC/LSP certification.

```sh
cmake --build build/ci-debug -j4
ctest --test-dir build/ci-debug -R '^arrays_storage_audit$' --output-on-failure
python3 tools/ci/aot_certificate_corpus.py --build build/ci-debug --root . \
  --cxx /path/to/llvm-23.1.4/bin/clang++ --output build/aot-certificate-corpus-example
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
  --llvm-bin /path/to/llvm-23.1.4/bin --output build/aot-incoming-example
```

This requires native Linux x64, GDB and permitted ptrace. It executes only distinct
ordinary control executables produced by the integration scripts. GDB checks entry,
call, callee-entry, return and cleanup SP; actual argument addresses/bytes/alignment;
and numeric's measured hidden-result pointer and written result fields. Failed
certificate outputs are never executed. The normal-profile witnesses do not imply
an O2/native Windows/macOS ABI proof or a complete caller/callee charging rule.

The historical pinned MIR observation for the numeric normal fixture is reproducible
with the preserved LLVM 19.1.7 tools (find its exact record/object path in
records.jsonl, then use that object's .optimized.ll snapshot):

```sh
/path/to/llvm-19.1.7/bin/llc -O2 -relocation-model=pic -mcpu=generic \
  -stop-after=finalize-isel input.o.optimized.ll -o numeric-finalize-isel.mir
```

Inspect the call's LEA from stack.47 to RDI and post-call loads from that slot,
with size=64/alignment=8 and no AllocaInst identity. This is independent compile
inspection, not certified delivery or native execution of a rejected object.

For the current managed handoffs case, an explicit accepted subset can be tested
at inherited and 1 MiB stack limits. LLVM 23 O2 now clones four homes in main;
that research certificate fails closed, as recorded in the migration compatibility
notes. The original LLVM 19 main acceptance remains historical:

```sh
python3 tools/ci/storage_shape_probe.py --build build/ci-debug --root . \
  --cxx /path/to/llvm-23.1.4/bin/clang++ --llvm-bin /path/to/llvm-23.1.4/bin \
  --programs handoffs --output build/aot-canonical-small-stack-example
```

The default still selects all three historical programs, including null_failure,
and fails if any selected object rejects. Part 3 leaves its optimized inline homes
unproved; the full corpus records that rejection. The selector never silently
skips failed candidates or relaxes exit/crash classification. See proposal Section
18 for the proof limits and actual evidence.

## Part 4: outgoing depth observations and final-CFG negative

Run the focused compiler suite first to produce current normal/O2 fixture IR and
reports, then use a fresh output directory:

```sh
ctest --test-dir build/ci-debug -R '^arrays_storage_audit$' --output-on-failure
python3 tools/ci/aot_outgoing_probe.py \
  --fixtures 'build/ci-debug/storage shape fixtures' \
  --llvm-bin /path/to/llvm-23.1.4/bin --output build/aot-outgoing-example
```

This native Linux x64 driver requires permitted GDB ptrace and LLVM 23.1.4. It
compiles distinct ordinary controls from the saved optimized IR, independently
links their C caller/callees, checks exact results and side effects at inherited
and 1 MiB stack limits, and measures caller SP while stepping over callees.
No staged rejected object is linked or executed. ASLR remains enabled.

The nine normal/O2 cases cover reserved and nonreserved areas, padding, two calls,
both branch paths, three loop iterations, two near-limit homes and a hidden result.
The compiler suite also retains null-bound tail/dynamic/indirect/realignment
negatives and owner-destination assertions. An observation is not a certificate:
final_encoded_cfg_bound is false and no reconciliation or incoming credit is given.

The driver also retains a bytes-only negative whose conditional branch targets
frame setup instead of the original alternative. It independently disassembles
both files and requires identical flattened SP/call lists. The modified file is
never executed or submitted for delivery. encoded-cfg-gap.json documents why the
existing linear final check cannot certify machine CFG path multiplicity.
See proposal Section 19 for the activation stop and required architectural review.

## Part 5A: read-only encoded-CFG pilot

Build/run the existing audit suite to generate current ordered machine observations.
The outgoing driver above can generate ordinary native controls in a fresh directory.
Then run:

```sh
python3 tools/ci/aot_encoded_cfg_probe.py \
  --audit build/ci-debug/void_arrays_storage_audit \
  --fixtures 'build/ci-debug/storage shape fixtures' \
  --controls build/aot-outgoing-example --llvm-bin /path/to/llvm-23.1.4/bin \
  --output build/aot-encoded-cfg-example
```

For the exact retained Part 4 mandatory pair, use
--controls build/p23-2-part4-outgoing-final. The driver verifies both hashes against
encoded-cfg-gap.json, requires the original to pass and the never-executed retarget
to reject, and preserves all inputs. Its other altered copies are never executed.
It also tests normal/O2 linked and relocatable controls and explicit malformed
instruction, edge, relocation and machine-evidence cases. This driver needs no
ptrace; only the separate outgoing native-witness driver does.

The read-only fixture command for one function is:

```sh
build/ci-debug/void_arrays_storage_audit --research-cfg object.o sample \
  machine.certificate.json research.json normal
```

Use O2 for an O2 profile or '-' for no machine comparison. Exit 0 means supported
research CFG analysis, exit 2 means research rejection. Neither authorizes delivery.
JSON always records input SHA-256, profile and false delivery/ownership/memory-bound
credits. Public AOT and all old staged certification decisions stay unchanged.
See proposal Section 20 for the explicit opcode/relocation subset, conditional C
call-continuation contract, selected-function scope and remaining origin/ABI gates.
