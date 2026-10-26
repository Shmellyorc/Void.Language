> **26 October 2026 combined-sprint update:** Public failures now use managed
> errors derived from `error.error`; inspect `reason.kind` using `error_kind`.
> [The current combined contract](../combined/CONTRACT.md) and
> [implementation/evidence](../combined/IMPLEMENTATION.md) supersede the earlier
> source-protection and implementation-cost statements below. The original
> sprint receipt and source hashes remain historical evidence.

# encoding/binary implementation and Wrench handoff

26 October 2026. Implementation of Boss-authorized Sprint 1 only. Independent
Golden Wrench review and Boss-controlled cross-platform acceptance are pending.
The [contract](CONTRACT.md) records the capability audit and complete public API.

## Implementation

`stdlib/encoding/binary.void` contains the entire shared numeric byte codec:
one generic encoder, one generic decoder, byte-order selection and exact input
count validation. Ten numeric specializations reuse these same source bodies.
The encoder extracts unsigned base-256 digits; big endian returns an independent
reversed slice. The decoder folds bytes from most significant to least significant.
At most eight bytes are accumulated, so every intermediate fits u64. It validates
count before indexing. Unsigned remainder bounds every byte cast to 0..255.

Three private operations in `src/frontend/binary_codec.inc`, admitted only in the
canonical standard-library module, check the concrete numeric family and expose
byte width and representation bits. Lowering in `src/frontend/compiler.cpp`
uses existing numeric `bitcast` and checked unsigned `convert` IR. This preserves
signed minima and IEEE NaN payloads without host byte copies or floating arithmetic.
No Runtime/LLVM backend changes or new native entry points were needed.

The new completion test found an existing crash when listing a generic function
whose return header is an unresolved collection pattern (`encode<t> -> [u8]`).
Completion now displays the source pattern without demanding its element layout.
Concrete specialization, type validation, documentation extraction and the locked
signature-help contract remain unchanged.

## Focused verification

Final commands, results and source hashes are recorded alongside this handoff.
See [the machine-readable results](evidence/verification.json),
[exact commands](evidence/commands.txt) and [source hashes](evidence/source-sha256.json).

| Linux configuration | Distinct selected suites | Final binary/documentation source recheck |
|---|---|---|
| Debug | 22/22 passed (16 regressions + 6 binary/documentation) | 6/6 passed |
| Release | 22/22 passed (19 regression/tooling + 3 binary) | 6/6 passed |
| RelWithDebInfo, strict ASan/UBSan/leaks | 10/10 passed | Included in the 10/10 run |

There are **54 distinct configuration/suite combinations, zero final failures and
zero skips**. Development repetitions are not added to this coverage count.
The final source recheck follows a readability-only split of chained private
helper assignments. Each configuration reports **2,251 ownership assertions**,
**183 failing allocation budgets in each normal/O2 mode**, **81 LSP assertions**,
**19 codec-driver processes**, and **zero module documentation warnings**.
Debug and Release each pass **seven actual 1 MiB stack processes**.

