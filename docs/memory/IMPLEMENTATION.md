> **26 October 2026 combined-sprint update:** Public failures now use managed
> errors derived from `error.error`; inspect `reason.kind` using `error_kind`.
> [The current combined contract](../combined/CONTRACT.md) and
> [implementation/evidence](../combined/IMPLEMENTATION.md) supersede the earlier
> source-protection and implementation-cost statements below. The original
> sprint receipt and source hashes remain historical evidence.

# io/memory — implementation and Wrench review handoff

26 October 2026. Boss-authorized Sprint 2 implements only `io/memory`, following
Wrench's source review of binary commit `64c485c5beb5df519e998fdb838a640fd9758376`.
Independent review of this sprint and Boss-controlled cross-platform acceptance
remain separate. No hosted CI, Git mutation or later module is authorized here.

The [contract and capability audit](CONTRACT.md) precede implementation and define
the public source API and observable behavior. Work proceeded sequentially through
capability audit, contract, owned state/seek, byte operations, numeric operations,
UTF-8/Pascal strings, documentation/formatting and focused verification.

## Source and failure model

`stdlib/io/memory.void` implements a single managed `stream`. Its private immutable
state holds bytes, position and canonical `binary.byte_order`. Aliases share the
outer stream identity; a complete new state is published through one reference
field after all fallible work. Constructor inputs, reads and `to_bytes()` snapshots
are independent byte arrays. Seeks/read advances share the private immutable
backing; writes stage a full independent copy. Neither readonly input nor a const
stream view grants mutation authority.

All exact read, raw-text and Pascal-frame validation occurs before cursor commit.
Pascal prefix and payload form one operation, including on truncation, invalid
UTF-8, caller budget rejection and resource failure. Up-to reads clamp to remaining
bytes; exact reads return unexpected_eof. Seek is absolute and bounded by the
existing data, with no gaps. Arithmetic uses subtraction-before-addition checks.
Pascal length narrows only after comparison with u32.max_value. Payload availability
and caller limits are checked before slicing or allocating from an untrusted prefix.

Numeric read/write call `binary.encode`/`binary.decode` directly for all ten types.
A zero encode supplies numeric width because source has no sizeof primitive.
String encode/strict decode reuse the compiler/Runtime UTF-8 implementation. No
numeric codec, UTF-8 validator, collection engine, IR opcode, Runtime ABI, allocator
or native resource was added. Stream objects need no close/deinit protocol.

## Necessary compiler-owned editor repair

The initial LSP completion test found that generic receiver methods such as
`read<t>`/`write<t>` were omitted until instantiated earlier in a file. The existing
candidate catalog scanned only concrete functions. Generic declaration lookup is
now shared with the existing method binder; editor listing reads the original
header and documentation without instantiating or checking its dependent body.
Direct-owner, import, private and conservative readonly restrictions remain in
force. No source binding/grammar/type feature is introduced. Tests also cover an
unused generic body that would fail if instantiated, private methods, readonly
receivers and transitive imports. Signature help continues to show only active
parameter documentation, including constructor defaults and generic writes;
parameterless reads return null rather than inventing parameter descriptions.

## Tests and final evidence

Final exact commands, accessible test receipts, counts and hashes are retained in
[the concise receipt](evidence/RECEIPT.md), [machine-readable results](evidence/verification.json),
[commands](evidence/commands.txt) and [source hashes](evidence/source-sha256.json).

| Linux configuration | Selected CTests | Final failures/skips |
|---|---:|---:|
| Debug | 21/21 passed | 0/0 |
| Release | 21/21 passed | 0/0 |
| RelWithDebInfo, strict ASan/UBSan/leaks | 19/19 passed | 0/0 |

This is **61 distinct configuration/suite combinations**, excluding development
repetitions. Each configuration reports **13,922 ownership assertions**, **749
failing allocation budgets in each normal/O2 mode**, **156 memory LSP checks**,
**35 execution-driver processes** and **zero module documentation warnings**.
Debug and Release additionally pass the existing complete frontend 1 MiB guard/
recovery script and seven processes each for expanded read-result independence.
That fixture expansion changes tests only; the complete sanitizer execution run
uses the expanded fixture. Memory-specific 1 MiB tests pass seven processes per
native configuration; binary stack tests remain unchanged. The existing Linux
Stage 11 global-stack gate is registered and passed in all three configurations.

