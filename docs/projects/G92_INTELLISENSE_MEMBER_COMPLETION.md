# G92 — IntelliSense member completion completeness

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

20 November 2026. Implementation candidate for independent Golden Wrench review.
Focused readonly corrections are recorded in section 8 and await independent
re-review. Sections 5–6 preserve the original candidate's artifact/qualification
record; they are not the corrected candidate's qualification.
Accepted baseline: `b8131687cad0c4fa6b8efdbf2e591ac145f96ce1` (G90), initially clean.
No staging, commit, branch, push, installation or publication was performed.
Local qualification is distinct from independent acceptance.

## 1. Reproduction and root causes

The exact supplied scene/scene_manager example is maintained at
[`tests/lsp/fixtures/member_completion.void`](../../tests/lsp/fixtures/member_completion.void).
Before implementation, the rebuilt G90 `voidls` and the installed `voidls` both
returned `scenes` and `on_update` at `s.`, and `count`, `get`, `pop`, `push`,
`insert`, `remove_at` and `clear` at `s.scenes.`. This also held inside the original
`if`, with a missing colon, with a partial member name, and at a selection within
the complete original condition. The actual TestGame project's existing
scene_manager source also returned its receiver fields/methods and list members
through both servers. These are original observations, not after-the-fix claims.

The original reported absence was **not reproduced** with either server. It is
therefore not assigned an invented compiler root cause. Installed compiler/server
and extension bytes differed from current source artifacts despite matching
human-facing version numbers. That is an identity discrepancy, not proof that
an outdated installation caused the original report. No installed tool or
extension was replaced, and no TestGame file was saved by this work.

The investigation did reproduce these current-source defects:

- Collection completion had a separate hardcoded name/result list. It offered
  `get`, `push`, etc. without parameter signatures or documentation. Dictionary
  and list mutation calls had no analyzed builtin declarations for assistance.
- `count` was recognized by expression checking but had no analyzed declaration,
  so hover was absent at an otherwise valid `s.scenes.count` selection.
- Receiver completion rejected every source method/property when any readonly
  authority was present, including pure reads the compiler accepts.
- Completion before capability analysis could see nominal method/property result
  types without their inferred readonly profiles. A returned readonly collection
  could consequently receive mutation suggestions.
- Const inline storage lost its place permission through a nested value-field
  selection. The probe now captures that permission before unwinding its scope;
  pure reads remain visible and mutating methods are excluded.
- Source field descriptions were replaced by a generic declaring-owner sentence;
  method-specific generic completion omitted its parameter signature.

Two existing regression harnesses also asserted pre-G87 semantic legends. Their
original failures were reproduced against the untouched Release baseline and
retained. Only the expected current `comment`/`keyword` types and `inactive`
modifier were added; their behavioral assertions remain.

## 2. Implemented correction

[`src/frontend/core_members.inc`](../../src/frontend/core_members.inc) owns the
text/collection APIs' applicable receiver kinds, property/method distinctions,
mutation requirements, positional-only contracts, exact parameter/result types
and documentation. Checked calls, property binding, compiler type hints and
completion consume that same catalog. Existing runtime kernel targets, argument
evaluation, integer-index support and lowering remain in the existing compiler.
Builtin records now supply hover, signature help and semantic classification.
This adds no extension-owned collection lists or second type-resolution engine.

Struct/interface member names still pass through ordinary compiler lookup,
including visibility, original owners, generic specialization, static selection,
inheritance and composition ambiguity. Authored field documentation follows the
selected storage route. Method-specific generic headers show their signature;
uninstantiated generic methods retain conservative readonly admission.

For an effect-dependent receiver, the compiler keeps the nominally checked prefix
and the actual receiver evaluation, removes the unfinished statement and suffix,
and summarizes that missing activation conservatively. The existing reference
capability analysis computes receiver authority, returned reference profiles,
receiver storage effects and virtual-family effects. Candidate selection then
allows proved compatible methods and properties and excludes mutation through
readonly or nonaddressable inline storage. The unfinished activation is never
assumed pure. Getter/display-hook nonmutation contracts remain enforced.

