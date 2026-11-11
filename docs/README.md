# VOID documentation

Current operations use **LLVM/Clang/LLD 23.1.4 exclusively**. Start with the
[toolchain requirements](SOURCE_COMPILER.md#active-toolchain). Earlier-version commands and dated
holds in historical records do not govern current work.

## Canonical documentation

- [Install from source](install/README.md), [first program](FIRST_PROGRAM.md),
  [CLI](CLI.md), [compiler/build architecture](SOURCE_COMPILER.md),
  [editor setup](EDITOR_INTEGRATION.md) and [troubleshooting](TROUBLESHOOTING.md).
- [Language design](LANGUAGE_BIBLE.md), [numeric types](NUMERIC_TYPES.md),
  [modules](MODULE_SYSTEM.md), [strings](STRINGS.md), [value semantics](VALUE_SEMANTICS.md),
  [value structs](VALUE_STRUCTS.md), [reference/value structs](REFERENCE_VALUE_STRUCTS.md),
  [managed structs](MANAGED_STRUCTS.md), [initialization](INITIALIZATION.md),
  [managed initialization](MANAGED_INITIALIZATION.md), [composition](COMPOSITION.md),
  [inheritance](INHERITANCE.md), [receiver methods](RECEIVER_METHODS.md), [properties](PROPERTIES.md).
- [Fixed arrays](FIXED_ARRAYS.md), [growable arrays](GROWABLE_ARRAYS.md),
  [array operations](ARRAY_OPERATIONS.md), [dictionaries](DICTIONARIES.md),
  [generics](GENERICS.md), [interfaces](INTERFACES.md), [advanced language](ADVANCED_LANGUAGE.md).
- [Async/Tasks](ASYNC.md), [native libraries](NATIVE_LIBRARIES.md),
  [GC Runtime](GC_RUNTIME.md), [runtime globals](RUNTIME_GLOBALS.md),
  [cache policy](compiler-health/stabilization/CACHE.md), [diagnostics](DIAGNOSTICS.md),
  [analysis compatibility](ANALYSIS_COMPATIBILITY.md), [language server](LANGUAGE_SERVER.md).
- Standard library: [binary](binary/CONTRACT.md), [memory](memory/CONTRACT.md),
  [I/O](io/API.md), [text/encoding](text-encoding/API.md), [random](random/API.md),
  [math](math/API.md), [JSON](json/API.md), [channel](channel/API.md),
  [OS](os/API.md), [collections](collections/API.md), [logging](log/API.md),
  [HTTP](net/http/README.md).

## Engineering documentation

- [Archive and retention policy](projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)
  governs qualification outputs and cleanup for every milestone.
- [Windows x64 D5 application packaging](projects/W2_WINDOWS_D5_IMPLEMENTATION.md)
  records the package contract and the
  [accepted Windows Server 2022 Debug and Release qualification](projects/W2_WINDOWS_D5_IMPLEMENTATION.md#hosted-acceptance).
- [Slice 2 project discovery and entry selection](projects/SLICE2.md): compiler-owned
  manifest/VFS admission, immutable D/E snapshots, any-name main, library checking,
  explicit overrides, actual execution tests and independent-review boundary.
- [Slice 1 project manifest implementation](projects/SLICE1.md): schema-only
  compiler API, validation/diagnostics, executed local qualification and explicit
  deferred integration/platform limits.
- [First-class projects and C interoperability roadmap](PROJECT_ARCHITECTURE_ROADMAP.md):
  Phase 1 schema/identity/discovery/metadata/creation contract proposal, source
  audit, compatibility matrix, 42 acceptance cases and six implementation slices.
  Local Slice 2 implementation/review boundary; later enum/FFI/raylib/package
  direction and the single final hosted acceptance gate are retained.
- [Current status/strategy](DEVELOPMENT_STRATEGY.md), [Wrench instructions](WRENCH_CLANKER.md),
  [Research role](RESEARCH_CLANKER.md), [known issues](KNOWN_ISSUES.md).
- VOIDUP: [Phase A](voidup/REPORT.md), [Phase B](voidup/PHASE_B_REPORT.md),
  [integration](voidup/INTEGRATION_REPORT.md), [LLVM CI repair](voidup/INTEGRATION_REPORT.md#llvm-2314-ci-repair).
- [HTTP current engineering](net/http/CURRENT_ENGINEERING.md),
  [TCP/UDP architecture handoff](net/TCP_UDP_HANDOFF.md),
  [compiler-health issues](compiler-health/ISSUES.md),
  [integrated performance qualification](compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md).
- [LLVM compatibility](llvm23/COMPATIBILITY.md), [preview blockers](release/PREVIEW_BLOCKERS.md),
  [redistribution/licensing](release/REDISTRIBUTION_AUDIT.md),
  [documentation consolidation report](voidup/INTEGRATION_REPORT.md#documentation-review-revision).

## Historical records

[Work log](WORK_LOG.md), [decisions](DECISIONS.md),
[approval ledger](LANGUAGE_COMPLETENESS_APPROVALS.md), module implementation reports
and Stage 7–11 acceptance records retain original decisions, CI links and failures.
Original proposal bodies remain at their existing `*_PROPOSAL.md` paths, labeled
historical; API guides above define delivered behavior.

The earlier index, Wrench, Research and strategy checkpoints already exist at
immutable commit `82e97f541fab7eec760083383569ab9c9ce8f355`:
[chronology](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/README.md),
[Wrench/workshop](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/WRENCH_CLANKER.md),
[Research](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/RESEARCH_CLANKER.md),
[strategy](https://github.com/Shmellyorc/Void.Language/blob/82e97f541fab7eec760083383569ab9c9ce8f355/docs/DEVELOPMENT_STRATEGY.md).
The same originals are available offline with `git show <commit>:docs/<file>`.
Their old toolchain recipes and pending-stage holds are historical, not current
instructions. No second set of full checkpoint copies is maintained.

## Clanker entry point and permanent documentation policy

Only implement the scope Boss authorizes. Keep engineering instructions and status
under `docs/`, use this index for navigation, and update the relevant canonical
contract or implementation report. Preserve unique approvals and evidence when
consolidating; link to history rather than copying old status into every guide.
The root README remains protected and requires separate explicit authorization.
Boss controls Git and hosted acceptance. See [Wrench](WRENCH_CLANKER.md) for the
shared workflow; old sprint gates do not create fresh authorization requirements.
