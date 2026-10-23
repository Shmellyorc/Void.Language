# Stage 10 evidence and audit register

Research date: 21 October 2026. Inspected source HEAD:
`e25816bdb80e3f32613edf77c9cf3258232af966`.
**Research/documentation only; no compiler test run, production edit or hosted CI
execution was performed for this design.** [Main roadmap](../STAGE10_DESIGN.md).

## Evidence authority and historical corrections

The worktree already contained Stage 9 documentation changes and CI #95 artifacts
when Stage 10 research began. They are not Stage 10 engineering results and were
preserved. Latest WORK_LOG/WRENCH entries report CI #95 Windows Debug and Release
160/160 each, resolving the previous Windows investigation. The full seven-job
matrix and actual second-volume transfer are not certified by those records.
Earlier KNOWN_ISSUES, role docs and phase guides contain historical “not authorized”
and “not implemented” statements superseded by later decisions and source.

For implementation facts use current source plus the latest feature contract.
For language authority use Boss's current request, DECISIONS.md and explicitly
approved Bible passages. A historical proposal/example is neither an implemented
API nor automatic approval of every detailed Stage 10 recommendation. No missing
Stage 10 feature is claimed fixed by historical CI counts.

## Contract cross-check inventory

| Contracts reviewed / cross-checked | Binding consequence for Stage 10 |
|---|---|
| [docs index](../README.md), [Wrench](../WRENCH_CLANKER.md), [Research](../RESEARCH_CLANKER.md), [strategy](../DEVELOPMENT_STRATEGY.md), latest [work log](../WORK_LOG.md) | Research/engineering/review/authorization roles; Linux focused development; Boss controls Git/hosted CI |
| [Bible](../LANGUAGE_BIBLE.md) §§5,8–9,11–13,16–17,52,66 and status addenda; [decisions](../DECISIONS.md) | Const access paths, automatic formats/print, declaring-struct field privacy, owner-contained arithmetic/equality/order direction |
| [source compiler](../SOURCE_COMPILER.md), [numeric](../NUMERIC_TYPES.md), [value semantics](../VALUE_SEMANTICS.md), [diagnostics](../DIAGNOSTICS.md) | Same frontend/IR; exact numeric types; constant/reload rules; original immutable diagnostic spans |
| [value structs](../VALUE_STRUCTS.md), [receiver methods](../RECEIVER_METHODS.md), [initialization](../INITIALIZATION.md) | Independent copies, inferred method effects, C1 complete fields/defaults before value init |
| [managed structs](../MANAGED_STRUCTS.md), [managed init](../MANAGED_INITIALIZATION.md), [reference values](../REFERENCE_VALUE_STRUCTS.md) | P3 deep views, P4 complete publication, S1 identity-free stages, RV per-reference-leaf authority |
| [composition](../COMPOSITION.md), [inheritance](../INHERITANCE.md), [interfaces](../INTERFACES.md), [generics](../GENERICS.md) | One-hop paths, original owners, managed-only inheritance, exact structural method conformance, specialization-time bodies |
| [properties](../PROPERTIES.md), [advanced language](../ADVANCED_LANGUAGE.md) | Existing accessor/closure/event/result/pattern/resource semantics; do not change private method policy or pattern equality accidentally |
| [strings](../STRINGS.md), [Stage 8 text/collections](../STAGE8_TEXT_COLLECTIONS.md), [ergonomics](../STAGE8_ERGONOMICS.md) | UTF-8/scalars/NUL; existing result signatures; actual string-only interpolation subset, not general formatting |
| [fixed arrays](../FIXED_ARRAYS.md), [growable arrays](../GROWABLE_ARRAYS.md), [array operations](../ARRAY_OPERATIONS.md), [dictionaries](../DICTIONARIES.md) | Value children, versioned mutation/iteration, ordered keys, data-only equality/hash; custom-equality keys remain outside current eligibility |
| [array architecture](../ARRAYS_PROPOSAL.md) current amendments, [GC](../GC_RUNTIME.md) | No arbitrary replacement array/output ceiling; existing coordinated heap, roots, staged publication and NativeSafe |
| [async](../ASYNC.md), [native libraries](../NATIVE_LIBRARIES.md), [application/native](../STAGE8_APPLICATION_NATIVE.md), [frame retention](../STAGE8_ASYNC_RETENTION.md) | No source worker/native permission expansion, no task resource transfer, deterministic drain/cleanup and code leases |
| [module system](../MODULE_SYSTEM.md), [discovery](../STAGE8_DISCOVERY.md), [local libraries](../LOCAL_LIBRARIES.md), [calls/numeric](../STAGE8_CALLS_NUMERIC.md) | Existing VFS/std domain and metadata discovery, direct imports, exact-case/security, ordinary binder/evaluation order |
| [analysis compatibility](../ANALYSIS_COMPATIBILITY.md), [tooling index](../TOOLING_FOUNDATIONS.md), [sessions](../TOOLING_SESSIONS.md) | Exact contracts, owning records, revisions, overlays, no stale snapshots or guessed reload approval |
| [Stage 7 implementation](../STAGE7_IMPLEMENTATION.md), [language server](../LANGUAGE_SERVER.md), [editors](../EDITOR_INTEGRATION.md) | Shared current capabilities supersede minimal historical tables; UTF-16, guarded rename and conservative formatting |
| [cache proposal/current contract](../STAGE5_CACHE_PROPOSAL.md), [Stage 9 workflow](../STAGE9_APPLICATION_WORKFLOW.md) | Existing cache identity, shared JIT/AOT, current CLI native build and atomic publication remain intact |
| [known issues](../KNOWN_ISSUES.md), [completeness approvals](../LANGUAGE_COMPLETENESS_APPROVALS.md) | Distinguish genuine missing features from outdated exclusions and separately scoped proposals |

Earlier `*_PROPOSAL.md` documents are historical rationale where corresponding
implemented contracts supersede them; no claim that their entire future scope is
implemented or reauthorized. Stage 10 does not add package management, new GC,
external managed ABI, implicit coercions or a general standard library.

## Current source findings

Links name actual symbols/regions, so reviewers can locate them even if later
engineering moves line numbers. The hash appendix records the inspected bytes.