[Debug focused details](evidence/debug-details.log),
[Debug regressions](evidence/debug-regressions.log),
[Release final focused details](evidence/release-final-details.log),
[Release regression details](evidence/release-regressions-details.log), and
[strict sanitizer details](evidence/sanitizer-details.log) retain exact outputs.
Strict settings are `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. The official SDK's
RTTI-off ABI makes vptr checking unavailable; supported address/undefined checks
and leak detection remain enabled. The final sanitizer build passed with a
fresh make invocation after the original invocation lacked the newly registered
ownership target in its loaded rules.

The generator reports 356 cases as "vectors": 346 numeric/IEEE byte-vector cases
plus ten invalid-width sweeps (each sweep checks counts 0..16 in both orders).
The following suites exercise actual standard-library imports:

* `binary_execution`: independent Python integer.to_bytes and struct.pack known
  vectors; every numeric type, both orders, minima/maxima, zero/negative values,
  seeded samples, IEEE signed zeros/subnormals/finite extrema/infinities and signed
  signaling/quiet NaN payloads. It compares actual decoded numeric values for
  ordinary values and exact re-encoded bits for all IEEE patterns. Each numeric
  type also checks counts 0..16 in both orders, unchanged const inputs, fresh
  outputs, recoverable wrong_width and `?` propagation. Check, normal/O2 JIT,
  linked build/native execution in both modes, and unsupported/private API rejection.
* `binary_ownership`: source arrays remain live through forced collection;
  allocation-budget sweep checks failure cleanup, transactional publication,
  precise roots, empty post-call collection and successful retry after each budget,
  in normal/O2 JIT. Public declarations/parameters/returns are audited through
  compiler metadata, and both diagnostic/warning collections must be empty.
* `lsp_binary`: real stdio server, nested import completion, module completion,
  source navigation, enum/function hover, return descriptions and active-parameter
  signature documentation through automatic/manual requests. Private compiler
  operations never appear in public completion.
* `binary_stack`: actual 1 MiB POSIX stack for source check, normal/O2 JIT and
  linked build/native execution. Sanitizers use the normal stack because ASan
  instrumentation is not the native small-stack configuration.

## Changed files

| File | Change |
|---|---|
| `stdlib/encoding/binary.void` | Complete public numeric codec and documentation |
| `src/frontend/binary_codec.inc` | Private concrete numeric representation checks |
| `src/frontend/compiler.cpp` | Existing-IR lowering and generic collection completion repair |
| `CMakeLists.txt` | Four focused CTest registrations and ownership test target |
| `tests/binary/execution.py` | Independent numeric/IEEE vectors, width errors, JIT/AOT and rejection checks |
| `tests/binary/ownership.cpp` | Public documentation audit, precise GC and allocation failure sweeps |
| `tests/binary/ownership.void` | Actual source ownership and collection fixture |
| `tests/binary/lsp.py` | Actual compiler-owned editor protocol checks |
| `tests/binary/stack.py` | Actual 1 MiB stack checks |
| `stdlib/README.md` | Binary module usage pointer |
| `docs/README.md` | Sprint contract/evidence index, preserving historical locks |
| `docs/binary/CONTRACT.md` | Capability audit and full public contract |
| `docs/binary/IMPLEMENTATION.md` | This review handoff and completeness audit |
| `docs/binary/evidence/` | Exact commands, test outputs, results and source hashes |

## Development failures retained honestly

The first execution fixture compared an i32 loop count with an i64 array count;
the fixture now uses an explicit checked widening. The first LSP run exposed the
generic collection completion defect described above; it is repaired. The first
signature fixture assumed plaintext capabilities without advertising them; it now
requests the supported plaintext representation explicitly.

The initial ownership probe expected list allocation to use the managed-struct
construction observer. It now includes a small ordinary managed marker to force
collection while codec arrays are rooted. Combining forced collection with an
exhausted allocation budget caused the existing probe's deliberate termination
when collection could not allocate. Forced collection and allocation sweeps are
tested separately, as in the established array/console suites; no production
failure behavior was weakened. The sweep checks every budget through first
success and retries each invocation after fault injection is disabled.

Two overlapping sanitizer configure attempts initially raced in CMake's scratch
directory and one linker reported a missing temporary output. The subsequent
configure/build result is recorded separately; this is a build orchestration
failure, not a codec or LLVM DSE repair. Stage 11's historical DSE observation and
all locked evidence remain unchanged.

The first strict sanitizer test attempt failed **0/9** at LeakSanitizer startup:
the execution tool's tracing sandbox prevents LSan thread inspection and reports
ptrace unsupported. [That original output](evidence/sanitizer-sandbox-failure.log)
is retained. Approved execution outside that sandbox passed **10/10** with the
same strict environment, including the newly built ownership target. No leak or
undefined-behavior option was disabled to obtain the final pass.

## Completeness and limits

The public API is exactly two enum types (three documented cases) and two generic
functions, each with documented explicit parameters and return behavior. All ten
numeric types and both orders are implemented. Every exact-width numeric bit
pattern is valid; no malformed-payload error is invented. No public offsets,
cursor, partial-buffer, bool, string, enum or arbitrary object serialization API
is claimed. Existing Runtime allocation failures remain RuntimeFailure.

Only focused local Linux evidence is claimed. Native Windows/macOS and a
big-endian host have not been executed. Cross-platform release acceptance belongs
to Boss; the codec's byte-order semantics use arithmetic rather than host layout.
Physical editor popup lifecycle checks were not repeated; compiler-owned LSP
responses are tested directly and the documentation lock remains in force.

This sprint did not write `stdlib/console.void` or root `README.md`. A concurrent
formatting diff appeared in `stdlib/console.void` during verification; it was left
intact and is excluded from this implementation's file list. Its observed original
SHA-256 was `4bf8ead48a350c9ecd7147bed4b8ef282a3a873169b5fa24ab4b36f2e04c791a`.
No branch, commit, push, hosted CI or other standard-library module was started.
