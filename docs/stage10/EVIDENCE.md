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

### Current source SHA-256

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
