# Stage 10: complete console, formatting, field policy and operators

## Boss authorization — 21 October 2026

Boss approved implementation of the entire 44-step Stage 10 mission, B1–B4,
and the Wrench amendments. This supersedes the proposal-only authorization
statements below; it does not claim implementation or acceptance.

- One heterogeneous print and shared display/format kernel; exact numeric
  ties-to-even rounding, checked allocation, no arbitrary width/precision cap.
- Declaring-owner field privacy and deep const paths; direct own-initializer
  replacement grant only; existing method/property privacy remains unchanged.
- Owner-contained exact operators include binary `%` and derived `%=` in addition
  to the other arithmetic, unary, equality and ordering families. Floating builtin
  remainder remains prohibited. Independently declared `!=` remains prohibited.
- Approved verified pinned Ryu/date and bundled versioned IANA rules; recursive
  custom-equality dictionary-key exclusion remains required.
- A standalone CR completes a line immediately. Pending CRLF continuation belongs
  to logical line reads; mixed raw reads retain byte ownership.
- Cursor queries require an accessible owned response channel. Redirected stdin,
  unavailable ownership or responses return a recoverable error; no hidden tty.
- Preserve Stage 9 Windows corrections, LLVM 19.1.7, ownership and GC. No Git
  mutation, hosted CI dispatch, publication or root README edit is authorized.


**Status: entire implementation authorized; engineering in progress, acceptance open.**
Research date: 21 October 2026. Source baseline:
`e25816bdb80e3f32613edf77c9cf3258232af966`, plus the pre-existing Stage 9
documentation changes described in [the evidence register](stage10/EVIDENCE.md).
The four systems below are the complete authorized **design scope**. None is
reported implemented by this document.

## Review package

| Document | Purpose |
|---|---|
| [Console I/O](stage10/CONSOLE_IO.md) | Public operations, streams, EOF, terminal behavior, platform/lifetime rules |
| [Display, formatting and interpolation](stage10/DISPLAY_FORMATTING.md) | One conversion path, exact format grammar, working temporal values, custom hooks |
| [Private and readonly fields](stage10/FIELD_ACCESS.md) | Ownership, constructor grants, capability propagation and independent IR checking |
| [Operators](stage10/OPERATORS.md) | Approved families, declaration syntax, selection, effects and dictionary boundary |
| [Acceptance plan](stage10/TEST_PLAN.md) | Focused source, Runtime, JIT/O2, linked AOT and real editor acceptance |
| [Evidence register](stage10/EVIDENCE.md) | Current source, superseding contracts, original VOID source and external references |

## Findings that determine the design

The compiler already has the expensive foundations: one typed frontend, nominal
types and shared member selection, conservative reference/effect analysis,
construction readiness, independently validated IR, one LLVM 19.1.7 lowerer,
precise GC, checked failures, immutable analysis snapshots and one language server.
Reuse those mechanisms. Adding an unrelated reflection system, ownership registry,
formatter interpreter in each editor, second stdlib resolver or second code
generator would duplicate existing authority.

The actual gaps are substantial. There is no source `print`, console stream API,
general display operation, numeric format engine, temporal value API, stored-field
access policy or source operator declaration. Current interpolation accepts only
single-line expressions whose result is string; formats and triple strings are
explicitly rejected. A `FieldType` can carry reference authority, but `ValueField`
has no declared field visibility/readonly policy. Existing arithmetic is selected
directly by operand types. These are missing systems, not fixes to a few aliases.

The original VOID source **is locally available**. Its console used shared input
lookahead, Unicode scalar decoding, standard stream wrappers and platform terminal
primitives. It also records a concrete shutdown bug caused by dispatching through
managed output objects after static-root teardown. Preserve the lessons; do not
port its C#-shaped class hierarchy or generated-C backend into VOID 2.0.

## Feature-completeness matrix

“Existing” means source/contract evidence exists, not that this research ran a
test. “Design” means required Stage 10 work. A row remains open until its acceptance
tests pass; intermediary milestones cannot close the overall stage.