| Source / anchor | Verified finding | Reuse / necessary extension |
|---|---|---|
| [ir.hpp](../../include/void/ir.hpp), `Type`, `FieldType`, `ValueField`, `ValueType` | Existing readonly paths, nominal ownership/arena, managed/interface/collection types; ValueField only name/type/owner/using/event | Add declared stored-field visibility/const metadata, retain shared type model |
| [compiler.cpp](../../src/frontend/compiler.cpp), struct parser around 1020 | Parses stored defaults/using/event; no private/const field declaration or operator body parsing | Small header extensions, ordinary statements/functions underneath |
| [compiler.cpp](../../src/frontend/compiler.cpp), `unary_expression`, `binary_expression` around 2435 | Direct builtin numeric/string/equality handling; no user operator candidate selection | Resolve exact owner operators to normal calls before builtin fallback |
| [compiler.cpp](../../src/frontend/compiler.cpp), `argument_mapping`, builtin numeric/text handling | Existing shared binder and compiler-owned prelude/intrinsics | Reuse metadata/binding; add bounded heterogeneous print without general varargs |
| [strings.inc](../../src/frontend/strings.inc), `string_token` | Single-line substitutions, original offsets, doubled braces, strict escapes; explicit rejection of triples; lexer nesting 64 | Extend hole/spec token states rather than reparsing decoded output |
| [capabilities.inc](../../src/frontend/capabilities.inc), `analyze_capabilities` | Fixed-point reference effects, authority origins, call graph/native-unsafe classification | Add declared const projections and ordinary hook/operator calls |
| [construction.inc](../../src/frontend/construction.inc), [inheritance.inc](../../src/frontend/inheritance.inc) | Current readiness/owner/phase mechanisms and physical ancestry | Scope init grants to true original owner/place; no incomplete identity |
| [module.cpp](../../src/ir/module.cpp), `field_type` around 252, `dictionary_key_supported` around 29 | Shared field projection and recursive key eligibility | Central field policy and custom-equality key rejection |
| [interfaces.inc](../../src/ir/interfaces.inc), `select_member` around 11 | Shared real-member/one-hop selection; scope currently module-based | Add lexical nominal-owner context, preserve precedence/ambiguity |
| [reference_capabilities.inc](../../src/ir/reference_capabilities.inc), [IR construction](../../src/ir/construction.inc) | Independent IR initialization/capability proofs | Reject forged grants/readonly stores and new call effects independently |
| [numeric.cpp](../../src/numeric.cpp) | Compiler uses LLVM APFloat/APInt for current numeric semantics, nearest ties-even | Keep compiler semantics; do not link LLVM into a Runtime formatter |
| [runtime_abi.hpp](../../include/void/runtime_abi.hpp), [abi.cpp](../../src/runtime/abi.cpp) | Private same-build string/collection/root/failure ABI, noexcept wrappers | Add formatting/I/O/clock primitives through same checked boundary |
| [coordination.cpp](../../src/runtime/coordination.cpp), [native_binding.cpp](../../src/runtime/native_binding.cpp) | Existing Managed/NativeSafe coordinator and deterministic resource lifecycle | Copy bytes before waits; reuse cleanup/admission; no unmanaged GC payload pointers |
| [backend.cpp](../../src/backend/llvm/backend.cpp), string op lowering around 1152–1190, Runtime binding around 1467 | One lowerer and explicit JIT symbol binding for Runtime operations | Share new lowerings with AOT, lower user hooks/operators to ordinary calls |
| [frontend.hpp](../../include/void/frontend.hpp), [tooling.cpp](../../src/frontend/tooling.cpp), [tooling.hpp](../../include/void/tooling.hpp) | Owning declarations/references/docs, editor assistance and format_source | Add owner/field/operator/spec records and typed completion context |
| [lsp/main.cpp](../../src/lsp/main.cpp), [positions.cpp](../../src/lsp/positions.cpp) | Shared protocol/session adapter and UTF-16 conversion | No editor semantic engine; source spans remain original |
| [main.cpp](../../src/main.cpp), [native_build.cpp](../../src/native_build.cpp) | Current CLI check/run/emit/build, application/legacy distinction, Windows wide argv, existing native staging/publication | Preserve; console does not redesign native linking or entry selection |
| [stdlib](../../stdlib/README.md) and directory inventory | Directory contains README only; real module catalog absent | Supply console/time source modules and docs through existing standard root |
| [CMakeLists.txt](../../CMakeLists.txt) | C++20, LLVM 19.1.7 EXACT, existing Runtime/backend separation | Add small shared format kernel target/dependencies without changing LLVM pin |

No console/display/operator implementation was found in actual source by the
relevant parser, IR, ABI or stdlib paths. Existing runtime C++ logging and CLI result
printing are not source-language console APIs. Existing field capability metadata
is not declared const-field enforcement. String interpolation support is real, but
general format examples in the Bible remain proposed behavior.

## Original VOID source recovered locally

The source tree is located at:

```text
/home/shmellyorc/Documents/Codex/2026-10-05/referenced-chatgpt-conversation-this-is-an/work/VOID
```

It is a local historical source snapshot, not the current Git checkout or a
verified public release. The separate similarly named local non-world directory
does not contain that implementation. Do not cite its audit's historical pass
counts as newly reproduced evidence.

| Original path relative to that tree | Source observation / transferable lesson |
|---|---|
| `StandardLibrary/Void/Console.void` | Standard readers/writers, redirection flags, encoding policy, Read/ReadLine/ReadKey, color/cursor/title; many per-type Write variants not appropriate for VOID 2.0's one-print direction |
| `StandardLibrary/Void/IO/StandardInput.void` | `_pendingByte`, one ReadByte path, scalar decode, invalid continuation putback, CR lookahead, final partial line vs EOF |
| `StandardLibrary/Void/PrimitiveFormatting.void` | Per-type ToString routes to StringBuilder; reuse common conversion principle, not its type-overload proliferation |
| `Compiler/src/compiler.c`, around 19700–19850 | Native Windows key repeat queue, ReadConsoleInputW, surrogate assembly and input lock; POSIX scoped termios state and key-sequence parsing |
| `Tests/test_console_read_shared_input_buffering.py` | Alternating scalar/line reads, CR followed by data, NUL/supplementary UTF-8, malformed/truncated sequences; useful behavioral fixture ideas |
| `Docs/ConsoleStandardIoTerminalIntegrationAudit.md` | Records managed-standard-writer dispatch after root teardown and direct default-output correction; carry lifetime lesson into Runtime cleanup |

Original `SetIn/Out/Error`, configurable encodings and broad Stream/TextReader
hierarchies are not mandatory copies. VOID 2.0 can deliver complete standard
console behavior with UTF-8, borrowed endpoints and an existing host test context.
Likewise original deferred signal callbacks and async stream APIs do not justify
changing VOID 2.0's worker or signal ownership architecture in Stage 10.

## External primary sources and dependency checks

Retrieved 21 October 2026. These are comparison/evidence sources, not permission to
copy another language's semantics wholesale.

