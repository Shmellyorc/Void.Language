# Slice 6 numeric-backed enums and fallible casts

Golden Wrench approved the S6 language contract for local implementation on
baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`. The pre-approval
[architecture audit](evidence/slice6/preapproval-audit.txt) and
[executed baseline probes](evidence/slice6-baseline-audit.json) remain available.

## Implementation

- The existing frontend accepts `enum mode(u8):` with explicit, unique decimal
  values across all eight integer backings. Member constants, comparisons and
  exhaustive matches use exact nominal identity and the declared member set.
  Declarations reject overflow, payloads, duplicate names and duplicate
  mathematical values. Existing tagged enums retain their tag/payload semantics.
- `as?` evaluates its operand once at existing `as` precedence and association.
  Direct integer literals choose the first fitting source from `i32`, `i64`,
  `u64`. The constant evaluator uses the existing integer range routine and
  tests enum membership. Contextual `Some` and `None` use canonical
  `option<T>` storage and patterns. A visible user-declared `option` keeps
  its own identity and normal lookup precedence. The conversion always returns
  the canonical option.
- Independent IR validation checks the exact enum descriptor, source ownership,
  width, declared constants, option shape, operand categories and initialized
  inline storage at CFG joins. Managed construction retains the established
  write-before-read proof. Explicit enum and option field defaults are checked
  before a custom initializer body. The existing sum access checks active
  tags before payload projection. Zero bytes in the inactive payload of
  `None` stay internal even when zero is outside the enum's member set.
- Shared LLVM lowering compares a mathematically extended operand against
  destination bounds before narrowing, checks enum membership before
  constructing `Some`, and verifies loaded enum values. The backing integer
  determines the scalar layout. Backing and discriminants participate in
  native layout, cache and reload compatibility. This is a VOID layout
  contract. C ABI admission remains at the existing provider boundary.
- Compiler-owned completion, navigation, hover and diagnostics carry the
  exact signed or unsigned member value and source span. The existing
  session, overlay, UTF-16 and formatting paths handle the new syntax. VS Code
  adds TextMate highlighting and uses the compiler's existing semantic
  requests.

The [language contract](../NUMERIC_TYPES.md#numeric-backed-enums-and-fallible-casts)
records the tagged option design and the earlier nullable proposal it supersedes.
Enum extraction, additional fallible categories, `??`, C FFI, dependency
resolution and raylib remain separate milestones.

## Qualification

The process matrix executes 542 signed and unsigned boundary cases across
all source and destination widths, including sparse enum discriminants. It
also checks 39 malformed sources, constant and runtime results, evaluation
count, imported generics, user `option` shadowing, exact matches, aggregate
defaults, managed fields and collections. Both normal and O2 JIT execute the
matrix and the integration program. The CLI links and independently launches
an AOT executable for each program and optimization mode. The Linux cache sequence
checks cold miss, hit and a discriminant-edit miss with changed behavior. Windows
checks the existing unavailable-cache status, absence of cache publication and
changed execution after a discriminant edit.

Independent typed-IR tests reject bad descriptors, forged options, invalid
operations, undeclared constants and uninitialized enum storage. A separate
assertion observes literal operand kinds across the signed 32-bit, signed
64-bit and unsigned 64-bit boundaries. A small GC budget forces collection
during live managed option/enum use. Reload accepts a body-only edit and
rejects backing or discriminant edits without changing the live generation.
Linux tests execute the integration program with a 1 MiB stack in both JIT
modes. Native Windows processes use the platform's normal stack settings.

The raw language-server test records 15 requests and overlay revisions,
including exact UTF-16 conversion ranges, imported ownership, references,
signed hover/completion, formatting and recovery after dirty duplicates.
The VS Code 1.141.0 Linux extension host ran six project checks. Its seven
configuration tests passed, including the configured old-server path.

| Configuration | Executed |
| --- | --- |
| Linux x86-64 Release | [24/24 focused CTest cases](evidence/slice6/linux/release-ctest.txt), plus [final enum/IR replay](evidence/slice6/linux/release-final-enum.txt) |
| Linux x86-64 Debug | [24/24 focused CTest cases](evidence/slice6/linux/debug-ctest.txt), plus [final enum/IR replay](evidence/slice6/linux/debug-final-enum.txt) |
| Linux x86-64 fatal ASan/UBSan | [24/24 focused CTest cases](evidence/slice6/linux/strict-ctest.txt) with an instrumented compiler/runtime and leak detection |
| Windows 10 x64 build 19045 Release | [23/23 native CTest cases](evidence/slice6/windows-final/Release-ctest.txt) |
| Windows 10 x64 build 19045 Debug | [23/23 native CTest cases](evidence/slice6/windows-final/Debug-ctest.txt) |
| VS Code 1.141.0 on Linux | [6 live extension-host checks](evidence/slice6/editor-host.json) and [7 configuration tests](evidence/slice6/editor-config.txt) |

All qualified builds use LLVM 23.1.4. The [Linux runner](evidence/slice6/run-linux.sh)
records the command and sanitizer settings. The evidence contains JUnit output,
process and LSP payloads, exact tool versions and executable hashes. Windows
qualification used the existing provider-attested native toolchain. The replay
reused build inputs only where the captured bytes and timestamps matched,
rebuilt the changed inputs, and verified restoration of the original source.

Windows persistent native caching remains unavailable. Windows sanitizer and
extension-host runs, Windows Server 2022, macOS, other architectures and hosted
CI are outside this executed matrix.

## Failures and corrections

- Early Linux Release runs exposed cast-context inference in imported
  generics, a same-library shadowing fixture, and a missing built-in option
  resolution path in generic declarations. The first three process
  receipts preserve the inputs, statuses and diagnostics. The final
  import/generic/nominal-shadowing integration exercises these paths.
- The old numeric suite expected a lexical failure for the newly supported
  token. Its float conversion control now checks the semantic rejection.
  The malformed IR fixture now supplies a valid source name so it reaches
  the intended validator. Forced GC attaches the test thread to the
  Runtime and enters managed execution before collection. Initial failures
  remain in the [first](evidence/slice6/release-first-ctest.txt) and
  [second](evidence/slice6/release-second-ctest.txt) CTest reports.
- The managed default reproducer originally returned VOID1106. Checked
  enum and option defaults now enter the existing initializer prelude.
  The [original](evidence/slice6/linux-managed-default-original.txt) and
  [corrected](evidence/slice6/linux-managed-default-corrected.txt) Release
  command logs preserve the outcome. The final integration test covers
  both field types and independent construction validation.
- Early sanitizer qualification reused a frontend object whose compilation
  straddled the initializer edit. The original diagnostic and replay are
  retained. The corrected run forces that object to rebuild and keeps
  fatal ASan, fatal UBSan and leak detection enabled.
- The editor harness first needed Promise normalization, then exact hover
  comparison after VS Code's Markdown encoding and formatting comparison
  after composing all edits. Its original logs and final host receipt are
  preserved. The compiler's raw LSP tests also verify the exact hover and
  the formatted source.

- The first Windows 10 Release and Debug replays reported two fixture
  failures each. The standalone IR test omitted Runtime attachment for a
  forced collection. The process test assumed the POSIX cache was available
  on Windows. The corrected replay checks the native unavailability contract
  and the expected execution result after editing a discriminant. Both
  [original Release](evidence/slice6/windows-original/Release-ctest.txt) and
  [original Debug](evidence/slice6/windows-original/Debug-ctest.txt) transcripts
  retain the failure details. The replay verified all 3,258 inputs and used
  the same attested toolchain and provider checks.

## Source and Git inventory

The [exact implementation patch](evidence/slice6-source.patch) applies to the
reviewed pre-implementation working tree and covers 24 source, test and
language documentation paths. [Before and after hashes](evidence/slice6-source-hashes.json)
and the [layered inventory](evidence/slice6-implementation-inventory.txt)
identify the precise source and evidence layers. The earlier
[859-path audit inventory](evidence/slice6-inventory.txt) remains intact.
The final verification compares HEAD, index entries and earlier evidence bytes.

## End-of-task temporary workspace cleanup

The [host cleanup inventory](evidence/slice6/host-cleanup-results.json) records
13 assignment-owned launch screenshots, disposable fixtures,
scratch scripts and bytecode paths removed after preservation. The
[Windows cleanup receipt](evidence/slice6/windows-cleanup-results.json) records
the exact contents removed from both restored-overlay backup directories and
the two empty test temp directories. Windows `C:` free space went from
5,304,213,504 to 5,307,506,688 bytes (4.940 to 4.943 GiB).

The guest receipt verifies 14 retained binary hashes. The qualified build
trees, both verified native source archives and their harnesses, reusable caches,
the Linux source checkpoint and required diagnostics remain available for
review. Cleanup left unrelated files, installed toolchains, VM configuration
and Git state intact.

HEAD and the staged index match the starting state. Hosted CI remains on hold.
This handoff stops at Golden Wrench's independent S6 review.
