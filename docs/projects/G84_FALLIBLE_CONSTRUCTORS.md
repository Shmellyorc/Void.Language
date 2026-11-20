# G84 — Fallible Constructors

Candidate based on accepted G83 `cdebc9351de200feff431712f9f11b87bc9c8469`.
Independent Golden Wrench acceptance and Boss-authorized integration remain pending.
Linux verification only; Windows and macOS remain unqualified.

## Constructor contract and design

G84 extends the existing `init` mechanism. Infallible initialization remains
`init(...)` with `T(...) -> T`. A fallible initializer declares
`init(...) -> result<bool, e>`; the same ordinary `T(...)` call then produces
`result<T, e>`. Callers use existing `match`, result patterns and `?` with exact
error types. There is no separate constructor operation, keyword, hidden unwrap,
new allocator or exception facility. The initial experimental `try_init` builtin
was removed following Boss's direction; it is not part of this candidate.

```void
value struct point:
    x: i32
func (p point) init(x: i32) -> result<bool, string>:
    if x < 0:
        return Err("negative x")
    p.x = x
    return Ok(true)
func create(x: i32) -> result<point, string>:
    return Ok(point(x)?)
```

`bool` is an existing representable success payload; no general `void` type was
introduced. The `Ok` tag means completion, including `Ok(false)`. Constructor
lowering replaces that Boolean with the completed owner. The compiler rejects
other initializer result shapes rather than guessing conversions. Named/default
arguments, written evaluation order, single initializer ownership, visibility and
generic specialization remain shared with ordinary constructors. Constructor
result inference and editor signatures expose `result<T, e>`, while the initializer
declaration retains its actual `result<bool, e>` signature.