Strict options remain `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. Approved local
execution outside the tracing sandbox lets LeakSanitizer inspect threads. No
sanitizer option was weakened. The official SDK has RTTI off, so vptr checking is
unavailable in that ABI; supported address/undefined checks and leaks are enabled.

The suites execute actual `use io/memory` source, without replacing
methods with host-language test implementations:

* `memory_execution`: byte ownership, shared aliases, snapshots, seek boundaries,
  overwrite/extension/tail preservation, zero/negative/huge requests, short/exact
  EOF, const inputs, strict UTF-8 including embedded NUL/BOM/max scalar and malformed
  sequences, Pascal framing/limits/truncation/recovery, manual numeric-prefix/raw
  string interoperability and `?` propagation. A Python bytearray oracle checks
  64 deterministic cursor transitions. Independent integer.to_bytes/struct.pack
  oracles and explicit IEEE patterns exercise 268 numeric cases in both orders,
  minima/maxima, subnormals, signed zeros, infinities and signed signaling/quiet
  NaN payloads. Three programs run through check, normal/O2 JIT, linked builds and
  native execution; 14 compile rejection cases cover readonly/private/type bounds.
* `memory_ownership`: precise roots, forced collection and fault injection across
  byte write, Pascal write, exact read, text read, Pascal read, numeric read, raw
  text write, seek, numeric write, up-to read, snapshots and construction. Every
  failed allocation budget checks unchanged visible bytes/cursor through aliases,
  no abandoned objects after collection and successful retry. Success verifies
  exact committed state. Global roots preserve stream identity between invocations;
  Runtime shutdown must release all handles. Compiler metadata audits public
  declarations, explicit parameters, returns and zero documentation warnings.
* `lsp_memory`: real server import/member completion, navigation, hover, constructor
  and method signature help, parameterless suppression and generic visibility.
* `memory_stack`: actual 1 MiB POSIX stack for check, normal/O2 JIT, linked build
  and native execution, including aliases, i64 minimum, f64 negative zero and Pascal
  Unicode/NUL. Existing binary suites remain unchanged and run as regressions.

## Changed files

| File | Change |
|---|---|
| `stdlib/io/memory.void` | Complete owned stream, byte/numeric/text/Pascal operations and public documentation |
| `src/frontend/generic_inference.inc` | Extract existing generic declaration lookup for shared binder/editor use |
| `src/frontend/compiler.cpp` | List visible generic receiver method headers without instantiating bodies |
| `CMakeLists.txt` | Four memory CTests and ownership target |
| `tests/memory/execution.py` | Numeric wire oracle, cursor model, CLI/JIT/AOT and rejection driver |
| `tests/memory/semantics.void` | Ownership, reads/writes, aliases, seeks, UTF-8, errors and Pascal interoperability |
| `tests/memory/ownership.cpp` | Compiler documentation audit, precise GC, atomic failure/cleanup/retry probes |
| `tests/memory/ownership.void` | Persistent alias/state fixture covering twelve operations |
| `tests/memory/lsp.py` | Real compiler-owned documentation/completion and generic visibility checks |
| `tests/memory/stack.py`, `tests/memory/stack.void` | Native 1 MiB source/JIT/AOT checks |
| `stdlib/README.md`, `docs/README.md` | Module and evidence index entries |
| `docs/memory/CONTRACT.md`, `docs/memory/IMPLEMENTATION.md` | Contract, audit, review handoff and limits |
| `docs/memory/evidence/` | Concise receipt, exact output/commands, failures, results and hashes |

## Retained development findings

Initial source checks and execution probes passed the ownership, seek, byte,
all-ten-numeric-type and UTF-8/Pascal stages before the complete suite was added.
The original public metadata audit incorrectly included public fields belonging
to the private state type; the audit now follows owner visibility. That was a test
classification issue, not missing public API documentation.

Fault injection found a genuine post-commit failure in byte writes: evaluating
`bytes.count` for the return value could allocate metadata after publishing the
new state (normal JIT scenario 0, budget 91). The implementation now captures the
count before staging/commit and returns the local scalar afterward. The full fault
sweep then passed 749 failing budgets per normal/O2 mode across twelve operations,
including reset/collection/retry after every failure. The failure was repaired,
not hidden by weakening the atomic-state assertion. Original receipts are retained.

The cursor-model fixture was also refined to issue the original oversized up-to
request rather than its already clamped expected count. Final receipts refer to
the corrected model. Historical binary/Stage 11/documentation evidence is unchanged.

## Completeness and limits

The API consists of two public types (`stream`, `error`), four error cases, one
constructor, five readonly properties and eleven methods. It implements all ten
numeric primitives, both byte orders, raw strings, u32 byte-prefixed Pascal
strings, reads, writes, seeking and snapshots. Recoverable errors and Runtime
allocation failures preserve the previous logical state; no partial frame is exposed.

Writes cost O(buffer length + input length) and stage an additional full copy;
repeated small writes can be quadratic. Reads/seeks allocate a small state object,
and numeric reads allocate a tiny encoded zero for width discovery. These costs
are disclosed for review; no optimized streaming performance is claimed.
There is no synchronization, borrowing mode, truncate/clear extension, configurable
mid-stream endian setter, filesystem, native I/O or async stream API in this slice.

Native Windows/macOS and big-endian execution remain unperformed; Boss reserves
cross-platform acceptance until the major phases are complete. Physical multi-GiB
buffers/strings were not allocated to cross i64 growth or u32 write-prefix limits;
those guards are source-audited, while malicious u32 read lengths are executed and
rejected before payload allocation. Memory/binary sanitizer tests use the normal
process stack and their native 1 MiB tests run separately; the existing Stage 11
global-stack sanitizer gate also passed. LSP protocol evidence does not repeat physical
editor popup lifecycle acceptance from the locked documentation repair.

`stdlib/console.void`, root `README.md`, binary source and binary tests remain
unchanged. The final source snapshot records their hashes. Stop here for Golden
Wrench review; no next module or hosted CI was started.