| System / capability | Existing baseline | Required Stage 10 result |
|---|---|---|
| Strict UTF-8, NUL, scalar indexing, owned byte conversion | Implemented | Reuse unchanged |
| Named/default binding, left-to-right written argument evaluation | Implemented | Reuse for print controls and public library calls |
| Variadic heterogeneous print, zero/many values | Missing | One `print`, automatic display, separator/end/stream/flush controls |
| No-newline output and prompts | Missing | `end = ""`, explicit flush; predictable redirected prompts |
| stdin/stdout/stderr and byte/text access | Missing | Borrowed standard endpoints, shared input state, checked output completion |
| Character/text/line reads, EOF and failures | Missing | Distinct EOF, empty data and error; partial progress retained |
| Redirection and Windows Unicode | No console contract | UTF-8 bytes for redirects, wide console boundary, no ACP dependence |
| Terminal input and presentation | Missing | Capability queries, keys, modes, colors, cursor, size, clear, title, restoration |
| Concurrent output and normal exit | Missing | Serialized complete records, failure accounting, deterministic cleanup |
| String-only interpolation | Implemented bounded subset | All displayable types and ordinary expression forms |
| Numeric/bool conversion | Missing | All ten numeric types and bool through the shared display path |
| Numeric formatting | Missing | `0/#`, grouping/signs, precision, padding, percent, hex/binary/scientific |
| Deterministic rounding / special float values | Numeric arithmetic exists | One documented formatting rule independent of host locale/rounding mode |
| Date/time/duration values and formats | Missing | Validated values, clocks, offsets/zones and working calendar patterns |
| Custom struct display/formatting | Missing | Owner methods with checked signatures, effects and ordinary generation leases |
| Private stored fields | Missing | Declaring-type access, shared selection, construction and tooling checks |
| Readonly stored fields | Missing | `const` declaration, direct initializer grant, deep projections thereafter |
| Value/managed copies, readonly views and initialization | Implemented | Preserve C1/S1/P3/P4/RV semantics across the new features |
| Inheritance/composition/interfaces/generics | Implemented | Preserve current lookup; extend policy rather than bypassing it |
| User arithmetic/equality/order operators | Missing | Complete approved families, exact signatures and deterministic selection |
| Compound assignments | Built-in numeric/string | Reuse arithmetic call plus existing captured-place assignment |
| Dictionary hashing/equality | Implemented data-only key semantics | Preserve coherence; exclude newly custom-equality key families explicitly |
| LSP queries, source docs, completion, navigation | Implemented shared infrastructure | Extend compiler metadata for every new declaration and hole/spec context |
| Reload/cache/JIT/AOT | Implemented infrastructure | Include policies, selected calls and formatter/dependency versions in exact contracts |
| Git package manager | Future proposal | Separate; unnecessary for this stage |

## Settled requirements and four Boss decisions

Settled by Boss's Stage 10 request: all four complete systems, one automatic
display/format path, simple print, numeric and temporal formats, custom display,
existing ownership/visibility/LLVM/GC preserved, research before engineering.
Existing decisions also settle deep readonly views, value copies, module ownership,
construction publication, direct imports, exact numeric typing and dictionary
metadata without code callbacks. Stage 10 must not reopen those foundations.

The following are **recommendations, not approvals**. Approval can accept each
bundle or amend its exact rule; the detailed documents make the consequences
reviewable.

| ID | Decision genuinely needed | Recommended answer |
|---|---|---|
| B1 | Public console/format syntax and deterministic format policy | Accept the APIs and grammar in the two contracts: `print(*values, sep, end, file, flush)`, `string(value)`, recoverable `format(value, spec)`, invariant output, ties-to-even numeric rounding; no arbitrary source precision/width ceiling; explicit 64-active-custom-hook resource guard |
| B2 | Meaning of readonly fields during initialization and lexical field privacy | Use `const field: T`; permit direct own-field assignment only in that owner's initializer; preserve complete-default value initialization and identity-free managed staging. Field-private means nominal owner, while existing file-private methods/properties keep their meaning |
| B3 | Exact operator syntax, unary arithmetic and comparison pairs | Accept owner-contained `operator` declarations, binary `+ - * /`, unary `+ -`, `==` with compiler-derived `!=`, optional independent `< > <= >=`; exact operand tuple resolution and readonly operands. No separately declared `!=` |
| B4 | New dependencies, time-zone distribution and custom equality keys | Approve small pinned Ryu/date dependencies and bundled versioned IANA data; prohibit dictionary keys recursively containing a type with custom equality until a separate explicit key contract is approved. Existing eligible key programs retain behavior |

B4's key exclusion follows the already-deferred **custom-equality keys** boundary
in DECISIONS.md. It avoids silently interpreting a custom `==` as a Runtime hash
callback or accepting apparently equal keys with incompatible hashes. The complete
operator system remains usable outside dictionary key positions.

No decision asks whether to deliver only strings, only integers, only print, only
value structs or only JIT. Those are not acceptable completion points.

## Architecture and dependencies

```text
shared field policy + owner context + construction grants
    -> safe source library types and user display hooks
    -> operator declarations and exact specialization-time calls

LLVM-independent display/format kernel + owned output builder
    -> string(value), format(value,spec)
    -> typed interpolation and constant formatting
    -> print record construction

Runtime standard-stream adapters + NativeSafe transitions
    -> console reads/writes, terminal sessions and exit cleanup

validated calendar/duration values + pinned zone rules
    -> date/time/duration formatting using the same format dispatcher

all new declarations/calls/policies
    -> analysis metadata, independent IR proof, exact reload/cache contracts
    -> existing voidls and all thin editor adapters
```