The existing [VOID initialization guide](../INITIALIZATION.md),
[managed initialization contract](../MANAGED_INITIALIZATION.md) and
[result documentation](../LANGUAGE_BIBLE.md) govern the implementation. Research
used the official [Rust Result/propagation guide](https://doc.rust-lang.org/book/ch09-02-recoverable-errors-with-result.html)
and [Swift initialization specification](https://github.com/swiftlang/swift-book/blob/main/TSPL.docc/LanguageGuide/Initialization.md):
both provide useful precedents for explicit failure handling and initialized
stored properties. VOID keeps its own result representation, exact error matching
and existing constructor syntax. No upstream implementation patch was imported.

## Safety and ownership

Value fields retain C1 preparation before the body. A fallible value initializer
and its local copies are quarantined by the existing construction-helper analysis;
the whole unfinished owner cannot escape through return/error payload, storage,
closure, interface or opaque helper. Safe value helpers preserve by-value copies
and receiver mutation. Only managed parameters acquire IR construction capabilities.
A rejected value restores private temporary storage to its checked structural
constant template, dropping field roots before the caller continues without
rerunning child initialization.

Managed constructors retain private staging and one most-derived final commit.
A fallible call leaves the stage pending until its actual result tag is checked.
Success authorizes commit; failure extracts the typed error and clears private
stage storage. Independent IR validation checks call/result provenance, success
readiness, rejected-stage disposal and checked returns through cleanup slots.
Malformed IR controls reject swapped result branches, omitted disposal, unchecked
returns and missing required writes. Runtime allocation/commit failure remains
an existing source failure, not a synthesized typed error.

Fallible base initialization requires mandatory first `super.init(...)?` in a
fallible derived initializer with the same error type. Base failure skips derived
preparation/body. Infallible bases retain ordinary `super.init(...)`. Fallible
children require explicit handled construction; implicit default construction
cannot silently discard an error. Successful assignment replaces its destination;
a failed constructor preserves the previous destination. Earlier external effects
are not rolled back.

Existing `with`/`defer` ownership handles success, early error and propagated error.
Failed acquisition activates no cleanup for an unconstructed owner. Existing
shared references retain their own ownership; construction does not automatically
close borrowed resources or call `deinit` on arbitrary incomplete stages. Precise
GC roots retain error payloads across allocations and release rejected private
fields and completed locals at their established lifetime boundaries.

## Confirmed issues corrected during implementation

- Constructor anchoring originally inferred the owner rather than the result when
  passed to generic functions. The shared constructor result resolver fixes both
  ordinary and generic calls.
- Readiness analysis omitted scoped and match bodies; it now merges their actual
  continuations. Exhaustive match lowering terminates its impossible unmatched
  edge, avoiding a spurious uninitialized join.
- Fallible value receivers could escape via `Err(self)`. The existing finite
  construction-helper worklist now also checks these value contexts, without
  changing their copy ABI. Failed value temporary roots are cleared explicitly.
- Attempting `t()` when inference selected a result type emitted an internal
  "Missing owned value type" message. It now produces a normal source diagnostic.
- Generic field completion exposed encoded specialization names. Presentation uses
  the declaring type's readable display name. Shared nominal-name shortening also
  keeps CLI diagnostics and LSP text readable without altering canonical identities.

Original unsupported-source probes, the value escape reproduction, generic editor
presentation failure and intermediate failed test receipts are preserved in the
G84 evidence directory. They are not counted as final qualification successes.

## Qualification

All selected Linux gates pass on the same frozen production sources and final
LLVM/Clang/LLD **23.1.4** artifacts. Every compiler/server/application child in
protocol/source/native qualification uses **1,048,576-byte soft and hard stack
limits**, core generation disabled. No existing nesting guard or stack requirement
was reduced. No unrelated full-suite qualification was run.

| Profile | Selected gates | G84 source records | G84 LSP records |
| --- | --- | --- | --- |
| Debug | 10/10, 61.36 s | 105/105 | 24, clean server exit |
| Release | 10/10, 29.57 s | 105/105 | 24, clean server exit |
| Fatal ASan/UBSan/leaks | 10/10 across initial run and source-only rerun | 105/105 | 24, clean server exit |

The ten gates are `fallible_initialization_source`,
`fallible_initialization_runtime`, `fallible_initialization_aot_normal`,
`fallible_initialization_aot_optimized`, `lsp_fallible_initialization`,
`managed_initialization_source`, `reference_values_source`, `inheritance_source`,
`initialization_cli` and the separately identifiable `lsp_enum_field_defaults`.
The source gate executes ten positive programs in fresh JIT/AOT O0/O2 (70
check/build/run records) and 35 expected source-diagnostic rejections. Native
JIT/AOT probes additionally cover forced GC, error payload roots, rejected value
field roots, failed final allocation, retry and malformed IR authority.
The original enum-default regression retains its 17 CLI plus 60 protocol records
per profile. Existing source, reference-value, inheritance and initialization gates
cover the infallible contracts directly affected by the implementation.

Fatal settings were exactly:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
LSAN_OPTIONS unset
```

The first sanitizer run passed nine gates; its source gate failed at an AOT link
with `clang: ... Bus error` using `/tmp` scratch. That unfavorable receipt is
preserved as `strict-first-qualified.*`. G83 already recorded the same linker
symptom and disk-backed scratch workaround. Only the failed source gate was
rerun, on `/home/shmellyorc/Documents` scratch, with identical sources/artifacts,
1 MiB stacks and fatal settings: **1/1, 51.83 s**, all 105 records passed.
The linker's underlying cause was not established; no linker fix or sanitizer
suppression is claimed. `strict-source-rerun.*` records the successful rerun.

Additional bounded checks passed in all three profiles: the original G83 24- and
48-level empty-list equality source checks and uncached JIT O2, generic
type-parameter value/managed constructors in JIT/AOT O0/O2, and the preserved full
TestGame enum-default/incomplete-expression edit sequence. Each TestGame replay
exited zero with empty server stderr. These checks preserve the original source
snapshot and do not write TestGame. The final before/after snapshot confirms 29
current project files unchanged; the earlier installation report retains its own
24-file snapshot from that earlier point in time.

Real **VS Code 1.144.0** and the provided portable **VSCodium 1.143.1** each passed
all seven maintained editor observations using the existing installed VOID
extension and the final Release candidate server: result hover, visible parameter
hints/documentation, imported definition/references, semantic tokens, actual
completion popup during an unsaved member edit, readable generic specialization
presentation, and invalid initializer annotation/repair without server death.
The test workspaces were disposable; fixture files remained unchanged on disk.
`npm run build` passed for the editor harness. No new production extension syntax
or protocol was needed for ordinary constructor/result syntax.

`qualified-source.json` freezes every `include/` and `src/` file. Each profile
receipt records those hashes, exact command/environment, artifact SHA-256 and
result. `build-identities.json` records the compiler build identities and pinned
configuration. Final compiler/server SHA-256:

| Profile | `voidc` | `voidls` |
| --- | --- | --- |
| Debug | `0014edc5c92a1fab9a28742125c4c4cd83ab68cca304133322b5a9591383ef41` | `268734882c336cdaac4af6fa11dc2a2426a76e8171cc2dd3f0bbfcdc8a913f4c` |
| Release | `2a091ed0f9335805d8de0a3132664a69a485204a67613e75bc012f031804e526` | `9eedc60588cc556f3f64b3a685542194344676922c62b8de9da9bb3d75ca8df6` |
| Fatal sanitizers | `264f1ea021b73eed6c09eb7d4688fae16f6392163d2b809d870605695b5de200` | `3485a2ff00100e1d2a766363b12bc61b1b85c25b0a52f7ef89e8f2830a0e609c` |

Evidence directory: `/tmp/g84-fu5ik560`. Review archive:
`~/Documents/Void.Language-archives/g84-fallible-constructors-2026-11-20-fu5ik560.zip`.
It contains the changed candidate, layered patches, original/intermediate probes,
final raw receipts, member hashes and replay proof. Its adjacent `.sha256` and
verification receipt identify the archive; every extracted member is SHA-256 and
CRC checked. `git diff --check` and new-file whitespace checks pass. The index is
empty and accepted G83 HEAD unchanged.

Genuine limits: only `result<bool, e>` initializer results are supported; error
conversion is explicit; implicit fallible child defaults are rejected; arbitrary
result-valued returns require conservative readiness; unfinished-owner identity
and opaque escapes remain prohibited. No general rollback, automatic incomplete
object deinitialization or new foreign constructor ABI is promised. Windows and
macOS have not been qualified; independent Golden Wrench review is still required.

## Separate TestGame crash correction

The six-file enum field-default correction predates G84 and is separately frozen
in the evidence archive. Its report is
[ENUM_FIELD_DEFAULT_RECOVERY.md](../compiler-health/ENUM_FIELD_DEFAULT_RECOVERY.md).
The original installed-server crash logs, interactive input and TestGame snapshot
remain preserved in their original archives. G84 does not replace that evidence.

The corrected G83 plus crash-fix tools are already installed at
`~/.local/bin/voidc` and `~/.local/bin/voidls`. VS Code and the provided portable
VSCodium use the updated existing VOID extension. G84 candidate binaries are
verified from the existing repository build directories; this candidate is not
installed over the stable tools. No duplicate toolchain installation was created.
A final process inspection confirms the existing TestGame server still executes
`~/.local/bin/voidls` (PID 52771) with SHA-256
`4f3ecaaa1275d3a25028114c34a7a7cfb9971a2faf6c13d742fb3cc7d37554bc`.
The installed compiler remains SHA-256
`b96fb7767a88a27be59443199cabcae68ddb4fc436415ad75326f6b26019206b`,
build identity `e717ff723f5fa880f3e917c8f3ae828e4d016a459db66202db7bb8babc3ff870`.
Both existing editor extension directories retain version 0.1.2, production bundle
SHA-256 `1be8b2a139f0fafd2f868fe839cdc2867df163f126e79e9b4647985274c72b5f`.
Their G80 `static` coloring and actual integrated-terminal PATH verification are
preserved in the earlier installation report/archive.

## Exact changed-file inventory

The candidate changes exactly **37 files** relative to the accepted G83 baseline.
The crash correction contains six files; G84 contains 33 files, with two shared
files (`CMakeLists.txt`, `src/frontend/compiler.cpp`). The four crash-only files
are unchanged from the frozen installed correction.

| File | Layer / purpose |
| --- | --- |
| `CMakeLists.txt` | Both: register focused regressions and native AOT drivers |
| `src/frontend/compiler.cpp` | Both: enum recovery, constructor typing/lowering and editor metadata |
| `docs/compiler-health/ENUM_FIELD_DEFAULT_RECOVERY.md` | Crash: original root cause, correction and installed evidence |
| `tests/lsp/enum_field_defaults.py` | Crash: original and incomplete enum-default recovery |
| `tests/lsp/fixtures/enum_field_defaults/main.void` | Crash: reproducer fixture |
| `tests/lsp/fixtures/enum_field_defaults/scene.void` | Crash: imported scene fixture |
| `include/void/ir.hpp` | G84: result predicate and construction proof operations |
| `src/ir/module.cpp` | G84: initializer signatures and operation validation |
| `src/ir/construction.inc` | G84: pending/success/failure stage and return proof |
| `src/ir/reference_capabilities.inc` | G84: checked-return reference facts |
| `src/backend/llvm/backend.cpp` | G84: result check, private-stage clearing and checked return |
| `src/frontend/construction.inc` | G84: value quarantine, scope/match readiness and helper checks |
| `src/frontend/inheritance.inc` | G84: fallible mandatory base prelude |
| `include/void/diagnostic.hpp` | G84: shared readable nominal presentation declaration |
| `src/frontend/source.cpp` | G84: readable CLI nominal diagnostics |
| `src/lsp/main.cpp` | G84: reuse shared nominal presentation |
| `docs/INITIALIZATION.md` | G84: ordinary call, value safety and cleanup contract |
| `docs/MANAGED_INITIALIZATION.md` | G84: managed result/stage/base contract |
| `docs/LANGUAGE_BIBLE.md` | G84: current constructor language contract |
| `docs/GENERICS.md` | G84: constructor result inference |
| `docs/projects/G84_FALLIBLE_CONSTRUCTORS.md` | G84: this engineering and qualification report |
| `examples/fallible_initialization/main.void` | G84: runnable value/managed success and typed errors |
| `examples/fallible_initialization/roots.void` | G84: runnable managed/value GC fixture |
| `tests/fallible_initialization/source.py` | G84: positive execution and negative source diagnostics |
| `tests/fallible_initialization/lsp.py` | G84: live protocol and generic presentation recovery |
| `tests/fallible_initialization/editor.void` | G84: real-editor call-site fixture |
| `tests/fallible_initialization/integration.void` | G84: generic errors, inferred calls and fallible inheritance |
| `tests/fallible_initialization/cleanup.void` | G84: scoped/deferred cleanup and failed acquisition |
| `tests/fallible_initialization/assignment.void` | G84: failed replacement and preserved external effects |
| `tests/fallible_initialization/value_helpers.void` | G84: private value helpers, copies and receiver mutation |
| `tests/fallible_initialization/imported/api.void` | G84: visibility, docs and generic imported constructors |
| `tests/fallible_initialization/imported/main.void` | G84: imported factory and constructor execution |
| `tests/fallible_initialization/proof.hpp` | G84: shared forced-GC/root assertions |
| `tests/fallible_initialization/tests.cpp` | G84: native JIT, commit failure and malformed IR controls |
| `tests/fallible_initialization/aot.cpp` | G84: runtime-only AOT/GC and failed-commit recovery |
| `tests/managed_initialization/tests.cpp` | G84: update wording for still-invalid non-result init |
| `editors/vscode/src/test/fallibleInitializationHost.ts` | G84: maintained real Code/Codium integration checks |

Evidence contains a complete `six-file-crash-correction.patch` against G83 and
`g84-only.patch` against that frozen correction. Their ordered replay reproduces
the final candidate bytes; the replay receipt and candidate SHA-256 manifest are
outside the Git index. No files have been staged, committed, pushed or published.