| Source | Finding used |
|---|---|
| [Python print](https://docs.python.org/3/library/functions.html#print) | One print with separator/end/file/flush; no need for a family of newline/conversion functions |
| [Rust Display](https://doc.rust-lang.org/std/fmt/trait.Display.html), [Python format hook](https://docs.python.org/3/reference/datamodel.html#object.__format__) | Owner-defined display/format customization can share ordinary conversion machinery |
| [.NET custom numeric](https://learn.microsoft.com/en-us/dotnet/standard/base-types/custom-numeric-format-strings), [standard numeric](https://learn.microsoft.com/en-us/dotnet/standard/base-types/standard-numeric-format-strings) | Familiar patterns; overloaded comma/sections and historical rounding differences require an explicit VOID rule |
| [.NET date/time patterns](https://learn.microsoft.com/en-us/dotnet/standard/base-types/custom-date-and-time-format-strings) | Understandable calendar tokens, with explicit value/zone applicability in VOID |
| [C# readonly](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/keywords/readonly), [operator declarations](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/operators/operator-overloading) | Constructor assignment and owner/arity precedents; do not import shallow-reference or newer compound-operator semantics |
| [WriteConsole](https://learn.microsoft.com/en-us/windows/console/writeconsole), [Windows VT](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences) | Wide console vs redirected file APIs; VT requires capability/mode support |
| [Linux write](https://man7.org/linux/man-pages/man2/write.2.html), [read](https://man7.org/linux/man-pages/man2/read.2.html), [termios](https://man7.org/linux/man-pages/man3/termios.3.html) | Partial transfer/EOF/EINTR and native mode semantics; Linux evidence is not macOS/Windows execution |
| [Ryu v2.0 commit](https://github.com/ulfjack/ryu/tree/72e18e8d14d4392a4a3261f37cde248e4fbedbec) | Tag API dereferenced to exact commit; `ryu/d2fixed.c` around 480–533 and 717–776 implements tie detection/round-if-odd |
| [Ryu buffered/allocating source](https://github.com/ulfjack/ryu/blob/72e18e8d14d4392a4a3261f37cde248e4fbedbec/ryu/d2fixed.c#L545) | Buffered entry points available; allocating d2fixed uses malloc(2000), so do not forward arbitrary requested precision into it |
| [date v3.0.3 commit](https://github.com/HowardHinnant/date/tree/5bdb7e6f31fac909c090a46dbd9fea27b6e609a4), [CMake options](https://github.com/HowardHinnant/date/blob/5bdb7e6f31fac909c090a46dbd9fea27b6e609a4/CMakeLists.txt) | Exact tag commit; manual/non-system db modes; required library can avoid remote download dependency |
| [date tz.cpp loader](https://github.com/HowardHinnant/date/blob/5bdb7e6f31fac909c090a46dbd9fea27b6e609a4/src/tz.cpp#L3758), [zone semantics](https://howardhinnant.github.io/date/tz.html) | Loader uses filesystem/source streams, not an embedded-byte API; overlaps/gaps explicit; in-memory source adaptation is engineering work |
| [IANA data](https://data.iana.org/time-zones/tz-link.html) | Versioned rules change politically; select/digest/package a real release rather than depend on ambient OS data |
| [IANA 2026e release](https://www.iana.org/time-zones/releases/2026e), [published version](https://data.iana.org/time-zones/tzdb/version) | Official source reports 2026e, released 29 September 2026; concrete recommended initial data baseline, with archive digest to be captured at dependency integration |

The Open Group POSIX pages returned HTTP 403 through the research browser;
Linux man-pages upstream documentation supplied the inspected Linux API evidence.
No native platform experiments or dependency compilation were run. Per-thread
SIGPIPE handling, Windows cancelable-read ownership, terminfo availability,
embedded-zone loader adaptation and exact dependency integration therefore remain
**implementation/verification obligations**, explicitly included in the roadmap.
No source-backed proposal is mislabeled as a successful native experiment.

## Dependency integration constraints

Ryu should use its buffered, nonallocating core with bounded-by-type useful
precision and checked adapter-owned output; append additional requested zeros in
the shared builder. Never use its fixed allocating convenience wrapper as a
general arbitrary-precision API. Preserve upstream license files and select a
compatible license alternative explicitly during engineering review.

date's non-system source loader iterates named input files around lines 3818–3875;
the proposed embedded data integration must replace its input/version source and
disable unused Windows local-zone mapping loading, automatic remote updates and
host-current-zone discovery. Keep its tested rules/calendar algorithms; do not
write a second time-zone engine. Return parse/load failures through Runtime
errors, suppress direct loader stderr output and never silently ignore a corrupt
bundled required rules file. Review the exact adapter before claiming dependency
integration complete. No runtime Git/network/package manager is needed.

Terminfo is a native capability dependency when present, not a hard platform
requirement for ordinary console I/O. A dumb/unknown terminal must remain usable
for text while explicitly reporting unsupported presentation operations.

## Research verification and reproducibility

The final documentation checks verify local link targets, balanced fenced blocks,
whitespace and the changed-path boundary. No executable tests are represented as
passed by this research. Engineering commands/results belong in a future acceptance
record only after execution. The source digest appendix below records inspected
files and the original snapshot; it can distinguish later edits from this baseline.

Executed documentation checks: **PASS** for all seven design documents, 96 local
link targets, balanced fenced blocks, trailing whitespace and `git diff --check`.
All 16 recorded current-source digests were rechecked unchanged. Tracked changes
remain under `docs/`; pre-existing Stage 9 documentation/artifacts were preserved.
Compiler, Runtime, editor, dependency-build and hosted tests: **not run**.

### Historical initial engineering source SHA-256

| Path | SHA-256 |
|---|---|
| `CMakeLists.txt` | `dd2b9dc88852815d8da306d8833547fff789ddb065e717c2f2b9757a0b0d996e` |
| `include/void/ir.hpp` | `aeb9ab89677fe24826de79f20a72feed136440c8f072be4eaf6252a1a06437fb` |
| `include/void/frontend.hpp` | `2f14b29e5398d84b72ff44943aa595a0deaa58ce6d744254bc5079d2a058260d` |
| `include/void/runtime_abi.hpp` | `ce9aed4e408ae82961c8d34228ec5b9ce8b182f947eb5fbfe5277f03e1ca0ed1` |
| `src/frontend/compiler.cpp` | `efa94508269c250901a7e2d9171d60558d81504c6c93827b088f3abb8c90cc38` |
| `src/frontend/strings.inc` | `4a3bf952644e999f5e8770f8633e217c8d3c123c40118429d208c4a37c7c40fd` |
| `src/frontend/capabilities.inc` | `92ea163b33139dd2b3d80897639f7ec8dc260443f94a371aaa146f9e39b68d7a` |
| `src/frontend/construction.inc` | `e53e1f76772f78c1dfae2b75f44f32bcca54b1bf1d4c347f7e0856f99c289d58` |
| `src/ir/module.cpp` | `c82705513ccda2152cad7d48d5e4ab379ec4fe7d000652ddd988b591167801a7` |
| `src/ir/interfaces.inc` | `f3d8ab548419bb11a9c2acddb270f20e8ceaae81efa582318d8309baaf55ec11` |
| `src/ir/reference_capabilities.inc` | `1c7017920038c83113ae98b3e1cdb74d44084cbecda32d9c5302aa07cddb1fb8` |
| `src/runtime/abi.cpp` | `c9d2b7a3d94d5b9472a5b0be758ccfd95155e9436ea2518d5261c491b2d56453` |
| `src/runtime/coordination.cpp` | `49044ee248dc9c471933da2eb3766c0620645b25df62e77175c025cdb6182dec` |
| `src/backend/llvm/backend.cpp` | `fbe77e9c95160ff85da10536f88db7dca68acc90320b12ea496f46b6df4715e0` |
| `src/main.cpp` | `bb39594b99bdf00a8298fd47bc63ff5c8cb9fdc4f236701685f10a2a75554508` |
| `src/native_build.cpp` | `5d71c086bbd73011ca9f2465c8b081e3da38e3ad7080acc4d18f3c796a2ca8ea` |

### Original local snapshot SHA-256

Paths are relative to the historical tree above.

| Path | SHA-256 |
|---|---|
| `StandardLibrary/Void/Console.void` | `54ab357f69f5a8ac492f83c620a71b133f7410d2f002c009b21b814c17215799` |
| `StandardLibrary/Void/IO/StandardInput.void` | `77af9938d9086a0a83678e80784241d0b45b7d714170a5d75f955c7a2c094413` |
| `StandardLibrary/Void/PrimitiveFormatting.void` | `26e4ccdc98806d75e9a78e6a48034cc681f4b9bee16b6487a380eae75355e8a5` |
| `Compiler/src/compiler.c` | `dc0782f2b87620147b5a7870caeab0ed5872f2eab397e171002e9fa6c6d93ad9` |
| `Tests/test_console_read_shared_input_buffering.py` | `4c4de108523f96e9f31220b4c3c95609619503871f8d82ab6120e6215959adff` |
| `Docs/ConsoleStandardIoTerminalIntegrationAudit.md` | `bd572a7593956ccfc3d0a8f0d7b4ba1acee5871a4e6051e58345dafca197cda4` |


## Engineering continuation — 22 October 2026

Boss subsequently authorized all 44 steps and B1–B4. The research baseline above
is historical. [Current implementation, exact open obligations and matrix](IMPLEMENTATION.md)
and its engineering logs record later source changes. No complete Stage 10 or
independent Wrench acceptance is claimed.

Continued primitive source formatting/interpolation evidence is included in
`engineering/results.json` and the `display-*` logs. Native VS Code and Neovim
acceptance is recorded separately; source scope and unsuccessful attempts are
explicit. `IMPLEMENTATION.md` continues to mark the complete stage INCOMPLETE.

### Implemented hooks/temporal/editor continuation

`IMPLEMENTATION.md`, `engineering/results.json` and the `hooks-time-*` logs capture
the current source scope, failed development attempts and focused acceptance.
Nominal/custom hooks, source temporal APIs, malformed-IR checks, ordinary reload
dependencies, installed library relocation and standalone native zone formatting
now have concrete tests. Native Neovim checks include temporal properties and
stdlib navigation. Native Windows/macOS and genuine Vim remain unavailable;
previous VS Code host acceptance predates these latest changes.

Clock adapter investigation used the actual native contracts:
[POSIX clock_gettime/getres](https://www.man7.org/linux/man-pages/man2/clock_getres.2.html),
[Windows precise UTC](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemtimepreciseasfiletime),
and [Windows high-resolution counters](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps).
Linux paths are exercised; Windows code is implemented but not natively verified.
Resolution reporting must not be interpreted as clock accuracy. Full Stage 10
acceptance remains open.

### Step 32 owner-contained declarations — 22 October 2026

The increment adds owner-body parsing and declaration validation; it does not
claim source glyph resolution or compounds. Exact evidence is in
[step32-results.json](engineering/step32-results.json), the `step32-*` logs and
[the recovery ledger](IMPLEMENTATION.md). Positive/negative tests cover all approved
glyphs, mixed exact operands, nullable managed owners, generic specialization,
alias-normalized duplicates and malicious IR. Normal/O2 execution of declaration
bodies uses existing ordinary IR calls, explicitly distinguished from the still
missing source expression selection. Field/hook/generic/LSP neighbors are retained.

Development failures were corrected rather than weakening validation: immutable
operand bindings use the existing snapshot representation, which does not retain
a store initialization flag or scalar readonly type bit. Independent validation
checks single entry argument stores, fixed-place and mutating receiver access,
and iteration-capture replacement. The alias fixture now receives the repository
root explicitly rather than assuming CTest's working directory. Full operator
JIT/AOT/reload/tooling acceptance remains for subsequent integration steps.

### Step 33 exact owner resolution — 22 October 2026

[Step 33 results](engineering/step33-results.json), `step33-*.txt` transcripts and
[source hashes](engineering/step33-source-hashes.json) record actual source
selection, normal/O2 JIT and independently linked native AOT behavior. The existing
MemberSelection shape and IR function catalog supply direct-owner lookup; selected
expressions use ordinary calls, effects, snapshots, roots and failures. Independent
IR reselects from the actual operands and glyph rather than trusting a target name.
No source arithmetic is silently delegated to builtin operations after a custom
match. Derived != calls == once; patterns/internal identity guards remain builtin.

The focused source suite covers generic specialization/definition-scope imports,
private/internal/nominal visibility, two-owner ambiguity, nullable exact matching,
no lifting/boxing/ancestry/promotion/reversed retries, numeric default/result-context
rules, value snapshots/managed identity, readonly results, checked failure/recovery,
selected references and malformed IR. Native fixtures cover every approved glyph
plus generic owners/functions and fresh managed results. Debug neighbors and the
unchanged 1 MiB frontend stack probes pass. Existing LSP regression results concern
field/display/interpolation neighbors; complete operator editor acceptance is open.

One real regression was reproduced: callable-null equality lost its nullable
context during the new pre-selection operand typing. `step33-callable-null-before.txt`
records rejection; `step33-callable-null-after.txt` and final regression acceptance
record the repaired behavior. Earlier passing runs precede this correction and
are labeled separately. Final configuration outcomes remain distinct from full
Stage 10 acceptance; native Windows/macOS and editor-host operator tests were not
run. No hosted CI or Git mutation was performed.

Final Step 33 Linux acceptance: Debug **14/14 (32.60 s)**; strict
ASan/UBSan/leak **3/3 (7.15 s)**, including **161** resolution/call checks,
**278** declaration/IR checks and **16** source JIT/AOT process checks. Strict
runs use the final-source RelWithDebInfo compiler and Runtime outside ptrace
restrictions, with leak checks enabled; generated application code is not
equivalently instrumented. The existing RTTI-off LLVM SDK excludes vptr checks.
Raw final build/configuration/test transcripts are retained under `engineering/`.

### Step 34 call/GC/effect/failure integration — 22 October 2026

[Exact results](engineering/step34-results.json), [source hashes](engineering/step34-source-hashes.json)
and `step34-*.txt` records cover final-source Debug **9/9 (43.01 s)** and strict
ASan/UBSan/leak **7/7 (45.20 s)**. The new harness passes **637** integration
checks; preserved resolution/declaration suites pass **161/278** checks; expanded
source normal/O2 JIT/independently linked AOT coverage passes **124** process checks.
The existing async IR, parallel worker and lifecycle neighbors pass in both builds.
No production compiler/Runtime repair or dependency change was required.

Evidence includes collection during RHS construction, across RHS await, and on
real worker safepoints; generic reference-bearing snapshots and managed identity;
allocation-failure cleanup/retry; exactly-once derived inequality with native
side effects; retained original source and owned errors; older reached cleanup
after failing newer cleanup; rejected native/clock/task/indirect worker effects,
const mutation and structured-cleanup enqueue. Independent IR rejects an inserted
unknown native edge in a worker-reachable operator.

Development records retain a diagnostic-case expectation error, correction from
an incorrect static cleanup-enqueue expectation to the established Runtime guard,
and three initially unbuilt async test executables. Final acceptance has zero
failures or skipped suites. Earlier passing runs precede the final RHS-await tests
and are labeled separately. Strict runs retain leak checks outside ptrace;
compiler/probes/native fixture/Runtime instrumentation does not equivalently
instrument generated application machine code. The RTTI-off SDK excludes vptr.
Release, native Windows/macOS and editor-host operator tests were not run.

Step 34 is ready for independent Wrench review. Step 35 compounds and Step 36 key
protection remain unimplemented, and full Stage 10 acceptance remains open. The
approved future VOID Sections direction is recorded in the Bible/decisions as
documentation only. No Git, hosted CI, publication or public README mutation.

### Step 35 captured-place compounds — 22 October 2026

[Exact results](engineering/step35-results.json) and [frozen source hashes](engineering/step35-source-hashes.json)
record Debug **14/14 (166.01 s)** and strict ASan/UBSan/leak **12/12 (151.86 s)**.
The compound harness passes **730** checks in both configurations; existing
resolution/declaration/Step 34 suites retain **161/278/637** checks. Expanded
normal/O2 source JIT and independently linked AOT fixtures pass **244** process
checks. Fixed/list/dictionary/property/async neighbors and unchanged 1 MiB frontend
stack probes pass; Debug additionally includes field/generic suites.

Implementation uses the existing assignment checker, selected-expression/place
machinery, exact selector and ordinary call/effect/root/failure paths. All five
arithmetic compounds, mixed/generic operands, owner/index/RHS rebindings, mutable
managed identities and value copies, property counts, GC/RHS await and result
assignment conversions are covered. Native traces prove failed setters do not
run and invalidated list/dictionary writes leave old elements unchanged. Checked
failure reaches cleanup. Malformed IR cannot forge a target/glyph, discard return
authority, substitute bad collection write values or read a missing async capture.

Preserved development failures show the array-capture initialization omission,
stale readonly snapshot type and overwritten structural Runtime error; all were
repaired rather than disabling checks. One constructor fixture was corrected to
respect existing explicit initializer parameters. Earlier 14/14 acceptance predates
result-conversion support and is labeled separately. No final failures/skips.

Strict runs retain leak checks outside ptrace and instrument compiler/Runtime/
fixture/probes; generated machine code is not equivalently instrumented, and the
RTTI-off LLVM SDK excludes vptr. The existing ciso646 SDK warning remains unsuppressed.
No new dependency/ABI/GC/backend was introduced. Native Windows/macOS, Release,
editor-host and complete Stage 10 acceptance were not run. Step 35 is ready for
Wrench review; Step 36 recursive custom-equality key exclusion remains unimplemented.
No commits, branches, pushes, hosted CI, publication or public README changes.

### Step 36 recursive dictionary-key protection — 22 October 2026

Shared concrete-owner key eligibility now excludes any custom == declaration and
eligible recursively contained value fields, including private/generic/managed
leaves. Source dictionary annotations and literals identify the key use and original
== declaration, including foreign/private operators and generic instantiations.
Independent IR checks dictionary shapes and declaration/marker agreement, preventing
forged policy removal or insertion. Managed payloads and inherited == declarations
do not alter an otherwise plain direct owner's identity-key eligibility.

The existing Runtime nominal catalog carries canonical key-schema identity and a
transitive exclusion bit, emitted for value as well as managed owners. The private
same-build LLVM/C++ catalog ABI is layout-checked. Transactional explicit key-schema
admission and native dictionary metadata reject forbidden roots and nested managed
leaves; ordinary values/schemas remain legal. Catalog compatibility prevents policy
downgrade or canonical-policy aliasing. Data-only equality/hash and patterns remain
unchanged; no executable dictionary descriptor or user hashing API is introduced.
Runtime metadata is the compiler's private trusted native ABI; independent IR
validation precedes its generation. This does not claim arbitrary hostile native
code can be made safe by descriptor validation.

Fixed arrays remain unsupported keys under DICTIONARIES.md. Custom equality within
fixed shapes, including zero extent, receives the relevant declaration diagnostic;
this increment introduces no new fixed-array hash contract or platform limitation.

Debug focused acceptance: 13/13 suites, 153.94 seconds. The final strengthened key
harness separately passes 131 checks (1/1 CTest, 0.66 seconds), checking the rejection
reason rather than allowing an earlier malformed-shape failure. Preserved operator
resolution/declaration/compound/integration counts are 161/278/730/637, and the
normal/O2 JIT/independently linked AOT runner passes 260 process checks. Existing
dictionary insertion/lookup/hash/collision/reload/allocation-failure, schemas,
generics, patterns, interfaces and fixed/growable array regressions pass. Unchanged
1 MiB source-depth probes pass. Strict ASan/UBSan/leak checks pass 13/13 in
157.82 seconds with the final 131-check harness and 260-process JIT/AOT runner.
Compiler/Runtime/native fixtures are instrumented; generated code is not
equivalently instrumented, and the existing RTTI-off LLVM SDK excludes vptr checks.
Leak checks execute outside ptrace; no suppression or disabled assertion is used.
Raw build/test/detail logs, exact commands, development fixture repairs and source
hashes are in engineering/step36-results.json and step36-*.
No focused Linux failure remains. The original Step 35 manifest is preserved in
step35-checkpoint-source-hashes.json; current code hashes are source-hashes.json
and the bounded step36-source-hashes.json.

Native Windows/macOS, native Vim, latest editor-host checks, Release and consolidated
Stage 10 acceptance were not run for this increment. No Git, hosted dispatch or
public README operation occurred. Step 37 is the next bounded increment.

### Step 37 operator/format compatibility, cache and reload — 22 October 2026

Implemented exact owner/glyph/template/signature/visibility/effect contracts, unused
generic operator sets/owner policy and native binding identities before editor
normalization. Existing graph invalidation follows selected !=/compound calls and
operator owners. Owned framed format-spec syntax, ordinary hooks/helpers, existing
constant/default contracts and full pinned Ryu/date/IANA policy identities enter
compatibility and native cache keys. Existing archive/source cache, operation binding
and whole-generation leases remain the only machinery. No new dependency/license.

Final hash-verified standard-library snapshot: Debug **14/14 (126.81 s)**;
strict ASan/UBSan/leaks **14/14 (139.70 s)**. **219** new reload/cache checks,
**132** hook reload and **8** temporal IR checks; preserved key/operator checks
**131/161/278/730/637**; **276** normal/O2 JIT and independently linked AOT process
checks. Existing cache/allocation/root/failure/multiprocess/tooling/session/analysis
and unchanged 1 MiB depth checks pass. Strict execution is outside ptrace, no checks
suppressed. Generated code is not equivalently instrumented; SDK RTTI-off excludes
vptr. O2 persistence remains disabled by the existing Debug-cache contract.

Earlier harness .count was corrected to .length. One initial current-revision
publication rejection did not reproduce with/without extra inspection assertions;
the minimal variant passes 217 checks. A later default-root run failed two tests
with _year/year library diagnostics. Original library hashes later matched baseline;
isolated, explicit-original and default-root direct reruns passed. Cause remains
**unproven**; external edits are a hypothesis only. Final acceptance uses identical
baseline bytes at an isolated root, whose hashes are retained. No production library,
loader, assertion or protection was modified to accommodate the observations.

Exact commands/results, failures and hashes: engineering/step37-results.json,
step37-library-hashes.json, step37-source-hashes.json and step37-*.txt. Prior Step 36
source manifest is preserved. Native Windows/macOS/Vim/latest editor, separate
Release and final Stage 10 acceptance are unexecuted. Next Step 38 compiler-owned
tooling; no commit/branch/push/hosted CI/public README change.


## Step 38 tooling engineering — 22 October 2026

See [focused results](engineering/step38-results.json), raw step38-* logs and bounded
source hashes. Concrete generic callable/field presentation, direct-owner operator
completion, source glyph navigation/references, guarded rename, typed formatting
suggestions and custom-hook navigation all use compiler-owned metadata and probes.
Actual Linux VS Code/Neovim/Vim paths are exercised alongside protocol/session tests.
Vim's triple-string synchronization defect was reproduced and repaired. Host test
Markdown escaping/snapshot synchronization issues are documented as harness repairs.
Step 37's earlier unexplained publication/time-source observations are preserved,
not described as fixed. Existing hash-verified library snapshot is used explicitly.
Console/composite display integration and final Stage 10 acceptance remain incomplete.

Final Step 38 results: Debug 18/18 (23.88 s), strict ASan/UBSan/leak 18/18
(29.43 s); 20 metadata, 170 LSP, 161 preserved resolution and 219 reload/cache
checks. Actual Linux VS Code 1.143.0 (three trust/workspace profiles), Neovim
0.13.0-dev-1410 and Vim 9.2 pass. TypeScript/build/unit and unchanged 1 MiB source
depth checks pass. Seventeen bounded source hashes, 152 cumulative entries and 132
unchanged prior entries are verified. No final native Windows/macOS/Release/AOT or
whole-stage acceptance is inferred. Exact limitations remain in step38-results.json.


## Step 39 conservative formatting/diagnostics — 22 October 2026

Fixed a reproduced public format_source safety defect: unchanged physical literal
bytes did not prove unchanged indentation tokens inside multiline interpolation.
Existing-parser syntax validation plus physical/expanded token comparison now refuse
unsafe edits. No replacement parser or semantic editor adapter. The LSP keeps its
whole-project semantic/contract/idempotence/revision/disk checks. Original diagnostics
and UTF-16 locations are preserved across safe formatting and dirty repair.

Debug 20/20 (26.71 s), strict ASan/UBSan/leaks 20/20 (31.45 s), plus final temporal
LSP extension 1/1 per configuration pass. Final compiler checks 321, real LSP checks
144; prior metadata/LSP 20/170 and operator resolution/reload 161/219 remain passing.
Actual VS Code 1.143.0 trusted/restricted/multi-root, Neovim and Vim 9.2 pass; native
clients reject a stale formatter result after a newer text revision. Existing 1 MiB
depth checks and VS Code TypeScript/build/unit test pass. Compiler/Runtime sanitizer
coverage does not imply generated JIT instructions are equally instrumented; the
existing RTTI-off LLVM vptr exclusion remains. No new AOT/Release/native Windows/macOS
or consolidated Stage 10 pass was run. Historical Step 37 observations are retained
unexplained. Console/composite implementation and final integration remain open.

Reproduction, exact commands, development fixture failures, logs and source hashes:
engineering/step39-results.json. Step 39's currently applicable functionality has
focused acceptance; independent Wrench review remains pending. Next: Step 40.


## Step 40 bounded implemented-system acceptance — 22 October 2026

Existing integration/native/reload/AOT harnesses now combine private/const generic
fields, initialization, inheritance/composition, exact operators/compounds/inequality,
custom hooks, interpolation and temporal zones/arithmetic. Forced GC/root/frame checks,
allocation failures, failed-hook no-commit/cleanup/source frames, readonly/private
rejection, await/workers/effects and generation/cache compatibility pass. No production
architecture/ABI/dependency change or repair was needed.

Debug combined integration 1/1 (56.91 s), final neighbors/AOT 17/17
(429.13 s); strict ASan/UBSan/leaks 18/18
(516.81 s). Integration 1143, cache/reload 248 and
JIT/O2/linked AOT 344 checks pass; preserved feature/LSP and 1 MiB depth regressions
pass. Eight source workload measurements per configuration validate 10000 interpolated
records (270000 scalars) and a 1000003-byte formatted result checked byte by byte.
Compile/invoke/process wall times and JIT process peak RSS are recorded separately;
AOT memory is not sampled. Native kernel workload remains passing. Initial fixture
errors and old 240 s AOT timeout remain in evidence; expanded 600 s suite budget
retains per-process limits and every check. No arbitrary performance threshold.

Exact commands, failures/repairs, measurements and source hashes:
engineering/step40-results.json. Step 37 observations remain unexplained.
Native Windows/macOS, separate Release, fresh editor/VSIX and final Stage 10 acceptance
are not claimed. Console/terminal Steps 23–31 and composite display remain missing;
complete those and revisit four-system Step 40 before final acceptance. No Git/hosted
CI/public root README action. Stage 10 remains INCOMPLETE; Wrench review is pending.


## Step 23 native stream foundation — 22 October 2026

Added Runtime-owned native sessions over shared borrowed process endpoints, one
input gate and shared stdout/stderr coordination. Native adapter endpoint inspection
and monotonic clock are injectable with isolated contexts. Per-session admission,
stop tokens, queued rejection, granted-operation retention/drain and NativeSafe
cleanup preserve GC/lifetime safety without closing/duplicating host handles.
Lazy initialization prepares outside the GC mutex and rechecks closing before
publishing. No new source/IR console API, renderer, GC path, ABI operation or dependency.

Strengthened self-drain testing reproduced Runtime admission closing before the
self-operation drain rejection. Checking the same operation chain before changing
Runtime state repairs it; the regression remains. Source/closed error priority and
concurrent lazy initialization/close are covered. Existing callback admission and
self-close rules remain intact. Native-state destruction is NativeSafe even when
last host references are released during Managed code.

Debug 7/7 (12.97 s), strict ASan/UBSan/leak 7/7
(5.89 s); 72 native foundation and 14 real POSIX
endpoint checks pass. Neighboring coordination, buffers/schemas, allocation-failure
and fatal-invariant regressions pass. Real pipe/file/PTY/closed-stdin probes verify
independent roles, descriptor flags and untouched Unicode/NUL input after teardown.
Exact source changes, commands, failures/repairs, ownership guarantees, hashes and
limits: engineering/step23-results.json. Native Windows/macOS, new source
JIT/O2/AOT console and final acceptance remain unverified/unimplemented as applicable.
Previous Stage 40 evidence is preserved, not claimed to cover these native changes.
Step 37 observations remain unexplained. Next: Step 24 checked native transfers,
progress/EOF/errors, deadlines and cancellation of owned OS requests. Later decoding,
Windows wide paths, print/terminal/shutdown integration and composite display remain
open. No Git/hosted CI/public root README changes. Stage 10 is INCOMPLETE.


## Step 24 native cancellation blocker — 22 October 2026

No production changes. engineering/step24-feasibility.md records deterministic
Linux evidence against relying solely on readiness/self-pipe for borrowed blocking
output with independent host writers, and against assuming targeted POSIX AIO
cancels an entered pipe read. The standalone probe passes 17 checks under Debug
and strict ASan/UBSan/leaks. Existing seven Runtime suites rerun 7/7 Debug (13.03 s)
and 7/7 strict (5.80 s). io_uring setup succeeds outside restriction and is denied
inside; actual transfers/cancellation are unverified. Native Windows/macOS and all
Step 24 implementation/acceptance remain open pending Boss/Wrench's request/platform
policy decision. Exact logs, commands, source hashes and unchanged earlier checkpoint
verification: engineering/step24-results.json. Step 37 observations remain unresolved.


## Step 24 implemented under Boss's revised contract — 22 October 2026

The preceding blocker is resolved by Boss's approved best-effort entered-syscall
policy. No io_uring requirement or weakened lifetime/progress checks. Implemented
checked native byte transfers in the existing Runtime Adapter/Context/Operation,
owned buffers, short-write completion, explicit EOF and native/cleanup errors, one
monotonic deadline, queued/readiness cancellation and retained entered-call lifetime.
POSIX poll/wake pipes and scoped per-thread SIGPIPE preserve borrowed descriptors
and host state. Windows redirected-byte request/thread/event paths are implemented
but natively unverified; wide console remains Step 26.

A real closed-input regression reproduced wake-descriptor reuse as fd 0 and hung.
Retaining the original availability snapshot repairs both the sequential case and
a confirmed concurrent output wake-fd reuse case; closed stdin/stdout and all other
focused cases pass. Final Debug 9/9 (13.09 s), strict ASan/UBSan/leak 9/9
(6.53 s), 178 deterministic checks and 210 real POSIX driver checks, plus unchanged
72 foundation/14 endpoint checks. Per-case native progress/counters, exact commands,
builds, source hashes, initial failure and limitations: engineering/step24-results.json.
Leak checking ran outside ptrace. Native Windows/macOS, separate Release, source
console/JIT/O2/AOT/editor and final-stage acceptance are not claimed. Previous
checkpoints/Step 37 observations remain preserved; next is Step 25, not started.


## Step 25 shared native input — 22 October 2026

One Streams-owned transactional native queue now serves bytes/char/text/line and
legacy/leased byte reads. The existing shared UTF-8 validator is reused; strict
invalid-prefix recovery, partial/native errors, available text, EOF and immediate
CR/pending CRLF all share the existing input lease/deadline. Allocation faults
after OS consumption preserve queued bytes until owned result construction succeeds.
A new regression reproduced replay of an upstream native encoding error; explicit
event provenance fixes it without conflating decoder recovery. Original failure
log retained. No second parser/GC/I/O framework or public source console API.

Debug 11/11 (13.40 s), strict ASan/UBSan/leak 11/11 (7.20 s), 2261 new deterministic
checks and 264 real POSIX driver checks in 19 native process cases. Prior Step 23–24
checks stay enabled. Actual pipe/file/PTY/large boundary, live CR, timeout/retry and
buffered stop/precise GC evidence plus commands/source hashes/platform limits are
in engineering/step25-results.json. Native Windows/macOS and public source-console
acceptance remain open. Step 37 observations stay unexplained; next Step 26 has
not started. No Git, hosted CI, publication or public root README changes.

## Step 26 Windows wide-console boundary

ReadConsoleW/WriteConsoleW dispatch and allocation-free strict boundary transcoding
now share the existing native worker/lease and UTF-8 queue architecture. Raw
redirection and POSIX transfer policy stay unchanged. Partial UTF-16 completion
maps to exact complete-source-byte progress plus explicit trailing half-scalar
progress; invalid console binary rejects before output. All native storage remains
alive through actual completion under the approved cancellation contract.

Focused Linux production-helper/adapter acceptance: Debug 12/12, final added cases
1/1; 271656 deterministic wide checks. Strict ASan/UBSan/leak 12/12 (7.24 s); exact
logs and source hashes are in engineering/step26-results.json. Steps 23–25 suites
remain enabled. Real Windows SDK compilation/runtime is UNVERIFIED, with failed
local target-header probe retained. Linux simulation is not native Windows proof.
Step 27 public print/source integration has not started. Composite display, native
platform and final Stage 10 acceptance remain open; Step 37 observations preserved.

## Step 27 public print and source-console integration

Public heterogeneous print and standard borrowed endpoints now use existing
frontend calls, typed IR, shared display/builder, LLVM ABI and Runtime sessions.
Recoverable read/write/flush/classification wrappers expose readonly io_error with
operation/stream/native/cleanup and exact UTF-8/partial-UTF16 progress. No source
native handles, general varargs, second renderer or stream registry.

Final focused Debug 25/25 (160.57 s); strict acceptance, exact source-harness/driver
counts and raw output records are in engineering/step27-results.json. Actual binary
comparisons cover 24 normal/O2 JIT/linked-AOT executions and real closed-pipe failure.
Private endpoint/worker/malformed-IR, GC/fault/cleanup, installed stdlib and cache
checks remain enabled. Native Windows/macOS/fresh console editor and final Stage 10
acceptance are not claimed. Stage 37 historical observations remain unexplained.

Retained development evidence proves the const-field capability return defect,
the initial overwritten console-failure detail and the synthetic count-property
writable-walk exception. Repairs preserve const/readonly checks and original failure
frames. Exact backtrace, failed test logs, final reruns, source hashes and remaining
prompts/terminal/composite obligations are recorded for independent Wrench review.

Final frozen-source rerun: Debug 25/25 (160.57 s), strict ASan/UBSan/leak 25/25
(171.38 s); 917 source assertions and 127 driver checks per configuration.
Endpoint default/copy/repeated-template and bare-null allocation-order regressions
pass. Six fixtures give 24 binary-output records per configuration. Earlier passes
are historical; final evidence is engineering/step27-results.json.

### 22 October 2026 — Step 28 explicit prompts and lifecycle

Existing print/read/flush operations now have focused prompt/exit/failure/shutdown
integration acceptance. Repaired admitted source cleanup reporting Invalid console
operation after Runtime close and its first-use intrinsic byte-array metadata
admission. New host admission remains closed; shape/key/readonly checks stay on.
No helper, stream/GC framework, OS mode/signal/handle policy or source ABI added.

Debug 20/20 (109.61 s), strict ASan/UBSan/leak 20/20 (112.09 s); 2141 source
assertions, 278 driver checks, 56 binary output records, 16 live redirected prompts
and two actual broken-pipe cases per configuration. Exact diagnostics, cleanup
order, source frames, partial native progress, allocation/GC and retained entered
call lifetime pass. Evidence: engineering/step28-results.json.
Native Windows/macOS, Release/editor/final-stage verification remain open; Step37
observations preserved verbatim. Next Step29 terminal capabilities/presentation;
not started. No Git mutation, hosted CI, publication or public README edit.

### 22 October 2026 — Stage10 Step29 terminal presentation

Implemented source capabilities/size/cursor/visibility/color/reset/clear/title/bell
through existing console leases, queue, stdlib, typed IR and ABI. Conservative
system terminfo/live ioctl POSIX and native Windows code; no new manager/parser/
GC/registry, hidden tty or mode/code-page mutation. Cursor input ownership and
fresh report parsing preserve queued bytes, CRLF, errors and bounded readiness.
Private console-v2/terminal-v1 compatibility and versioned C entry protect binding.

Final Debug 23/23 (184.87 s), strict ASan/UBSan/leak 23/23 (187.43 s); 2581 source
assertions, 482 terminal assertions, 320 driver checks, 64 JIT/O2/linked-AOT records,
4 source PTYs and 77 native POSIX checks/10 process cases per configuration.
Production native build/run passes. Reproduced/fixed swallowed report-boundary error
and orphaned system-facility caches on dlclose (initial strict 21/23); retain module
for process lifetime, free per-call TERMINAL, preserve host cur_term, no suppression.
Evidence: engineering/step29-results.json.
Native Windows/macOS, Release/editor/full-stage acceptance and Step37 observations
remain open. Step28 stale prompt/lifecycle matrix references corrected; no helper
added. Next Step30 keys/scoped modes/restoration, not started. No Git/hosted CI/
publication/public README action.

## Step30 focused Linux evidence — 23 October 2026

Implemented compiler-owned source keys/availability/scoped sessions over the existing
console queue and native Session/Streams ownership; exact mode restoration, entry
cleanup, partial/error recovery and ordinary IR/JIT/AOT paths. Final Debug 27/27
(393.60 s), strict ASan/UBSan/leak 27/27 (463.49 s); 3205 source
assertions, 127 key/mode checks, 92 real key PTY checks/8 processes per configuration,
72 exact execution records and production native build/run. Native flags and
control characters are serialized; no disabled assertion/sanitizer/suppression.
Preliminary Python 3.11 harness failure and native apply/error repairs are retained.
Exact commands, hashes, counters, output hex and limits are in
[Step30 results](engineering/step30-results.json). Native
Windows/macOS, Release/editor, Step31, composite display and final acceptance remain
open; Step37 observations unchanged. No Git/CI/publication/public README action.


## Step31 console concurrency/lifetime — 23 October 2026

[Focused results](engineering/step31-results.json), frozen/cumulative source hashes,
commands, native flags and exact output bytes preserve the Step30 baseline. Debug
28/28 (341.90s), strict ASan/UBSan/leak 28/28 (352.20s).
No production repair required; native, source/JIT and real PTY harnesses extended.
Permanent Boss editor observations are deferred in IMPLEMENTATION and linked from
TEST_PLAN. Fixture corrections and intentionally interrupted broad build logs are
retained, distinct from final focused acceptance. Step37 observations unresolved;
native Windows/macOS, composite display and final-stage verification remain open.

### 23 October 2026 — Collection display focused acceptance

Implemented iterative fixed-array/list/dictionary display through existing string,
format, interpolation and print paths; typed snapshots/roots, active-path cycles,
ordinary hooks/effects, version failures and complete-record cleanup preserved.
Debug 20/20 (840.04s), strict ASan/UBSan/leak 20/20
(993.45s); 156 conversion checks, 3674 source console
assertions, 199 display reload checks and 84 exact JIT/O2/linked-AOT records per
configuration. Production native builds and five collection cache cold/hit/corrupt/
hook-body/specification checks pass in each configuration.

Equivalent deep proof fast paths retain the unchanged 72-level and forged-IR tests.
A transient pinned Clang build crash remains unexplained; unchanged retry passed.
Step37 observations remain unresolved. Exact commands, failures/repairs, output,
source hashes and limitations: [collection results](engineering/collections-results.json). Enum/result display,
native Windows/macOS, fresh Release/editor cross-products and final Stage10
acceptance remain open. Stop for Wrench review; next source work is enum/result
display in existing frontend adapters. No Git/CI/publication/public README action.

### 23 October 2026 — Enum/result display

Implemented enum/result defaults through the existing typed composite work stack,
all four consumers and shared builder/context; active payloads, Ok(null), cycles,
readonly/copies, ordinary hooks, GC/failures/cleanup and outer record atomicity pass.
Repaired recursive readonly admission using the existing shared authority predicate;
independent IR checks remain unchanged. No new renderer, IR/ABI, GC or dependency.

Debug 24/24 (994.99s), strict ASan/UBSan/leak 24/24 (1137.49s);
203 conversion checks, 4123 source console assertions, 270 display reload checks,
92 exact JIT/O2/linked-AOT records and six cache probes per configuration. Production
native builds and four managed-identity alias paths also pass. [Exact evidence](engineering/sum-display-results.json) includes hashes, commands, repairs and
retained deep-compilation/Clang/Step37 observations. Stage10 remains INCOMPLETE;
native platforms, fresh editor/Release, four-system/final acceptance remain open.
Next: compiler-owned console/composite tooling. Deferred compiler/editor directions
remain recorded only. No Git/CI/publication/public README action.

## 23 October 2026 — Console/composite tooling

[Focused results](engineering/console-tooling-results.json), frozen source manifests,
full LSP requests/responses, exact JIT/O2/linked-AOT output records and actual editor
logs record Linux Debug29/29/strict29/29 plus sum45 each. Prior sum/collection evidence,
Clang reproducer, deep cost and Step37 observations are preserved. Future debugger
direction is documentation only; final/native-platform Stage10 acceptance remains open.

## 23 October 2026 — Step40 final four-system focused integration

[New integration evidence](engineering/step40-final-integration-results.json) records
Debug25/25 and strict25/25 plus sum45 each, exact source/native executions, output
progress, PTY restoration, failure/GC/lease/cache and workload observations.
[Earlier Step40](engineering/step40-results.json) remains partial historical evidence.
Tooling deep-test wording was corrected with the original JSON retained; no earlier
test outcome changed. Step37 and transient Clang observations remain unresolved.
This is focused Step40 acceptance, not Step41 or complete Stage10/platform acceptance.


## Step41 resumed acceptance: blocked security boundary —23 October2026

Final Debug199/199 plus native workload and both genuine1MiB probes pass.
Fresh Release198/199: source_cli and both standalone1MiB gates reproduce SIGSEGV;
Release native workload passes. Final strict199-suite run has not started.
All six editor phases and six default-root controls pass;766 frozen inputs and
historical artifact hashes match. Earlier196/199 Debug and corrected legacy
harness evidence are retained. No partial attempt certifies full acceptance.

The parent-owned Release GDB probe returned `ptrace: Operation not permitted`;
no stack was obtained. Engineering stopped at Boss's required security boundary,
without escalation, substitute program or policy change. Owner recovery decision
and Release failure diagnosis/repair are pending. No limits/guards/timeouts were
changed. See [step41-results.json](engineering/step41-results.json) and the latest implementation ledger checkpoint.
Step41 and Stage10 remain INCOMPLETE; Steps42–44 remain separate.

## Step41 consolidated Linux acceptance — 23 October 2026

[Step41 results](engineering/step41-results.json) record unfiltered Debug199/199,
fresh Release199/199 and strict199/199 (597/597, no final failures/skips), unchanged
1MiB gates, full203/deep72 regression, actual Linux editors, native/GC/console/cache
and workload evidence. Initial/intermediate failures, the builtin-span and result-handler repairs and
three corrected legacy test expectations remain separate. Native Windows/macOS and Steps42–44 remain open.

The Release stack repair diff (+3/-1), original crash and Boss native successful
reproduction are preserved with matching binary/source hashes and observed
1,048,576-byte soft/hard wrapper limits. Original Release build exit was **0**;
no unchanged retry occurred. Its emitted SDK GVN stack remains unexplained,
separate from the preserved historical Clang/Step37 observations.