Ordinary source wrappers live in the existing standard source domain. Primitive
numeric rendering, checked OS I/O and clocks are compiler-known Runtime operations,
like today's text/collection operations, **not another external native provider**.
Do not consume the single externally admitted provider slot or grant imports
arbitrary native authority. Built-in `print`, `string(value)` and `format` are
documented compiler prelude entries; their shared metadata points to matching
source documentation. `use console` and `use time` use the existing resolver.

No broad stdlib, general source varargs, general function overloading, reflection,
implicit conversion, async I/O scheduler or package installation is a prerequisite.
Compiler-recognized heterogeneous print is a bounded language operation, lowered
to concrete typed display calls rather than boxing into a new dynamic value type.

## Ordered implementation roadmap

This is one Stage 10 delivery with sequential reviewable milestones. Engineering
may consolidate adjacent steps when a shared change is smaller. Each milestone
must carry positive behavior and applicable rejection/failure coverage.

| Step | Engineering action | Dependency / completion evidence |
|---|---|---|
| 1 | Settle B1–B4 and copy approved policy into feature contracts | No production changes before Boss authorization |
| 2 | Record focused baseline commands and currently available platform tools | Existing tests, no invented pass counts or hosted runs |
| 3 | Add stored-field declaration metadata and lexical access context | Parser, owned nominal descriptor, analyzed declaration |
| 4 | Extend shared member selection with declaring-owner field access | Direct/inherited precedence and one-hop promotion preserved |
| 5 | Add declared readonly projection to shared field-type/path operations | Every reference leaf and inline place retains the right authority |
| 6 | Implement direct initializer-only const-field write grants | C1 complete defaults; S1 readiness/commit; base owner boundaries |
| 7 | Independently verify field writes and construction grants in IR | Forged metadata/paths/grants reject before LLVM |
| 8 | Complete constructors, properties, closures, interfaces and generic policy integration | FIELD_ACCESS matrix; old source behavior retained |
| 9 | Update exact compatibility, source metadata and private-field tooling | Access/policy changes require restart; inaccessible suggestions suppressed |
| 10 | Add one checked native byte builder and plain format-plan representation | Owned memory; no managed pointer retained across a poll/native boundary |
| 11 | Integrate pinned numeric conversion kernel and integer bit rendering | All integer widths, signed minima, f32/f64 defaults and bit-pattern vectors |
| 12 | Implement numeric patterns, aliases, exact scaling and one rounding step | Fixed/scientific/grouping/signs/percent/hex/binary; malformed specs reject |
| 13 | Add ordinary type-aware display and conversion APIs | Every existing storable type has a documented default or explicit invalid category |
| 14 | Add user owner display/custom-format methods and recursion protection | Existing calls/effects/leases; malformed hooks and recursive hooks diagnose |
| 15 | Share compiler constant formatting with the same kernel | Existing constant limits; no second numerical implementation |
| 16 | Extend string hole tokenization and typed AST to all expressions/specs | Original spans; dict/colon/nested string/comment/delimiter correctness |
| 17 | Complete multiline interpolation/triple string rules and escaping | All ordinary expression forms; LF/CRLF mapping and existing literal compatibility |
| 18 | Lower interpolated output through the common builder | Evaluate once, root values, checked failure cleanup, no repeated concat loop |
| 19 | Supply validated date/time/local/instant/offset/duration source values | Factory errors, private readonly components, real usable values |
| 20 | Add UTC/monotonic clock operations and checked arithmetic | No wall-clock dependence in unit tests; overflow and precision explicit |
| 21 | Integrate pinned time-zone rules and local-time resolution | Identical data across platforms; missing/ambiguous/nonexistent cases tested |
| 22 | Complete temporal patterns and defaults in the format dispatcher | Real dates/durations/zones; incompatible tokens fail precisely |
| 23 | Introduce Runtime-borrowed standard endpoint state and injectable OS adapter | Reuse Runtime lifecycle, no source-owned raw handle or new resource registry |
| 24 | Implement complete low-level reads/writes and error/progress records | Partial transfer, EINTR, zero progress, broken pipe, EOF and invalid UTF-8 |
| 25 | Implement one shared byte queue/decoder for byte, scalar, text and line reads | Mixed reads preserve lookahead; CR/LF/CRLF; no char/nullable-string dependency |
| 26 | Implement Windows wide-console and redirected UTF-8 paths | Unicode/surrogates/NUL; no code-page mutation or ANSI conversion |
| 27 | Implement public print binding and record writes | Zero/many values, keywords, output stream, no newline, formatting errors before output |
| 28 | Finish prompts, flush and exit/failure cleanup | Explicit prompt flush; no static managed callbacks after teardown |
| 29 | Add capability-based terminal presentation operations | Unsupported/redirected operations fail without emitting controls |
| 30 | Add scoped input modes, key events, available-key query and restoration | PTY/Windows console tests; no hidden `/dev/tty`/CONIN$ bypass |
| 31 | Verify concurrent record writes, blocking reads, GC and Runtime shutdown | No Managed wait on native locks; no stream lock across source formatting |
| 32 | Parse/register the complete approved operator set and invalid definitions | Owner/arity/signatures/visibility/duplicate tuples checked |
| 33 | Resolve operators in ordinary/generic expressions and report ambiguity | Exact operands; direct owners; no expected-result ranking/coercions |
| 34 | Reuse calls/effects/rooting/copies for operator lowering and derived inequality | Normal/O2 and linked AOT agree; partial stage identities never escape |
| 35 | Extend compound assignments through captured-place arithmetic | Receiver/index/getter/RHS exactly once; no commit after failure |
| 36 | Enforce custom-equality dictionary key exclusion transitively | Preserve current data-only hashing and all old eligible-key behavior |
| 37 | Integrate operator/default-format dependencies with reload and cache | Selected targets/effects/policies and dependency versions are exact inputs |
| 38 | Complete compiler-owned completion/hover/signatures/references for all systems | No evaluation of hooks or duplicate semantics in voidls/editors |
| 39 | Complete format-aware conservative source formatting and diagnostics | Preserve literal bytes and tokens; stale overlays never supply edits/results |
| 40 | Run cross-system scenarios and measured large-output/read tests | Construction + custom display + operator + GC + console + temporal formats |
| 41 | Run consolidated Linux focused/O2/AOT and strict sanitizer acceptance | Reuse existing CTest/Python/client helpers; no verification framework |
| 42 | Run locally available Windows Debug/Release and macOS platform probes | Clearly record executed vs unavailable; Boss controls any hosted matrix |
| 43 | Update contracts, examples and internal acceptance evidence | Completeness matrix has no remaining approved functionality missing |
| 44 | Return the engineering result for Wrench review and Boss release decisions | Git, hosted CI and publication remain Boss controlled |