No poisoned expression or parser hole enters capability analysis. This probe
always terminates as editor assistance and cannot produce executable IR.
Ordinary diagnostics are still generated from the actual buffer. Transport,
cancellation, snapshot/input checks and stale-response filtering were not changed.

The implementation follows the established separation of syntax recovery and
compiler-owned semantic completion described by
[clangd](https://clangd.llvm.org/design/code) and
[rust-analyzer](https://github.com/rust-lang/rust-analyzer/blob/master/docs/book/src/contributing/guide.md).
Those upstream designs informed the choice; they do not qualify VOID's code or
replace its pinned LLVM/Clang/LLD 23.1.4 toolchain.

## 3. Before and after

| Selection | Observed G90 | G92 candidate |
| --- | --- | --- |
| Receiver `s.` | `scenes`, `on_update` already present | Preserved, with authored field docs where available |
| `s.scenes.` | Seven applicable list members already present | Preserved, with typed signatures and docs |
| List `get` | `result<scene, string>` only | `get(index: i32) -> result<scene, string>` |
| List `push` | `no result`, no docs | `push(value: scene) -> no result`, append docs |
| Valid `.count` hover | Absent | `count: i64`, readonly-property docs |
| Const `node.` | Stored fields; pure methods/properties omitted | Pure `read`, `view`, `size` included; `change` omitted |
| Const `node.view().` | Nominal result could offer list mutations | Exact returned authority; `count`/`get`, no mutation |
| Returned inline value | Could suggest a mutating method without usable storage | Pure methods/fields; mutating method omitted |
| Fixed array | `count`/`get` | Preserved; length-changing operations absent |
| Nullable receiver | No valid member authority | Preserved until explicit unwrap |

## 4. Real editor verification

The maintained native test is
[`tests/lsp/member_completion_editor.cjs`](../../tests/lsp/member_completion_editor.cjs).
It uses actual VS Code/VSCodium extension hosts, real completion-provider commands,
rendered suggestion rows and editor `type` commands. Word-based suggestions are
disabled. Each launch uses a fresh isolated profile and explicit matching local
compiler/server paths. Existing installations, user settings and saved sources
are untouched. Both editors are checked sequentially to avoid focus interference.

VS Code 1.144.0 and VSCodium 1.143.1 passed receiver and chain popups,
partial-name filtering, missing-colon recovery, automatic dot triggers, incomplete
semantic tokens, repair diagnostics, hover and definition. The same minimal
fixture is also exercised as an unsaved buffer in the real TestGame manifest
workspace. The harness restores the original buffer and verifies unchanged disk
bytes. Editor JSON results and protocol traces remain in the external evidence
directory below.

Original harness mistakes are retained in that evidence: an ambiguous substring
selected the final `s.` inside `scenes.`, and a standalone expression was initially
given an orphan indented `return`. Those failures are test-input defects, not
evidence of missing receiver completion. The first simultaneous-editor launch
also failed a rendered-popup wait; sequential, explicitly focused hosts passed.
The baseline VSCodium probe reached all completion checks and then failed the new
`.count` hover assertion, retaining direct before-correction evidence.

## 5. Original candidate source and artifact identities

Evidence root: `/home/shmellyorc/Documents/Void.Language-G92-evidence/`.
`baseline-lsp.json`, `baseline-conditions.json`, `baseline-testgame.json` and
the original editor result/profile directories retain the before observations.
Final source/test hashes, complete compiler build identities, binary SHA-256s,
editor JSON/PNG hashes, flags, installed-tool identities and checks are recorded
in `identities-and-results.json`. `candidate.patch` and `candidate-source/` contain
the exact 13-file review candidate without staging it.

| Final profile | Compiler SHA-256 | `voidls` SHA-256 |
| --- | --- | --- |
| Debug | `4ac2bab6aa49c9c09ad8f90518305f1aa6fbba6d1e861c2e6d5d35ae5faec0f9` | `20276dfa6f06815c27d529885009a3efa2963b26396a419e972de45c1a116038` |
| Release | `27178133f334c97ffed7c5bfe19e2655dd5596abad8bd687b1de7c0c15e47448` | `5c69cb05afc2d384f29fbd32172a061e39fb8a92777d39c0fec4e34bc2f8c179` |
| Fatal ASan/UBSan | `0d9862ea7659208a6bc014e117d97eebbecba90ba6a594cc0740758b78b0b074` | `7f26abcafccc8ffdae5e76022c167c8368c1b4ab71d7d293cd3fbc89a5e626a4` |

All three report LLVM 23.1.4. The SDK's Clang, `llvm-config` and LLD also report
23.1.4. Final implementation SHA-256s are
`1e5bf7f0710e32b493c8785089614b67f645b99f91b20a14f3908c7b1df16f4a`
for `compiler.cpp` and
`708bdc4d027cdece9af694d6b62cf8f9d41e344b683a360abb37261e0daef66e`
for `core_members.inc`. The installed server/extensions were checked again and
remain byte-identical to their initial observations.

Initial source Debug server SHA-256:
`293de28326e8cd459baff12260cea84437bc878ebf45ace5319efe977bcba639`.
Installed server:
`4f3ecaaa1275d3a25028114c34a7a7cfb9971a2faf6c13d742fb3cc7d37554bc`.
Initial source compiler build identity:
`a914e3ffadbfa33dd8076946cc079395874a440d04280b4e45e35fba86f6a9ad`;
installed compiler:
`e717ff723f5fa880f3e917c8f3ae828e4d016a459db66202db7bb8babc3ff870`.
Both report LLVM 23.1.4. The source and installed extensions both report 0.1.2,
but source `dist/extension.js` SHA-256 was
`5336240bfc56853cf1c05c010f292553633b26cdaa79859074c3f7406a8882a4`,
versus installed VS Code and VSCodium
`1be8b2a139f0fafd2f868fe839cdc2867df163f126e79e9b4647985274c72b5f`.
Extension source was not changed.

## 6. Original candidate qualification and limits

Qualification uses the existing `build/llvm23-debug`, `build/llvm23-release` and
`build/llvm23-strict` configurations and the verified local 23.1.4 SDK/providers.
The focused selector covers member completion (ordinary and 1 MiB stack), G76
freshness/cancellation, editor recovery, semantic tokens, static/nullable/enum
assistance, generic hints, documentation, Stage 8, text encoding and const recovery.
It also includes the unchanged deep semantic-chain regression.

| Final local qualification | Result | Preserved log under `logs/` |
| --- | --- | --- |
| Linux x64 Debug | **15/15 passed** | `g92-final-storage-debug-tests.log` |
| Linux x64 Release | **15/15 passed** | `g92-final-storage-release-tests.log` |
| Linux x64 fatal ASan/UBSan, leak detection | **14/14 passed**, no sanitizer finding | `g92-frozen-strict-tests.log` |
| VS Code 1.144.0, real TestGame workspace | **Passed** | `g92-review-vscode.log` |
| VSCodium 1.143.1, real TestGame workspace | **Passed** | `g92-review-vscodium.log` |

The final source/test freeze, exact changed-file inventory, report relative links,
empty staged diff and `git diff --check` were verified. No full-suite run or hosted
acceptance is claimed. Final editor evidence uses the exact Debug binary above.

Replay from the source root with the matching configured builds:

```sh
selection='^(lsp_(intellisense|member_completion|member_completion_stack|semantic_tokens|editor_recovery|static_members|nullable_completeness|enum_recovery|enum_completeness|documentation|stage8|text_encoding|const_recovery_stack)|generics_hint_lsp|text_encoding_semantic_depth)$'
cmake --build build/llvm23-debug --target voidls voidc -j 2
ctest --test-dir build/llvm23-debug -R "$selection" --output-on-failure -j 2
cmake --build build/llvm23-release --target voidls voidc -j 2
ctest --test-dir build/llvm23-release -R "$selection" --output-on-failure -j 2
cmake --build build/llvm23-strict --target voidls voidc -j 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1 \
ctest --test-dir build/llvm23-strict -R "$selection" --output-on-failure -j 2
```

The strict configuration omits the pre-existing `lsp_const_recovery_stack`
registration. Its G92 stack matrix and deep semantic-chain test still run at
the unchanged 1 MiB limit.

For either real editor, run the maintained harness with a native Node 24 runtime:

```sh
export VOID_G92_EDITOR_OUTPUT=/absolute/fresh/evidence-directory
export VOIDLS_PATH="$PWD/build/llvm23-debug/voidls"
export VOIDC_PATH="$PWD/build/llvm23-debug/voidc"
export VSCODE_EXECUTABLE_PATH=/absolute/path/to/code-or-codium
node -e 'require("./tests/lsp/member_completion_editor.cjs").launch().catch(e=>{console.error(e);process.exit(1)})'
```

Optional `VOID_G92_EXISTING_PROJECT` and `VOID_G92_EXISTING_DOCUMENT` select an
existing workspace/document for unsaved-buffer verification. Without them the
harness creates its own standalone fixture. The final actual-workspace runs used
`/home/shmellyorc/Documents/TestGame` and `src/scenes/scene_manager.void`, with
`review-vscode/` and `review-vscodium/` results and real popup PNG captures.

The new matrix uses ordinary `voidc check` acceptance/refusal as an independent
oracle for supported and inaccessible accesses. It covers the exact supplied
example, nested value/struct/collection chains, method/property results, imported
and locked-package generic owners, field documentation, private fields, readonly methods and list
results, temporary value receivers, fixed/list/dictionary/string APIs, nullable
and invalid chains, static/instance separation, inheritance/composition, enum
members, rapid unsynchronized edits and diagnostic/semantic-token recovery.
It also cold-opens incomplete source without a prior successful snapshot and
executes shared list/dictionary/fixed-array/text operations through JIT at O0/O2
and a newly linked optimized AOT executable, including the 1 MiB invocation.

The unchanged `text_encoding_semantic_depth` regression found a candidate
stack overflow at its 400-method recovery case in all three profiles. The new
type-hint branch constructed a `Function`-sized signature temporary in every
recursive method/field frame. Moving signature construction into the dedicated
non-inlined `core_member_anchor` helper restored the original stack boundary;
no stack or source-depth limit was increased. The original Debug/Release exits,
fatal ASan stack, reconstructed failing source, exact input and failing artifact
identities are retained under `failures/` in the evidence root. A later nested
const-storage counterexample and its original result are retained there too.
An intermediate sanitizer compilation had read older source and completed after
a source edit; a subsequent no-work rebuild therefore retained stale object
bytes. That intermediate result is not used as current-source qualification.
The specific generated object was invalidated and rebuilt from the recorded
`frozen-inputs.json` bytes, with those hashes checked again after qualification.

An unknown receiver, invalid import/project admission, poisoned declaration or
unproved effect context may conservatively withhold assistance. Uninstantiated
method-specific generic bodies do not acquire a fabricated readonly contract.
No universal latency or arbitrary-malformed-source guarantee is claimed. No
Windows/macOS execution was available; their native editor and stack execution
remain **UNQUALIFIED**. Independent Golden Wrench acceptance is pending.

## 7. Exact changed-file inventory

- `CMakeLists.txt` — focused completion and 1 MiB stack registrations.
- `src/frontend/compiler.cpp` — shared builtin binding/hints, effect-aware receiver
  probes, ordinary member filtering, field docs and generic signatures.
- `src/frontend/core_members.inc` — compiler-owned text/collection catalog.
- `tests/lsp/fixtures/member_completion.void` — exact original reproduction.
- `tests/lsp/member_completion.py` — LSP/compiler regression matrix.
- `tests/lsp/readonly_completion.py` — focused readonly alias/caller, imported
  generic, display-argument ownership and incomplete-recovery regressions.
- `tests/lsp/member_completion_editor.cjs` — actual isolated editor verification.
- `tests/lsp/semantic_tokens.py` — current semantic legend expectation.
- `tests/lsp/enum_recovery.py` — current semantic modifier expectation.
- `docs/EDITOR_INTEGRATION.md` — member assistance and artifact identity guidance.
- `docs/LANGUAGE_SERVER.md` — compiler-owned member/effect probe architecture.
- `docs/DEVELOPMENT_STRATEGY.md` — candidate status, pending independent review.
- `docs/README.md` — report navigation.
- `docs/projects/G92_INTELLISENSE_MEMBER_COMPLETION.md` — this report.

**STRONG BOLTS. NO GHOST BOLTS.**

## 8. Focused readonly corrections after independent review

Golden Wrench returned **CORRECTION REQUIRED** for two P2 findings. The exact
review evidence from `/tmp/g92-wrench-review-zrt1t0d_/` is preserved, unchanged,
under `readonly-corrections/independent-originals/` in the evidence root. The
original 13-file candidate patch, identities and relevant source bytes remain
under `readonly-corrections/before-*` and `readonly-corrections/before/`.

### Confirmed defects and correction

1. A nominally mutable local such as `var alias = freeze(n)` did not itself
   contain a call or readonly token. Completion could therefore publish nominal
   members before capability analysis inferred the binding's readonly authority.
   The compiler correctly rejected `alias.change()` with `VOID2007`, but completion
   offered it. All reference-bearing receivers now pass through the existing
   capability analysis, including authority propagated through earlier bindings,
   generic calls and imported owners. Mutable controls keep their mutation
   suggestions; legitimate readonly reads remain available.
2. The opaque activation summary assigned mutation, retention into mutable
   storage and mutable-authority requirements to readonly formal paths. A valid
   const caller could then invalidate the probe and suppress every suggestion.
   The summary now respects `ir::readonly_paths` for each formal leaf. Those
   leaves still contribute returned-alias provenance, but the unknown suffix
   cannot acquire mutation authority forbidden by its declaration.

These changes affect editor probes only. The ordinary compiler's capability
analysis and its rejection semantics were not relaxed.

The expanded path also exercised a borrowed-pointer lifetime defect in the
original probe. Display argument checking can temporarily own an expression;
unwinding destroyed that expression before the probe used its receiver pointer.
The existing nested editor-recovery regression exposed a native crash. The
receiver is now owned by the probe exception before unwinding, and prefix
retention identifies the owning checked statement rather than dereferencing the
old expression. No display evaluation or executable lowering rule was changed.
The failing input, framed LSP request, Debug crash observations, GDB trace and
pre-correction compiler source are preserved under
`readonly-corrections/nested-recovery/`.

### Focused regression and editor coverage

`tests/lsp/readonly_completion.py` compares actual `voidls` completions with
ordinary compiler acceptance/refusal. It covers the original two review cases,
transitive aliases, generic and imported owners, generic value wrappers, collection
bindings, direct/chained selections, pure methods/properties, const and mutable
callers, display arguments and nested blocks, rapid incomplete edits and repair.
Readonly mutation remains a `VOID2007` error; the valid read programs still check.
The original independent JSON sources are also replayed byte-for-byte in each
profile by `readonly-corrections/replay_originals.py` at 1 MiB.

The native editor harness retains the original TestGame, signature/metadata,
automatic dot-popup and diagnostic-recovery checks and adds rendered readonly
alias, chained collection and const/mutable caller checks. Each editor uses a
fresh isolated profile and explicit source-built tools. Saved TestGame bytes and
installed tools/extensions remain untouched. An initial VS Code automatic-popup
timeout is retained in `readonly-corrections/vscode/`; subsequent evidence uses
the final harness and fixed source.

### Corrected candidate qualification and identities

Correction evidence is under
`/home/shmellyorc/Documents/Void.Language-G92-evidence/readonly-corrections/`.
Implementation and test hashes are frozen in `frozen-inputs.json` before the
Release/sanitizer builds, with no source edits during those builds. This avoids
the stale-object condition recorded for the original candidate.

| Corrected local qualification | Result | Evidence under `readonly-corrections/` |
| --- | --- | --- |
| Linux x64 Debug | **8/8 passed** | `debug-ownership-checks.log` |
| Linux x64 Release | **8/8 passed** | `release-checks.log` |
| Fatal ASan/UBSan, leak detection | **8/8 passed**, no sanitizer finding | `strict-checks.log` |
| Exact independent reproductions, each profile at 1 MiB | **Passed**; mutation still rejected with `VOID2007` | `original-replay-{debug,release,strict}.json` |
| VS Code 1.144.0 / VSCodium 1.143.1, actual TestGame workspace | **Passed**, four rendered readonly cases each, disk unchanged | `{vscode,vscodium}-final/results.json` |

Debug’s readonly pair was also rerun after the last focused fixture additions:
**2/2 passed** (`debug-final-readonly-checks.log`). The focused readonly and G92
member stack variants and unchanged deep semantic-chain test retain **1 MiB**.
Other existing protocol/recovery registrations keep their normal launch settings.

The focused selector is:

```sh
selection='^(lsp_(intellisense|member_completion|member_completion_stack|readonly_completion|readonly_completion_stack|semantic_tokens|editor_recovery)|text_encoding_semantic_depth)$'
ctest --test-dir build/llvm23-debug -R "$selection" --output-on-failure -j 2
ctest --test-dir build/llvm23-release -R "$selection" --output-on-failure -j 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1 \
ctest --test-dir build/llvm23-strict -R "$selection" --output-on-failure -j 2
```

| Corrected profile | Compiler SHA-256 | `voidls` SHA-256 |
| --- | --- | --- |
| debug | `81423a09af9c365952875a953100ca5c1cf7e6def5e4890094ea17142f72fca7` | `cac9f59a64a33518e3a6dfa3a3a5752f8893c025fd8f6d141da02021de0d361c` |
| release | `a0f90818debc06813ef40d2222c31c3be81a455e23929b67a52813943adb07ad` | `7e2cc89a64110edf1938e387a23a24b904f78625d1343619c9592a4424f2fc44` |
| strict | `34ff09debeddf4b41f88f8125230f196eaf73ec2c95ff2cd2ceb33987a25198f` | `72b3a6afe32ee183a5fa6255d638dc10a1291141bcccfdabd95246cbbf6c535b` |

All corrected compilers report LLVM 23.1.4. The corrected `compiler.cpp` SHA-256
is `303f73416a673fae02958f64caece6dd6a916776a8917eaeed98795ba076f6f7`.
`identities-and-results.json` records complete build identities, flags, source/test
hashes, editor identities and the unchanged installed-artifact checks.

The full corrected baseline patch is `readonly-corrections/candidate.patch`;
`corrections-only.patch` isolates the changes since the reviewed G92 candidate.
`candidate-source/` contains the exact current files. The original parent
`candidate.patch`, source snapshot and identity records remain historical evidence.

The exact current inventory is the **14 files** listed in section 7, adding only
`tests/lsp/readonly_completion.py` to the original G92 inventory. No staging,
commit, branch, push, installation, publication or hosted acceptance is authorized
or claimed. Windows/macOS remain **UNQUALIFIED**. Independent focused re-review
is pending.