## Risks and required handling

| Risk | Engineering requirement |
|---|---|
| Field const accidentally freezes all aliases or thaws through copies | Shared per-leaf projection and independent IR tests; preserve access-path semantics |
| Initializer exception becomes a backdoor mutable borrow | Grant bound to direct owner/init/place, never a source value, parameter or closure capability |
| Terminal operations block GC or deadlock cleanup | Owned native data, NativeSafe before wait, restoration outside managed callback teardown |
| Windows redirected output reintroduces CI Unicode conversion failure | Explicit wide console versus raw UTF-8 file/pipe paths in both Debug and Release |
| Formatter rounds a value twice or uses host locale | Bit-based kernel; exact percent scaling; one final rounding; invariant grammar |
| Format hooks recurse, allocate or fail | Ordinary failure/GC/leases; per-call active hook tracking, no source evaluation in LSP |
| Temporal formatting lacks actual values or reliable zone data | Factories/clocks/conversions and bundled versioned rules are acceptance dependencies |
| Private data leaks through display or constructor labels | Opaque nominal default display, field access checked on labels/patterns/promotion |
| Operator changes silently alter dictionary hashing or patterns | Explicit key exclusion; patterns keep existing semantics and never call custom operators |
| New declarations bypass cache/reload compatibility | Serialize policy/signature/binding/effect inputs and test old-generation ownership |
| Roadmap is marked done after one backend or scalar subset | Acceptance is per matrix row across source/JIT/O2/AOT/tooling where applicable |

No speculative optimization is proposed. Measure long output, many interpolation
holes, repeated scalar reads, shared-stream contention and editor analysis before
choosing additional buffering/caches. One output builder and chunked OS transfers
avoid evident repeated-concatenation/one-byte-syscall costs without changing the GC
or inventing an asynchronous console subsystem.

## Final engineering acceptance

All rows in the matrix must be implemented and documented. Source checking,
normal/O2 JIT and independently linked AOT must agree on values, output bytes,
side-effect order and failures. Runtime fault tests must establish partial-write,
GC, cleanup and lifetime safety. Real PTY/console tests must establish supported
terminal behavior; redirection must work without a terminal. The shared compiler
must provide correct editor behavior on dirty snapshots, and adapters must pass
the existing real-editor paths available locally.

Linux focused tests are the development gate. A full seven-job hosted run is not
required between steps, and Stage 9's remaining non-Windows acceptance does not
block this design. Missing local native-platform execution must be reported as
unverified rather than passed. Wrench reviews the completed result; Boss alone
authorizes Git changes, hosted CI and release. See [TEST_PLAN.md](stage10/TEST_PLAN.md)
for concrete expected outcomes and evidence requirements.
