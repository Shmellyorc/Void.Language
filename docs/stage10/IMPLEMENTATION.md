# Stage 10 engineering progress and permanent handoff

Updated 24 October 2026 (America/Los_Angeles). **INCOMPLETE.**
Current Step42 checkpoint: final macOS Release fixture synchronization repairs
implemented; Golden Wrench review and repaired native Release verification pending.
MainCI98 and ThinLTO experiment#3 at9a4985f8 each passed6/7 jobs. All configurations
except macOS Release are green. Main Release198/199 failed the redirected-input
terminal fixture; experiment Release198/199 failed the missing-stdin cancelled
write prefix fixture. These are separate observations, not a ThinLTO cause claim.

The two demonstrated fixture ordering holes are repaired without changing Runtime:
redirected input is queued before its zero-timeout probe starts, and cancellation
waits for an actual native writable→not-writable pipe transition. All original
assertions remain. Focused Linux Debug11/11 and strict11/11 pass, with each changed
native fixture20/20 per configuration. This is test-driver verification, not native
macOS Release or full CMake/JIT/AOT acceptance. Both workflows, all production
sources, original CI97 builds, Step41 evidence and1MiB protections are unchanged.
Steps42–44 remain open; Stage10 INCOMPLETE. See engineering/step42-results.json and
[final Release diagnosis, raw records and remaining gates](engineering/ci98/DIAGNOSIS.md).
Boss controls commits and native/hosted verification. Earlier records below retain
their historical status and evidence.

Original mission baseline HEAD: `e25816bdb80e3f32613edf77c9cf3258232af966`.
Step 32 builds on the owner checkpoint `ecf0bec6e04ef66e1e50cac4afc1598a945066c0`;
this increment performs no Git mutation. Existing Stage 9
records/artifacts are preserved. Root README, Git history/branches, hosted CI and
publication are untouched. No known-issues entry disguises missing Stage 10 work.

## Current source changes

- Stored fields accept public/internal/private and const, including const using;
  const events and duplicate/conflicting/out-of-order modifiers reject. Original
  declaring owners survive inheritance and generic specialization. Receiver
  methods and nested closures retain lexical owner metadata; method/property
  file-private semantics stay in the existing method access predicate.
- Shared member selection, explicit constructor labels and field patterns enforce
  field privacy. Const fields project readonly reference leaves and reject direct,
  nested/indexed and mutating-receiver writes. Separate mutable aliases retain
  authority. Own init may directly replace its field repeatedly; helpers,
  source aliases, closures and derived phases receive no grant.
- Independent IR checks validate field policy, inherited policy consistency,
  owner provenance, readonly projection, initializer identity/receiver provenance,
  private access, const stores and mutable receiver paths. Private final commit
  copies are allowed only when their read has exactly one commit consumer; slot
  indexes must not be counted as SSA operands. Existing construction proof still
  validates actual stage origin and phase.
- A const promoted component produces a readonly interface view. Shared binding
  metadata derives this policy; both frontend capability facts and independent
  interface-pack validation prevent authority restoration.
- Exact field compatibility and invalidation include visibility/const. Compiler
  completion hides inaccessible fields and marks const; hover names declaring
  owner. New stdio LSP regression covers failed dirty edits and repair.
- One LLVM-independent checked byte builder, owning numeric plan/parser and native
  numeric renderer live in `include/void/format.hpp` / `src/format.cpp`. Implemented
  decimal placeholders, literals, grouping, signs, fixed/optional fractions,
  percent, D/F/N/P/X/x/B/b/E/e aliases, scientific notation, scalar-count alignment,
  signed minima, integer-width bit rendering, special floats and signed zero.
  Percent shifts exact decimal digits; rounding uses guard/sticky/tie information
  once. Finite f64 exact expansion uses 1074 decimal fraction places; additional
  requested zeros are appended without a public precision cap. Counts/extents are
  checked. Ryu f32 shortest is used for f32; exact widening is used for fixed f32.
- Pinned date and IANA data load from compiled-in source bytes. The small vendored
  input-boundary adaptation skips file/folder discovery, remote APIs, OS data and
  Windows local-zone mapping input. No curl dependency or executable sidecar rules
  file is needed. Upstream source notices, chosen Ryu Boost license and IANA
  license are retained; date-linked executable bytes include the notices through
  an accessed volatile pointer anchor. The macOS support header is vendored.
  CMake packages formatter/date object files into existing core/Runtime archives;
  no extra native link archive is needed. Existing archive cache hashes include
  dependency/data code bytes.

- Primitive source `string(value)` and `format(value, spec)` now use this kernel
  through independent typed IR operations and rooted checked Runtime ABI calls.
  Numeric widths, bool and immutable strings participate; formatting returns the
  existing result<string,string>, and dynamic invalid patterns return Err with
  byte offsets. Named arguments capture in written order. Ordinary `format`
  declarations keep existing lookup precedence. Compiler-owned hover/signature
  records and constant evaluation share the same parser/renderer.
- Interpolation now parses ordinary expressions on original offsets, validates
  literal specs for primitive display, converts each hole once before the next,
  and assembles all parts with one checked owned builder. Added triple strings,
  preserved physical line endings, multiline holes/comments and parenthesized
  if/match/lambda bodies. Quoted spec braces, nested strings and original parameter
  navigation are covered. LLVM joins pass addresses of existing root homes, with
  entry-allocated argument storage. Earlier string-only negatives became positive
  Stage 10 regressions; malformed forms remain negative cases.
- VS Code/Vim syntax rules recognize multiline strings and holes. Native Neovim
  field/display/interpolation and syntax acceptance passes. VS Code unit/typecheck and native trusted, restricted and multi-root host profiles pass. Initial host attempts exposed test assertions that serialized MarkdownString as an empty object; direct value access fixed the assertion. The default `vim` executable is Neovim; actual Vim9.2 was subsequently verified
  using /usr/bin/vim.basic in the console/composite tooling increment.

- Nominal display adapters now use ordinary typed functions, shared member selection,
  calls/effects and roots. Exact synchronous nonmutating `string()` / `format(string)`
  hooks support generic owners, promoted components, private lexical field access,
  nullable references, custom-pattern interpolation and shared scalar alignment.
  Opaque defaults expose no fields. Empty patterns select default display. Hook
  rejections preserve owned source failure text through cleanup and worker tasks.
  Runtime thread-local guards reject managed object/hook re-entry and enforce the
  approved 64-active-hook value nesting limit; all normal/failure exits unwind them.
  Construction analysis rejects whole unfinished-self display, even after fields
  become ready. Independent IR rejects construction receiver hooks and mutation.
  Generated adapters remain absent from user declarations/references.

- Added the native temporal kernel and `use time` source library: private const
  component values/factories, date/time/local/instant/offset/duration/zone/zoned
  values, checked seconds/fraction arithmetic, bundled-zone resolution with explicit
  overlap choice and gap rejection, custom hooks over the shared temporal parser,
  UTC and per-Runtime monotonic samples/elapsed/compare and native resolution
  queries. Seconds remain i64 and fractions separate; no i64-nanosecond instant
  restriction. Temporal interpolation validates builtin token applicability through
  the same renderer grammar. New private typed-IR/ABI operations validate exact
  scalar/string signatures and return existing results; native work copies strings
  before NativeSafe and publishes rooted results afterward. Clock reads remain
  worker-prohibited; pure temporal formatting uses normal call/effect analysis.
- Source standard libraries and notices now install with compiler and tooling;
  CLI/LSP prefer matching executable-relative installed sources after an explicit
  `VOID_STDLIB_ROOT` override. Build-tree fallback remains configured explicitly.
  Native time-zone rules remain embedded; executables require no data sidecar.
- Integration exposed and fixed a display adapter cache identity defect: readonly
  paths alone do not identify a type. Adapter/prelude identities now include kind,
  exact length-prefixed nominal identity and nullability as well as capabilities.
  Added mixed-nominal and nullable/nonnul regressions. Shared numeric contextual
  hints now recognize property getter results.

- Step 32 adds owner-contained operator declarations to existing Function/Structure
  storage and ordinary body/effect/lowering paths. Value structs, managed structs,
  enums and instantiated generic owners accept unary +/-, binary + - * / %, ==
  and independent < > <= >=. Every operand is explicitly const; exact owner
  participation, arity, result category, bool comparisons, visibility and duplicate
  tuples are validated. Unsupported glyphs, independent !=, compounds, defaults,
  operator generics and prohibited modifiers reject. Operators do not become
  global functions, receiver methods, inherited members or promoted members.
- Concrete identities encode owner, glyph, arity and exact operand types without
  result/visibility ranking. Generic source identities use tokenized type patterns,
  not source offsets; concrete collisions diagnose at specialization. Unused
  templates validate headers/types/duplicate tuples and retain ordinary deferred
  body checking. Analyzed declarations own glyph spans, operands, result, owner,
  visibility and effects, and forbid glyph rename.
- Independent IR validates operator declarations and lexical owner provenance.
  Const operand bindings require one entry argument snapshot and reject mutating
  receiver/fixed-place access or iteration-capture replacement; deep reference
  authority uses existing capability validation. Immutable operand captures reuse
  the existing closure snapshot path, keeping private owner context without any
  constructor grant. Ordinary result-effect inference preserves readonly aliases
  and fresh result authority. Step 33 below supplies source expression selection.

- Step 33 selects unary/binary operators from direct static nominal operand owners
  using the shared IR function catalog and existing MemberSelection result shape.
  Exact type/nullability/concrete generic tuples, direct original-owner import,
  nominal visibility, owner-private visibility and readonly capabilities are
  checked before builtin fallback. No ancestor/promotion search, coercion,
  reversed retry, commutative rewrite or result-type ranking is performed.
- Selected expressions become ordinary typed calls with written-order operand
  snapshots, existing effects, roots and checked failures. Derived != calls the
  selected == once then negates its bool. Supporting call integration crosses
  into Step 34 deliberately: selected operations never lower as builtins.
  Each IR operator call carries its glyph proof; independent validation reselects
  from actual operand IR types and scope rather than trusting the callee string.
- Selected glyphs (including != -> ==) have ordinary source references/dependencies
  and original spans. Pure hints use declaration results for arithmetic chaining
  and generic inference without evaluating or rewriting operands. Unfinished
  managed self/aliases cannot enter operators; complete value snapshots remain
  ordinary values. Compiler-generated nullable display guards retain identity
  semantics, and pattern comparison remains data-only.
- Builtin numeric context/range handling, integer remainder, related-reference
  identity and nullable callable equality remain supported. A real development
  regression in callable-null context was reproduced and repaired, with both
  operand directions and != covered by normal/O2 execution.

- Step 34 completes focused call/GC/effect/failure integration using the existing
  generated-code ABI probes, native fixture and ordinary async/Runtime harness.
  There is no replacement operator backend or GC path, and no production-code
  repair was required by the exercised cases. Forced collection occurs before
  source construction, during RHS evaluation, across RHS await suspension, and
  on actual worker safepoints. Generic reference-bearing value snapshots retain
  their original contents when RHS replaces the owning field; managed snapshots
  retain identity when RHS mutates that object. Fresh results retain independence.
- Whole-invocation allocation-failure sweeps verify frame/root cleanup, empty
  post-call heaps and successful retry. Operator/caller reached defers execute
  after checked failure; older reached cleanup survives a failing newer defer,
  unreached cleanup stays unexecuted, and original glyph/callee/source frames
  remain available after releasing caller-owned analysis/source storage.
- Derived != invokes == once in successful and failing cases. Native side-effect
  traces prove left/RHS/operator/cleanup order and prove an RHS failure skips the
  operator. Dynamically allocated primary error text survives a second failing
  format hook during cleanup, including worker error handoff. Worker-private
  allocations and concrete generic/string results work; native, indirect, task,
  clock and mutable-through-const effects reject at their established boundaries.
  Indirect task enqueue during structured cleanup is a checked Runtime rejection,
  as for ordinary helpers. Independent IR rejects an unknown native edge inserted
  into a worker-reachable selected operator. Step 32–33 validation remains intact.
- Boss's approved future `# section Name` / `# endsection` direction is recorded
  in LANGUAGE_BIBLE.md and DECISIONS.md. It is documentation only and outside
  Stage 10 implementation; no editor or language Sections feature was added.

- Native console Step 24 extends the Step 23 Runtime/Adapter/lease path with checked
  owned bytes, complete record writes, short reads/EOF/native/cleanup errors and
  one deadline through admission and retries. POSIX poll/wake and scoped SIGPIPE
  preserve host descriptors/state. Queue/readiness stop is prompt; entered calls
  retain lifetime through actual completion under Boss's best-effort contract.
  Windows byte paths are written but native Windows/macOS acceptance remains open.

- Native console Step 25 routes bytes (including the old read/leased-read entry),
  characters, text and lines through one Streams-owned transactional native queue.
  Existing UTF-8 validation is reused; invalid-prefix recovery, exact partial
  progress, EOF and immediate CR/pending CRLF ownership share one input lease and
  deadline. Owned results are built before queue commit, preserving data on native
  allocation failure. Runtime sessions retain shared input independently of close.

- Step40 combines the completed systems in the existing source/native console and
  operator harnesses. The unchanged 1 MiB proof test exposed two frontend stack
  regressions: cold pattern-reference work now uses frontend_case, and editor name
  probing borrows its expected type. Final-source Debug/strict acceptance passes;
  semantic limits, readonly rules and diagnostics are unchanged.

**Remaining obligations:** Wrench review of consolidated Linux Step41 acceptance,
then native Windows/macOS and Steps42–44 acceptance. Console/composite tooling now has
focused Linux Debug/strict and actual VS Code/Neovim/Vim acceptance (29/29 per
configuration; engineering/console-tooling-results.json). Steps23–31 console
source/native systems have focused Linux acceptance; native Windows/macOS and
subsequent platform/final-stage acceptance remain open. No omitted implementation is called an
unsupported platform capability. Earlier field/operator/format/temporal work and
Step37's unexplained observations remain preserved.

## Dependency identity and license

See `third_party/stage10-versions.json` for downloaded archive SHA-256 digests.
Ryu commit `72e18e8d14d4392a4a3261f37cde248e4fbedbec` uses the Boost Software
License 1.0 option (upstream dual-license texts retained). Date v3.0.3 commit
`5bdb7e6f31fac909c090a46dbd9fea27b6e609a4` uses MIT notices preserved by file in
`third_party/date/NOTICE`. IANA 2026e source data is public domain as described by
its included LICENSE. `tools/embed_tzdata.py` reproducibly generates the embedded
header with per-file digests and portable byte escapes.

## Current verification

Step41 unfiltered consolidated Linux acceptance passes Debug 199/199, fresh
Release 199/199 and strict 199/199, plus unchanged 1 MiB frontend/depth and actual Linux
editor gates. Exact final-source evidence: engineering/step41-results.json.
Native Windows/macOS and Steps42–44 remain open; Stage10 INCOMPLETE.

### Preserved Step40 focused verification

Step40 completed-system integration passes focused Linux Debug **25/25** and strict
ASan/UBSan/leak **25/25** on the final frozen sources, plus **45** sum-display checks
each and unchanged Debug **1 MiB** proof/recovery probes. Exact JIT/O2/independent AOT, production native
build, real prompts/PTYs/partial progress, GC/fault/cleanup/cache/reload and workload
evidence: engineering/step40-final-integration-results.json. Two demonstrated
frontend stack-frame defects were repaired; all limits/protections are unchanged.
At this Step40 checkpoint consolidated Linux and native-platform/final obligations
were open. Current Step41 Linux acceptance is recorded above and in the final handoff.

### Preserved Step31 verification

Step31 console concurrency/lifetime integration has focused Linux acceptance:
Debug **28/28 (341.90 s)** and strict ASan/UBSan/leak
**28/28 (352.20 s)**. Per configuration: 3234 source
assertions, 347 native foundation checks, 127 key/mode checks, 99 real key PTY checks
in 9 processes, 72 normal/O2 JIT/linked-AOT output records and production native
build/run. A final native-only GC/closing extension passes 1/1 in each configuration; the
other 27 suites and production binaries are unchanged. Exact commands, source hashes,
native flags, diagnostics and generation
lifetime evidence: engineering/step31-results.json. Prior Step30/Step29 acceptance
remains preserved. No production defect required a repair in this increment.
Stage10 is INCOMPLETE; native Windows/macOS/final acceptance remain open.

Step 28 prompt/exit/failure/shutdown acceptance remains preserved in
engineering/step28-results.json; no prompt helper was approved or added. Step 27's
source/ABI checkpoint likewise remains historical and unchanged. This is focused
implementation acceptance, not a completed Stage 10 or independent Wrench review.

### Preserved Step 26 verification

Step 26 Windows wide-console paths are implemented; focused Linux conversion,
shared-queue/adapter and Runtime acceptance passes. Debug **12/12 (13.62 s)**;
final additional wide cases **1/1 (0.10 s)**, **271656** deterministic wide checks.
Strict ASan/UBSan/leak **12/12 (7.24 s)**. Exact commands/logs are in
engineering/step26-results.json.
Native Windows compilation/execution is **UNVERIFIED**: no Windows SDK/target C++
headers are installed; the real cross-compile probe fails before source checking.
No Linux simulation is native Windows proof. Next: Step 27 public print/source
integration. Stage 10 remains INCOMPLETE and Step 37 observations remain unexplained.

### Preserved Step 25 verification

Shared native input: Debug **11/11 (13.40 s)**; strict ASan/UBSan/leak
**11/11 (7.20 s)**; **2261** deterministic checks and **264** POSIX driver checks
across **19** native cases. Exact evidence: engineering/step25-results.json.

### Preserved Step 24 and Step 23 verification

Step 24 checked native transfers are implemented under Boss's approved best-effort
entered-syscall cancellation contract. Focused Linux Debug **9/9 (13.09 s)** and
strict ASan/UBSan/leak **9/9 (6.53 s)** pass. New transfer checks **178**, real POSIX
driver checks **210**; prior foundation **72** and endpoint **14** checks remain.
Native Windows/macOS acceptance is still unverified, and no source console API is
claimed. Exact contract, repair, logs and hashes: engineering/step24-results.json.
The historical feasibility blocker was resolved by the approved contract change,
not by claiming that readiness interrupts an entered syscall. Next: Step 25.

Step 23 native foundation passes focused Linux Debug **7/7 (12.97 s)**
and strict ASan/UBSan/leak **7/7 (5.89 s)**. Native foundation
**72** checks and real POSIX pipe/file/PTY/closed-input probes **14** checks pass.
Existing coordination, buffer/schema, allocation-failure and fatal-invariant tests
remain passing. Source changes/guarantees, reproduced self-drain repair, raw logs,
platform boundaries and hashes: engineering/step23-results.json. Step 23 exposes
no source console API. Step 24 now adds native transfers; Console Steps 25–31,
composite display, complete integration/final acceptance and native Windows/macOS
remain open. Step 37 observations remain unexplained.

### Preserved Step 40 verification

Step 40 bounded integration passes **18 unique focused Debug suites**: combined
integration **1/1 (56.91 s)** followed by final neighbors/AOT **17/17
(429.13 s)**. Strict ASan/UBSan/leak **18/18
(516.81 s)** passes. Integration **1143**, cache/reload
**248**, normal/O2 JIT/linked AOT **344** checks pass. The existing source-formatting,
metadata, LSP, field/hook/temporal/compound/key and numeric regressions remain passing.
Both configurations record **8** source workload measurements; all 1000003 output
bytes of the large case are checked. Existing native-format workloads and unchanged
1 MiB depth probes pass. Full results, failed development attempts, corrected test
budget, exact metrics and limits: engineering/step40-results.json. No production
repair was required. Console/composite and complete four-system Stage 10 acceptance
remain OPEN, as do historical Step 37 observations and native Windows/macOS gates.

### Preserved Step 39 verification

Step 39 focused Linux Debug **20/20 (26.71 s)** and strict ASan/UBSan/leak
**20/20 (31.45 s)** pass. The final added temporal formatting cases pass a targeted
**1/1** in each configuration (Debug **1.01 s**, strict **0.95 s**). The compiler
formatter harness passes **321** checks and the final real LSP harness **144**;
preserved Step 38 metadata/LSP **20/170** and operator resolution/reload **161/219**
checks pass. Actual VS Code **1.143.0** trusted/restricted/multi-root profiles,
Neovim and Vim **9.2** pass, including real formatting/literal preservation and
stale-edit rejection. VS Code TypeScript/build/unit test and unchanged 1 MiB depth
probes pass. No remaining focused failure. Full commands, development failures,
raw logs, source hashes and platform/instrumentation limits:
`engineering/step39-results.json`. Stage 37's unexplained observations and Stage 10's
unimplemented console/composite systems remain open. Final Stage 10 and native
Windows/macOS acceptance are not claimed.

### Preserved Step 38 verification

Step 38 final-source focused Linux Debug **18/18 (23.88 s)** and strict
ASan/UBSan/leak **18/18 (29.43 s)** pass. New metadata **20** and LSP **170**
checks pass, with preserved **161** operator resolution and **219** reload/cache
checks, normal/O2 JIT, immutable sessions, protocol/discovery and unchanged 1 MiB
depth probes. Actual VS Code **1.143.0** trusted/restricted/multi-root profiles,
Neovim **0.13.0-dev-1410** and real Vim **9.2** pass; VS Code's TypeScript/build
and existing unit test pass. New Stage 10 LSP tests shut down cleanly and check
sanitizer stderr. Native Windows/macOS, separate Release, fresh AOT for this
metadata-only increment and consolidated Stage 10 acceptance were not run.
Full commands, development failures, exact host scope and source hashes are in
`engineering/step38-results.json`. Historical Step 37 observations below remain
unresolved. No Stage 10 completion claim is made.

### Preserved Step 37 verification

Step 37 final-source, verified-library-snapshot Debug **14/14 (126.81 s)**
and strict ASan/UBSan/leak **14/14 (139.70 s)** pass: **219** new reload/cache
checks, **132** hook reload checks, **8** temporal IR/worker checks, preserved
**131/161/278/730/637** key/operator checks, and **276** normal/O2 JIT/linked AOT
process checks. Existing cache/allocation-failure/multiprocess/tooling/session/
analysis tests and unchanged 1 MiB source-depth probes pass. Full commands, hashes,
raw failures and instrumentation/platform limits: `engineering/step37-results.json`.
Earlier isolated publication and default-root time-library failures did not reproduce
in controlled reruns; their cause is **unproven**, not asserted repaired. Final
acceptance uses an isolated copy matching the baseline standard-library hash.
Stage 10 remains incomplete; independent Wrench/native/final acceptance is open.


Step 36 Debug **13/13 (153.94 s)** and strict ASan/UBSan/leak
**13/13 (157.82 s)** pass. Final key harness: **131** checks in Debug and strict;
a separate strengthened Debug rerun passes **1/1 (0.66 s)**. Preserved operator
resolution/declaration/compound/integration checks: **161/278/730/637**. Normal/O2
JIT and independently linked AOT pass **260** process checks. Existing dictionary,
schema, generic, pattern, interface and fixed/growable array neighbors and unchanged
1 MiB frontend probes pass. Exact commands, raw logs, development fixture repairs
and source manifests are in `engineering/step36-results.json`. No focused Linux
failure remains. Generated machine code is not equivalently sanitizer-instrumented;
RTTI-off LLVM excludes vptr. Release, native Windows/macOS, native Vim/latest editor
hosts and consolidated Stage 10 acceptance remain unexecuted. Stage 10 is incomplete.

Step 35 final-source Debug **14/14 (166.01 s)** and strict ASan/UBSan/leak
**12/12 (151.86 s)** pass. Counts: **730** captured-compound checks, **161**
preserved resolution checks, **278** declaration checks, **637** Step 34
integration checks, and **244** normal/O2 JIT/independently linked AOT process
checks. Existing fixed/list/dictionary/property/async neighbors pass; Debug also
covers fields/generics. Unchanged 1 MiB frontend stack probes pass. Exact commands,
frozen source hashes, development failures and raw logs are in
`engineering/step35-results.json`. Strict compiler/Runtime/probe instrumentation
does not equivalently instrument generated machine code; RTTI-off LLVM excludes
vptr checks. No Release, native Windows/macOS or native editor-host result is
claimed, and complete Stage 10 acceptance remains open.

Step 34 final-source Debug **9/9 (43.01 s)** and strict ASan/UBSan/leak
**7/7 (45.20 s)** pass: **637** integration checks, **161** resolution checks,
**278** declaration/IR checks and **124** normal/O2 JIT/AOT process checks.
Exact commands are recorded in `engineering/step34-results.json` and its raw
build/test transcripts. The new
integration harness adds GC, allocation failure, worker, retained error, cleanup
and RHS suspension coverage; the independently linked native fixture set now
includes these behaviors. Step 33's results below are the earlier checkpoint.
No final Stage 10, Release or native Windows/macOS/editor acceptance is inferred.


Step 33 current-source Debug focused acceptance passes **14/14 (32.60 s)**,
including **161 resolution/call checks**, **278 preserved declaration checks** and
**16 source normal/O2 JIT/independently linked AOT process checks**. Neighboring
field, hook, generic, conversion, interpolation, reload, Sprint 6 and stdio LSP
suites pass. The unchanged **1 MiB** receiver/numeric/string-index/type grammar
probes pass. A callable-null comparison regression was reproduced, fixed and
covered in both operand directions and derived !=. Strict ASan/UBSan/leak acceptance passes **3/3 (7.15 s)** on the same final
source, outside sandbox ptrace restrictions. Compiler/Runtime instrumentation does
not equivalently instrument generated application code. Exact records are in
`engineering/step33-results.json`.
These checks do not close the full operator system or Stage 10.

The following Step 32 results precede Step 33 source changes:

Step 32 final Debug focused acceptance passes **8/8 (17.63 s)**, including **278
operator declaration/validation checks**, normal/O2 ordinary-IR-call execution of
all twelve approved declarations, field/hook/reload/generic and LSP neighbors.
Strict ASan/UBSan/leak focused acceptance passes **3/3 (10.00 s)** outside
sandbox ptrace. No failures remain in these final focused runs. This is declaration validation, not
source operator resolution or complete operator acceptance. See
`engineering/step32-results.json` and the `step32-*` raw logs.

The following hook/temporal results precede Step 32 source changes:

Current continuation implements nominal/custom hooks and the native/source temporal
system. Debug hook checks pass **77**, including exact mixed-type/nullability adapter
identity, normal/O2 execution, recursion recovery, construction rejection and worker
failure retention. Temporal malformed-IR/worker tests pass **8** checks. Latest Debug
editor/reload regressions pass **5/5 (2.44 s)** after repairing incomplete conditional
calls/comments without counting synthetic interpolation parentheses. Native Neovim
passes field/display/interpolation and new time API/property/navigation checks.

Final current-source Debug **16/16 (53.35 s)** and Release **16/16 (28.82 s)**
focused tests pass. These include **1,308 temporal core**, **132 hook dependency/
reload**, **23 temporal JIT/O2/AOT/rejection** checks plus existing field, numeric,
conversion/interpolation and editor neighbors. Installed relocation passes **1/1
(3.49 s)** with 12 internal checks: packaged sources/notices, actual installed-source
lookup, relocated LSP and a standalone native historical-zone executable. Strict
ASan/UBSan/leak checks pass **16/16 (53.71 s)** outside sandbox ptrace, with
protections enabled and final current-source rebuild. There are no failures in
these final focused runs. Earlier failed attempts remain recorded. Prior Release 12/12 (24.52 s)
preceded the final editor repair and is retained as historical evidence.
A full Stage 10 acceptance has not run.

Previous integration: Debug **11/11 (16.57 s)**, Release **5/5 (2.13 s)**,
strict ASan/UBSan/leak **5/5 (5.12 s)**. Primitive source conversion has 78 checks;
interpolation has 57, alongside 54 field and 18,663 native numeric checks.
Debug includes the final metadata correction: internal interpolation operations
must not appear as written references to nonexistent declarations, which blocked
otherwise safe rename. Release/sanitizer checks precede that compiler-only
metadata correction; their primitive parser/IR/LLVM/Runtime bytes are unchanged.
A targeted stdio rename regression passes after the correction. Do not interpret
these focused checks as the final whole-stage matrix.

Normal/O2 JIT and independent AOT pass a combined private const construction +
primitive interpolation/conversion/format fixture (16 process checks). The new
example `examples/stage10_fields_display/main.void` returns 23 normally and at O2.
A repeated 1,000-iteration interpolation workload and unchanged 1 MiB semantic
stack budget pass. Native VS Code 1.141.0 passed trusted, restricted and multi-root
profiles before the final metadata fix; native Neovim and stdio LSP also pass.
A true Vim binary is unavailable: `/usr/bin/vim` is Neovim, which cannot execute
the saved targeted Vim9 script. Native Windows/macOS remain unexecuted.
Current machine-readable scope: `engineering/results.json`; prior field/kernel
results are preserved separately in `engineering/field-kernel-results.json`.

The following bullets retain earlier engineering/baseline evidence:

Toolchain: Clang/LLVM 19.1.7, CMake 4.3.4, Python 3.14.7, Linux x86_64.
`rg` and `/usr/bin/time` are unavailable; grep/find and Python resource measurement
were used. No native Windows/macOS environment is available. Wine presence does
not establish native acceptance.

- Pre-edit baseline numeric/frontend: 2/2. Four other selected old field suites
  passed; properties was initially Not Run because its executable was absent,
  then built and passed. The first regex selected only two tests, not nine.
- Focused field tests currently cover normal/O2 execution and malformed IR: **54 checks**. Final configuration
  outcomes and commands are retained in `engineering/`.
  Linux Debug focused checks passed **17/17 (80.13 s)**; Release **3/3 (0.51 s)**.
  Strict ASan/UBSan/leak checks passed **4/4 (5.96 s)** outside ptrace after
  fresh linking; final numeric-only refinements have their own targeted logs.
  These are focused configuration/suite outcomes, not a full stage acceptance.
- `tests/stage10/fields.py` passed 16 checks spanning normal/O2 JIT and independent
  native linking, including inheritance, private closure reads, const lists,
  generic private const storage and a readonly promoted interface view. Output is legacy entry-result driver output,
  not a source print implementation.
- `tests/stage10/lsp_fields.py` passed actual stdio server field privacy, const
  hover/completion, navigation, dirty rejection and repair checks. These earlier field-only results precede the later native VS Code/Neovim runs above; actual Vim acceptance remains open.
- Existing 1 MiB receiver, numeric receiver, scalar indexing and recursive type
  grammar checks passed with bounded rejection and later diagnostic recovery.
- Native numeric tests compare exact IEEE bit inputs with checked-in Python
  Decimal.from_float / ROUND_HALF_EVEN vectors under supported fenv settings and
  available locales; additional shortest roundtrip, integer-width and quoted-sign
  checks run (**18,663 checks** on this host).
  Upstream Ryu tests are retained but have not been independently run as an
  upstream GoogleTest target.
- Embedded zone tests load 344 zones / 253 links, check 1,720 projections,
  New York gap/overlap and Paris historical offset seconds with a nonexistent
  configured database path. These are parser/dependency tests, not source time API
  acceptance or exhaustive validation of every historical transition.
- Release native workload: 1,048,576 builder bytes, 1,000,003 bytes of requested
  numeric precision and 100,000 integer displays. See `engineering/workload.json`.
  Peak RSS includes process startup; live memory is not measured. A later isolated workload child is recorded in engineering/display-workload.json. No benchmark
  threshold or full system performance claim.

Engineering failures retained: wrong initial Ryu header guesses were corrected to
this revision's ryu2.h; data comments required byte-safe UTF-8 embedding; the first
new CMake target invocation preceded regeneration; initial generic fixture used
uppercase type parameters contrary to existing grammar. A real initial const-init
IR rejection was fixed by retaining the stable original construction receiver.
The combined AOT fixture then exposed the slot-vs-SSA commit-read counting defect;
it was repaired without weakening the unique-consumer check. The old inheritance
malformed-IR fixture was rejected earlier by the strengthened authority proof;
its diagnostic now explicitly says readonly, preserving its original expectation.
A combined readonly-interface AOT fixture initially selected a requirement whose
other provider captured/retained its receiver; the existing conservative global
effect summary correctly rejected that readonly call. The fixture uses a distinct
nonretaining requirement for the intended positive case. Strict sanitizer tests
initially aborted under sandbox ptrace; leak checking then ran outside the sandbox with its protections enabled. A later compound-init
fixture exposed a stale sanitizer compiler object: its earlier compile finished
after a source edit and hid that edit from timestamp dependency checking. Only
the generated frontend object was removed and rebuilt from final bytes. A
premature rerun before that rebuild completed is retained as another failed
attempt; that field-core acceptance used san-verified-acceptance.log; later display acceptance uses display-san-final.log. All are development
attempts, not successful acceptance counts.

ASan/UBSan cover the C++ compiler and Runtime; emitted application machine code
is not instrumented to the same extent. No whole Stage 10 consolidated suite or
native Windows/macOS acceptance has run.
Native obligations remain exactly those in TEST_PLAN.md: Windows Debug/Release
wide-console and redirected UTF-8, cancellation ownership/restoration, native AOT
packaging and time-zone parity; macOS terminal/SIGPIPE/zone/packaging behavior.
Those features must first be implemented. Dependency feasibility has not exposed
a genuine architecture blocker or justified any feature omission.

## All 44 roadmap obligations

“Implemented / focused” is not final system acceptance. No row below closes the
entire Stage 10 mission.

| Step | Current state |
|---|---|
| 1 | Approved decisions recorded in contracts and DECISIONS. |
| 2 | Focused baseline and toolchain captured. |
| 3 | Field AST/nominal metadata and lexical closure owner implemented; focused tests pass. |
| 4 | Shared field selection checks declaring owner, including promotion; acceptance remains partial. |
| 5 | Deep const projection implemented and focused copy/alias tests pass; full matrix open. |
| 6 | Direct own-init assignment implemented; ordinary helper/alias/derived writes reject. |
| 7 | Independent IR checks added; full malformed-IR matrix remains open. |
| 8 | Focused closures/generics/inheritance/composition/properties/interfaces/pattern cases implemented; complete cross-product open. |
| 9 | Field compatibility/metadata/LSP and focused Linux VS Code/Neovim/Vim checks pass; full field cross-product/final acceptance remains open. |
| 10 | Checked builder and shared numeric/layout/temporal parsing support collection and enum/result consumers; final integrated acceptance remains open. |
| 11 | Pinned Ryu and all integer native/source rendering integrated, including collection leaves and ordinary custom hooks; final acceptance open. |
| 12 | Native/source primitive numeric patterns, result errors and literal interpolation diagnostics implemented; full acceptance open. |
| 13 | Primitive, nominal/custom, temporal, collection and enum/result runtime defaults implemented; focused Linux accepted, remaining cross-system/final acceptance open. |
| 14 | Ordinary readonly custom hooks, effects, roots and recursion guards implemented; 77 focused checks; full cross-product acceptance open. |
| 15 | Constant primitive conversion, formatting and interpolation share Runtime numeric kernel; full display types open. |
| 16 | Typed interpolation supports primitive, nominal/custom, temporal, collection and enum/result values/alignment; focused compiler-owned console/composite tooling passes; final cross-system/stage acceptance remains open. |
| 17 | Triple strings, multiline holes/comments, escapes and source spans implemented; focused Linux compiler/LSP/editor acceptance passes, final-stage/platform acceptance open. |
| 18 | All current display families, including enum/results, use ordinary typed calls, roots and checked builders/failure cleanup; later cross-system/final acceptance open. |
| 19 | Validated source date/time/local/instant/offset/duration/zone/zoned values and factories implemented; focused acceptance only. |
| 20 | Checked wide seconds/fraction arithmetic, UTC/native resolution and per-Runtime monotonic context implemented; native Windows/macOS unverified. |
| 21 | Embedded IANA 2026e, canonical zones, explicit overlap choice and gap rejection integrated with source APIs; full historical/platform matrix open. |
| 22 | Shared temporal token grammar, defaults, alignment and literal diagnostics implemented; focused acceptance only. |
| 23 | Native Runtime sessions, shared borrowed process endpoints, isolated adapter/clock, NativeSafe admission/gates, cancellation and drain implemented. Source bindings supplied in Step 27. |
| 24 | Native checked transfers/progress/EOF/errors, single monotonic deadline, queued/readiness cancellation and retained entered-syscall lifetime implemented under Boss-approved best-effort contract; Linux Debug/strict 9/9 each. Native Windows/macOS unverified; engineering/step24-results.json. |
| 25 | Shared native queue/UTF-8 reads and immediate CR/CRLF ownership implemented and focused-tested. Step 27 binds source read methods to this queue; native Windows/macOS acceptance remains open. |
| 26 | Confirmed-console ReadConsoleW/WriteConsoleW and strict transcoding through the existing queue/worker implemented; focused Linux helper/adapter acceptance passed. Native Windows compilation/execution UNVERIFIED. |
| 27 | Public heterogeneous print, source borrowed endpoints and recoverable reads/writes/flush/classification implemented. Focused Linux Debug/strict JIT/O2/linked-AOT and native error evidence recorded in step27-results.json; collection and enum/result display added; final/platform acceptance open. |
| 28 | Explicit prompt visibility, unbuffered drain, normal/nonzero exit, reached cleanup, primary/secondary failures and source/Runtime shutdown integration implemented and Linux focused-tested; engineering/step28-results.json. Native-platform/final-stage acceptance remains open. |
| 29 | Source capability snapshots and size/cursor/color/visibility/reset/clear/title/bell implemented through existing native/IR/ABI paths. Linux Debug/strict source/native/PTY acceptance passes; native Windows/macOS and final-stage acceptance remain open. |
| 30 | Implemented; focused Linux Debug/strict/native PTY/JIT/O2/linked-AOT accepted. Native Windows/macOS and final-stage acceptance open. |
| 31 | Shared console/mode/input/output/GC/shutdown/code-generation integration verified in focused Linux Debug and strict sanitizers; actual PTYs, normal/O2 JIT, linked AOT and native production builds pass. Native-platform/final-stage acceptance open. |
| 32 | Owner-contained declaration metadata/parser and frontend/independent IR validation implemented; 278 focused declaration checks pass; Debug 8/8 and strict sanitizer 3/3. Step 33 now supplies source selection. |
| 33 | Shared exact direct-owner selection, access/ambiguity diagnostics, generic/nullable handling, selected references and required ordinary-call plumbing implemented; focused acceptance recorded below. |
| 34 | Ordinary-call integration and derived != implemented; focused GC/snapshots/RHS-await, worker effects, checked/owned failures, reached cleanup and source-frame tests pass in normal/O2 JIT and linked AOT. Final-stage/native-platform acceptance remains separate. |
| 35 | Captured-place custom +=, -=, *=, /= and %= implemented across locals, fields, properties, fixed/growable arrays and dictionary values, with generic/mixed operands, ordinary result conversions, effects/roots and guarded commits. Focused acceptance in engineering/step35-results.json; native-platform/full-stage acceptance remains separate. |
| 36 | Recursive direct-owner/custom-equality key exclusion implemented across source, independent IR and data-only Runtime metadata/schema admission; 131 focused checks, Debug 13/13 and strict 13/13 pass. Fixed arrays remain outside the existing eligible key domain. Final-stage/native-platform acceptance remains separate. |
| 37 | Exact operator sets/owners/bindings/effects and format specifications/pinned policy identities integrated in existing contracts, invalidation, reload and cache. Final verified-source Linux Debug/strict 14/14 each; earlier unexplained observations retained. Independent/platform/final-stage acceptance separate. |
| 38 | Compiler-owned operator/field/hook/interpolation/format/temporal plus console/collection/enum/result intelligence has focused Linux Debug/strict acceptance and actual VS Code/Neovim/Vim verification. Console/composite metadata106 and additional LSP237 checks pass; final Stage10/platform acceptance remains separate. |
| 39 | Existing-parser syntax gate and physical/expanded token preservation implement conservative formatting; compiler/LSP/native-editor diagnostics, literals, UTF-16 and dirty-revision regressions added. Focused Step 39 and subsequent console/composite tooling evidence below; final-stage acceptance remains open. |
| 40 | Earlier partial evidence preserved. Completed-system field/operator/display/temporal/console/composite scenarios now pass focused Linux Debug/strict25/25, normal/O2 JIT, independent AOT, production native build, real PTYs/partial progress, GC/failure/reload/cache and workloads. Independent Wrench review pending; this is not Step41 or full-stage acceptance. |
| 41 | Complete unfiltered Linux Debug199/199, fresh Release199/199 and strict199/199; unchanged1MiB frontend/depth gates and actual Linux editor hosts pass. Evidence below; Wrench review pending. |
| 42 | CI96 native Windows190/194 each and macOS build failures preserved; eight source/test/build changes focused-tested: Linux Debug18/18, strict18/18 and both1MiB probes. Repaired native source UNVERIFIED; Wrench review and Boss-controlled native reruns required. |
| 43 | Engineering progress and evidence recorded; final feature documentation/examples open. |
| 44 | Incomplete. Not a completed Stage 10 handoff and not independently Wrench-reviewed. |

## Every feature-completeness matrix row

| Capability | Current state |
|---|---|
| Strict UTF-8, NUL, scalar indexing, owned byte conversion | Existing source remains; Step 25 native shared input reuses UTF-8 validation and preserves NUL/BOM/raw bytes. Source bindings now use the same queue (Step 27); final integration remains open. |
| Named/default binding and written-order evaluation | Print pack and named-only controls implemented; all supplied values/controls capture once before display. Written-order side effects and value/reference snapshots verified. |
| Heterogeneous print | Implemented for primitive, nominal/custom, opaque resource, temporal and fixed-array/list/dictionary display. Enum/result defaults are now included; focused console/composite tooling acceptance passes; focused Step40 four-system integration passes; final-stage/platform acceptance remains separate. |
| No-newline output and prompts | Explicit print(end="", flush=true) followed by existing reads implemented and verified in Step 28, including redirection/EOF/errors. No convenience helper added. |
| Standard endpoints and byte/text access | Compiler-owned borrowed stdin/stdout/stderr, protected endpoint construction/payload, source recoverable write/write_bytes and all four read APIs implemented through Steps 23–26. |
| Character/text/line EOF/errors | Native/shared-queue and source read APIs implemented; focused source tests verify mixed CRLF/Unicode, EOF, invalid prefixes, partial bytes and invalid counts. Windows-wide feed implemented; native Windows verification open. |
| Redirection and Windows Unicode | Raw redirected bytes and source binary/NUL output verified on Linux; Step 26 wide paths implemented with source io_error retaining partial UTF-16 units. Native Windows/macOS acceptance open; Stage 9 corrections preserved. |
| Terminal input and presentation | Steps29–30 presentation, keys, availability and scoped modes implemented with focused Linux acceptance through one queue. Native Windows/macOS and final-stage acceptance remain open. |
| Concurrent output / normal exit | Steps28–31 normal/nonzero exit, reached cleanup, record serialization, shared Runtime/mode/queue ownership, GC, entered-call cancellation, code leases and restoration pass focused Linux verification. Native-platform and final-stage acceptance remain open. |
| Complete interpolation | Primitive, nominal/custom, temporal and collection typed holes/alignment plus multiline/triple parsing implemented; enum/result defaults now included; focused console/composite tooling acceptance passes; focused Step40 four-system integration passes; final-stage/platform acceptance remains separate. |
| Numeric/bool conversion | Primitive source/default/constant conversion and formatting implemented with focused tests. |
| Numeric formatting | Native/source APIs, constant evaluation and interpolation share implementation; focused four-system integration passes, consolidated/final acceptance open. |
| Deterministic rounding / special floats | Native tests pass; full shared-consumer acceptance open. |
| Temporal values and formatting | Native kernel/source APIs/hooks/clocks implemented with focused JIT/O2/AOT tests; native Windows/macOS and full-stage acceptance open. |
| Custom display / format hooks | Nominal ordinary-call implementation and 77 focused checks; reload and LSP tests pass; complete cross-product acceptance open. |
| Private fields | Core implementation and focused source/IR/tooling tests; full acceptance open. |
| Readonly fields | Core implementation and focused source/IR/tooling tests; full acceptance open. |
| Copies, readonly views, initialization | Focused old/new tests; full Stage 10 cross-product open. |
| Inheritance/composition/interfaces/generics | Focused field and Step40 inherited/const-promoted/generic composite cases pass; full final-stage matrix remains open. |
| Owner arithmetic/equality/order | Steps 32–34 declaration/selection/ordinary calls, including %, GC/worker/failure paths and derived != pass focused normal/O2 JIT and linked AOT. Steps 35–36 compounds and key exclusion and Step 38 tooling are implemented; final acceptance remains open. |
| Derived compounds | Custom arithmetic and %= use captured old-value/RHS calls before guarded commit; builtin behavior retained. Focused Linux tests cover counts, rebindings, readonly authority, GC/await, failures and collection invalidation. |
| Dictionary equality/hash coherence | Recursive direct-owner custom-equality key exclusion implemented in shared source/IR eligibility and data-only Runtime metadata; values and pattern comparisons preserved. Focused Linux acceptance passes; final-stage/platform acceptance open. |
| Compiler-owned LSP / editor integrations | Compiler-owned operator/field/format/hook/temporal/console/composite intelligence passes focused metadata/LSP and actual Linux VS Code/Neovim/Vim checks. Installed source/readonly and dirty/stale boundaries preserved. Final-stage/native-platform acceptance remains open. |
| Reload/cache/JIT/AOT | Step 37 operator/format declaration, exact binding/specification/policy dependencies implemented; body-compatible leases and Debug cache paths pass focused Linux/JIT/O2/AOT verification. Earlier unproven observations retained; native/final-stage acceptance open. |
| Git package manager | Separate, unmodified, outside Stage 10. |

## Continuing the already-authorized mission

Current bounded Step41 consolidated Linux acceptance is complete and stops for
Golden Wrench review. Exact evidence: engineering/step41-results.json. Step42
handles actual native Windows/macOS acceptance and unavailable platform gates;
Steps43–44 and the Boss release decision remain open. Stage10 INCOMPLETE.

## Deferred editor quality observations — Boss report, Step 31

**RECORDED ONLY. Not authorized for implementation in Step 31.** These are actual
observations in a real VOID project in VS Code on Linux. No cause is established;
a successful `voidc run .` does not prove that the editor analyzed the same snapshot.
Preserve this backlog in future handoffs; it is separate from Step37's unexplained
publication/standard-library observations.

- F12: executable `var msg = wait_message("What is your name?")` followed by
  `match msg:` reports **"No definition found for 'msg'."** Definition navigation
  for `wait_message` is also missing. This is an observed editor-navigation failure,
  not a proven compiler defect.
- F2 on `msg` or `wait_message` reports **"The element can't be renamed."** VOID
  already has guarded rename for a restricted symbol set; VS Code/Neovim use
  version-checked edits, and traditional Vim intentionally disables unsafe rename.
  Investigate compiler-owned symbol identity, analysis success, current overlays,
  eligibility and LSP capability negotiation. Do not remove safety checks or use
  text replacement. Boss separately proposes expanding rename to methods/fields,
  subject to architecture review; this is not implemented or approved here.
- Ordinary function hover is missing although typing `(` produces signature help
  and `.` completion. Their analysis paths differ. Future tests must inspect actual
  `textDocument/hover` responses and distinguish hover from suggestion/completion
  commands; no root cause is claimed.
- Signature help exposes raw `@param msg The message displayed before requesting
  input.` Desired presentation keeps general `##` description in general docs,
  attaches parameter documentation to the matching active parameter, and hides raw
  tags from general function documentation. Reuse existing compiler documentation
  extraction and parameter-help infrastructure; no second parser.
- Raw `@return` tags also appear. A separate **Returns** section in function hover
  is a proposed presentation improvement, not implemented functionality.

Future bounded regression: use local `msg` and function `wait_message` declarations
and all references/call sites in the same current source snapshot for F12/F2/hover;
saved and unsaved changes; valid/invalid project analysis; imported/cross-file
functions; conflicts and stale rename rejection; parameter/return documentation;
real VS Code/Neovim and intentional Vim restrictions. Inspect shared ToolingIndex,
compiler declarations/references and snapshots before proposing a design. Reproduce
before claiming a cause. No voidls, editor adapter, rename or doc-parser repair is
part of this console increment.

### Step31 engineering in progress — 23 October 2026

Extending existing native console, real PTY and source/JIT lifecycle harnesses for
shared Runtime record gates, scoped input conflicts, exact queue ownership, GC,
entered-call cancellation and code-generation retention. Preliminary native tests
pass; acceptance and handoff are pending. A new PTY fixture initially expected
literal CRLF while the host ICRNL mapped CR to LF; explicitly configuring the host
PTY input translation before capturing its original state corrects the fixture,
without changing Runtime mode policy. A mistyped build target was corrected; no
production defect is claimed from either development failure.

Development source run exercised the generation and shutdown assertions, then failed
with `runtime is closing`: the new test incorrectly tried to attach a fresh GC
participant after explicit Runtime close. Removed that invalid host attachment;
pre-close forced collections and post-join thread/drain checks remain. This is a
fixture correction, not a Runtime admission defect. The corrected PTY integration
passes 99 checks/9 processes including three real shared-stream Runtime contexts.


### Step31 focused handoff — 23 October 2026

Completed applicable console concurrency/lifetime acceptance in the existing native,
source and PTY harnesses. No production-code change, new dependency, ABI, queue,
registry, failure channel, scheduler or GC/backend was needed by these tests.
Two shared Runtime contexts preserve 256 output/control records through one-byte
native writes. Barrier-controlled entered key I/O combines nested ownership,
foreign readers/queries, independent close/output, forced GC and same-queue CRLF
recovery in a later Runtime. Actual PTY uses three Runtime contexts, concurrent
redirected Unicode output and exact saved/restored termios attributes.

Normal/O2 source calls retain old compiled code across compatible publication and
GC until reached cleanup and native completion; the next call executes the new
body. Combined shutdown preserves source native87/cleanup19/completed2, associates
reached cleanup errors with that primary, returns native restoration89 to its close
caller and supports successful retry. Host joins source work before destruction;
uncancellable borrowed syscalls may still block close. Current headers/archive
are rebuilt and production native executables execute successfully.

The initial 28-suite passes are followed by a final native-only 1/1 rerun in each
configuration: one new GC-while-closing assertion, 347 total checks, no production
changes or unaffected executable changes. Both freezes and supplemental logs are retained.

Focused Debug 28/28 (341.90s), strict ASan/UBSan/leak 28/28
(352.20s); 3234 source assertions, 347 native foundation checks,
127 key/mode checks, 99 real key PTY checks/9 processes and 72 exact JIT/O2/AOT
records per configuration. Build/test commands, full logs, output bytes, preserved
fixture failures, manifests and limits: engineering/step31-results.json.

Boss's real VS Code observations are permanently recorded in the deferred section
and linked from TEST_PLAN; no root cause or editor repair is claimed. Previous
Step37 unexplained observations are unchanged. Native Windows/macOS, Release,
composite display and later integration/final-stage acceptance remain open.
No Git mutation, hosted CI, publication or public root README change.

Next concrete source change: Remaining composite display (Steps13/16/18): extend src/frontend/compiler.cpp conversion dispatch at current unsupported-collection/enum checks and existing generated display adapters to typed container traversal/shared builder, preserving readonly, effects, roots and recursion handling. Separate later editor-quality authorization; no editor repair in Step31.


## Collection display increment — engineering checkpoint, 23 October 2026

Implementing fixed-array, growable-list and dictionary default display. The existing
conversion/interpolation/print dispatch now selects an ordinary generated adapter.
One explicit managed work/tag stack and per-concrete-type owner/cursor/version
stacks drive iterative traversal; dictionary entries snapshot both typed operands
before converting keys. Existing array/dictionary operations, version checks, const
paths, copies, ordinary leaf hooks/effects and precise roots remain authoritative.
A private per-call native context owns the shared checked Builder and active identity
path; nested calls inherit that path, and all normal/checked failure exits restore
it. Shared references repeat; active cycles emit <cycle>. No source-storable context,
reflection, boxing, metadata callbacks, dependency, new iterator/GC/backend/registry.

Initial Debug smoke: all four conversions, nested values, insertion order, Unicode/
NUL, alignment, actual cycles/shared references and a 1050624-byte result work.
Existing source console plus new GC/allocation/invalidation checks passed 3674
assertions before final source acceptance. Development exposed an invalid literal
emitted for a fixed-array no-op check; it now emits no operation. A generated helper
inside an owner hook initially failed lexical-owner validation; using the existing
$display$ identity/provenance contract repairs it without weakening that validator.
Fixture corrections: escape literal braces in VOID expected strings; use existing
make_date factory; do not admit already-prohibited fixed-array dictionary keys.
The first compiler build also caught a heterogeneous C++ initializer list in the
new helper; corrected without changing semantics.

Focused acceptance, cache/reload, independent linked AOT and strict sanitizers pass;
exact final evidence is recorded below and in engineering/collections-results.json.
Native Windows/macOS and final Stage10 acceptance remain unverified.
Enum/result display is the next bounded composite increment, not implemented here.
Boss deferred editor observations and Step37 observations remain unchanged.

### Deferred future compiler usability/optimization — Boss direction, 23 October 2026

**Recorded only; separately scheduled after Stage10 completeness unless Boss changes
priority.** No new stage number; this does not replace the current collection mission.
The permanent [development strategy](../DEVELOPMENT_STRATEGY.md#future-compiler-usability-and-optimization--boss-direction-23-october-2026)
records all three approved design directions:

- Automatic Debug caching for voidc run using Stage5/LLVM19.1.7/ORC and current
  source validation; future --no-cache/--release behavior, exact identities, safe
  corrupt/unsafe/unavailable fallback, existing --cache/--no-opt compatibility,
  clear conflicts and opt-in diagnostics. Preserve Windows fallback until verified.
- Smaller native executables: Boss's actual embedded IANA commentary/symbol/path/
  diagnostics/debug observations; audit retention, compact only non-semantic embedded
  data while preserving rules/directives/licenses, evaluate safe release stripping,
  keep intentional failure/crash diagnostics and independent AOT. No mandatory OS
  zones, sidecar or download. Measure size and launch/compile/link times.
- Finer incremental compilation is research only, contingent on real benchmark
  evidence that whole-project recompilation remains a bottleneck; reuse existing
  identities/dependencies/metadata/leases/reload. No speculative second pipeline,
  graph, per-function cache or lazy compilation without demonstrated justification.

No implementation, Git action, hosted CI or publication was authorized by this note.
The existing F12/F2/hover/@param/@return backlog and Step37 observations are preserved.

Deep 72-level inferred collection regression (no hook recursion) exposed high Debug
CPU cost in independent non-null initialization validation. A sampled stack is
retained in engineering/collections-deep-development-stack-2.txt (the first debugger
attempt lacked symbols because of the runner PID namespace). The proof repeatedly
projects every descendant even after a non-null root has already made the slot
required. Applied narrowly equivalent repair: compute the existing reference paths
once, mark such roots required immediately, then retain the full descendant proof
for all other slots. The added forged uninitialized collection-load test retains
the initialization rejection. Readonly/GC checks remain enabled. This is current
collection integration support, not any
of the separately deferred cache/incremental compiler projects. Final acceptance
uses the repaired frozen source; the slow development probe is not acceptance.

The unchanged strict C++ conversion test compilation initially crashed pinned
Clang19.1.7 in BDCE/DemandedBits while optimizing a libstdc++ string guard. The
exact same source/flags succeeded on retry. Both logs and the preprocessed source/
command reproducer are retained; no causal explanation, compiler repair or fixed
Step37 observation is claimed. No optimization/sanitizer checks or compiler version
were changed. The first proof fast path still left high deep-type CPU cost in
recursive capability assignment; a second sampled stack is retained. Identical
immutable descriptor/authority pairs now return the reflexive assignment result
immediately. Non-identical pairs retain the existing complete proof. The initial
slow probe was deliberately terminated (CTest failure records retained), not counted
as acceptance. The full unchanged 72-level test and forged initialization test
passed final focused verification.

### 23 October 2026 — Collection display focused acceptance

Fixed arrays, growable lists and dictionaries now share typed default display through
string, format, interpolation and print. One ordinary generated iterative helper
uses managed continuation/owner/cursor/version stacks and typed dictionary entry
snapshots; the existing checked Builder and private active-path scope preserve
cycles versus shared references, readonly/copies, hooks/effects, precise roots,
version failures, complete-record publication and ordinary cleanup. No new renderer,
GC/backend, metadata callbacks, runtime registry or dependency. Formatter policy is
void-format-v3;collections-v1; pinned dependencies/licenses are unchanged.

Final focused Linux Debug **20/20 (840.04s)** and strict ASan/UBSan/leak
**20/20 (993.45s)** pass. Each configuration records 156 conversion checks
(including 72-level inferred collection normal/O2 execution and forged-IR rejection),
3674 source console assertions, 199 display dependency/reload checks, 404 source
driver checks, 84 exact normal/O2 JIT/independently linked AOT records, 347 native
foundation checks, 127 key/mode checks and 99 real PTY key checks/9 processes.
Production native builds/run and five actual collection cache cold/hit/corrupt/
hook-body/specification checks pass in both configurations. Exact output/provenance,
key/value snapshot mutation, 1050624-byte output, GC/allocation failure/retry,
readonly generic hooks, cycles/shared references and incompatible reloads are covered.

Deep compilation exposed redundant independent initialization/capability proof work;
equivalent fast paths retain non-identical authority checks and the unchanged deep
regression. Its Debug test takes 519.10s and strict test 640.91s; no performance
threshold or broad optimization claim. Deliberately terminated development probes
are separate from acceptance. A pinned Clang BDCE/DemandedBits test-build crash
succeeded on unchanged-source/flag retry; reproducer retained, cause unexplained.
Earlier Step37 observations remain preserved, not repaired.

Evidence: [collection results](engineering/collections-results.json). Native Windows/macOS, fresh
Release/editor collection cross-products, enum/result display and complete four-system
final Stage10 acceptance remain open. This bounded increment stops for Wrench review.
Next concrete source change: enum/result default display in the existing
src/frontend/display_semantics.inc and compiler.cpp conversion/generated adapters.
Deferred compiler cache/native-size/incremental research and editor observations
remain recorded only. No Git mutation, hosted CI, publication or public README edit.

## Enum/result display increment — 23 October 2026, engineering checkpoint

Extended the existing generated collection work-stack helper to typed enum/result
continuations. Only the selected variant's payloads use existing checked sum_get
operations in declared order. Qualified enum cases and bare Ok/Err use the same
checked builder/context, active collection path, roots, failures and formatting
wrappers as collections. Invalid tags fail through existing panic; no new IR/Runtime
operation, renderer, reflection, callbacks, GC/backend or dependency. Formatter
policy becomes void-format-v4;collections-v1;sum-display-v1.

A recursive enum/list shared-reference fixture reproduced a frontend false
readonly rejection. The shared authority predicate already accepts equivalent
conservatively normalized paths; compatible() now uses that result instead of
subsequently rejecting raw unfolded spellings. Mutable escapes remain rejected;
independent IR authority validation is unchanged. Targeted sum tests pass 45
checks, including Ok(null), generic payloads, inactive cases, cycles/shared refs,
value copies, const mutation/escape rejection and malformed IR. Reload tests pass
270 checks in development. Final frozen Debug/strict acceptance passes; see the focused handoff below.

Fixture corrections: event is an existing reserved word; C++ raw strings with
payload-ending parentheses need a custom delimiter; readonly results may legally
return inferred readonly aliases, so the negative test must attempt a mutation
through the returned alias. No valid return rule was changed.

Preserve collections-results.json, its deep compilation cost and transient Clang
reproducer/status, Step37 unexplained observations, deferred compiler objectives
and editor backlog. Native Windows/macOS, fresh Release/editor and four-system/
final Stage10 acceptance remain open. Stage10 remains INCOMPLETE.

## Enum/result display focused handoff — 23 October 2026

Enum defaults now display qualified type.case with declared-order payloads, and
results display Ok(value)/Err(error), including Ok(null). string, format, typed
interpolation and heterogeneous print share one generated typed work stack with
collections, existing checked payload selection, builder/context, roots and failure
cleanup. Recursive sum/container cycles are iterative; shared references repeat;
nominal leaves keep existing ordinary custom/opaque behavior and private policy.
Universal scalar alignment works; literal unsupported patterns diagnose and dynamic
patterns return Err. No new renderer, IR/Runtime ABI, callbacks, GC/backend or dependency.

The recursive enum/list fixture exposed a frontend raw-path readonly false rejection.
Admission now honors the existing conservative normalized authority predicate;
nonassignable writes/escapes and independent IR validation remain checked. Tests
cover readonly pattern captures/arguments/returned aliases, independent mutable
aliases, fixed copies, real managed payload GC, callback order/counts, active-only
selection, malformed tags/layout/access and checked wrong-variant failure. Native
allocation sweeps, retries, nested failed hooks/defers, structural invalidation and
no outer partial record pass. Existing legal inferred readonly returns are preserved.

Focused Debug **24/24 (994.99s)** and strict ASan/UBSan/leak
**24/24 (1137.49s)** pass: 203 conversion checks (45 new
sum-only development checks), 4123 source console assertions,
270 display dependency/reload checks and 438 driver checks per
configuration. Exact output/failure records cover **92 normal/O2 JIT/independently
linked AOT executions** per configuration. Matching production native builds and
six cache cold/hit/corrupt/hook-body/specification/variant-name probes also pass.
Four additional exact JIT/O2/linked-AOT records per configuration prove that copied
results retain managed object identity while mutable/readonly aliases coexist.
Pattern/collection/dictionary/generic and existing numeric/temporal/field/hook/GC/
console/LSP/reload neighbors are retained. The 72-level regression remains unchanged;
its current conversion suite cost and historical collection cost are recorded.

Policy advances to void-format-v4;collections-v1;sum-display-v1; existing variant/
layout contracts, selected ordinary hook edges and exact compiler/Runtime/pinned
dependency identities protect reload/cache behavior. No dependency/license changes.
The earlier transient pinned Clang crash/reproducer, Step37 observations, deferred
compiler objectives and F12/F2/hover/documentation issues remain preserved, unresolved
where previously unresolved. Fixture corrections and exact commands/hashes/results:
[enum/result evidence](engineering/sum-display-results.json).

Stage10 remains **INCOMPLETE**. Native Windows/macOS, fresh Release/editor console/
composite cross-products, four-system and final-stage acceptance remain open. This
bounded increment stops for Golden Wrench review. Next concrete source work: extend
existing tests/stage10/tooling.cpp and lsp_display.py console/enum/result/container
fixtures, then repair demonstrated gaps in the existing compiler EditorProbe/metadata
and ToolingIndex/voidls paths before four-system integration. No new editor parser.
No Git mutation, hosted CI, publication or public root README modification.

## Console/composite tooling — 23 October 2026, engineering checkpoint

Continuing the accepted enum/result implementation, not restarting display or
console engineering. Compiler metadata probes reproduced missing enum-case menus,
payload-pattern references and print parameter help/public signature. Existing
EditorProbe and declaration/reference records now carry actual case payload names,
source docs, checked pattern variant tokens and concrete builtin result metadata.
Dirty enum calls visit written typed payloads before arity rejection so existing
signature/completion recovery works. Console sessions expose ordinary close()
completion while native payloads remain hidden. Source console documentation uses
existing ## metadata; no Runtime, GC, LLVM, native adapter or ABI changes.

Expanded tests/stage10/tooling.cpp and lsp_display.py cover cross-file enum payloads,
results, readonly captures, collection/nullable/generic views, all four consumers,
shared alignment validators, Unicode/CRLF/nested holes, private/installed boundaries,
invalid/dirty/stale/refresh/close/reopen paths and hidden generated helpers.
Real editor harnesses reuse the shipped VS Code, Neovim and Vim paths. Acceptance
is in progress; final exact counts/hashes and limitations follow in the handoff.

The Boss-approved [future debugging direction](../DEVELOPMENT_STRATEGY.md#future-source-debugging-and-breakpoints--boss-direction-23-october-2026)
is recorded only: native source debugging first, then JIT generation/lease/GC/async/
cache integration, and established DAP clients separate from voidls LSP. Actual
breakpoint/step/value/platform acceptance is required when separately scheduled;
DIBuilder alone proves no debugger. No stage number or debugger implementation.
All earlier deferred objectives, editor quality observations, Clang reproducer,
deep-collection cost and Step37 unexplained observations remain preserved.
Stage10 is INCOMPLETE; next mission remains four-system integration/acceptance.

## Console/composite tooling focused handoff — 23 October 2026

Completed the bounded compiler-owned console/composite intelligence increment.
Enum cases expose actual payload names/docs, typed incomplete-call help and genuine
cross-file constructor/pattern definitions/references. Result Ok/Err metadata and
contextual completion retain concrete generic/readonly capabilities without inventing
source declarations. Public print help exposes *values/named controls; console source
docs, session close completion and nonpublic installed-symbol filtering use existing
metadata. Shared format validators/layout suggestions, original UTF16/CRLF/nested
hole spans and dirty/stale/repair/refresh/close/reopen boundaries are covered.
No hook, clock or I/O executes to answer editor requests.

Final frozen-source Debug **29/29 (392.38s)** and strict ASan/UBSan/leak
**29/29 (429.36s)** pass, plus **45 focused sum checks per configuration**.
Each records 106 tooling metadata checks, 237 additional console/composite LSP
checks, retained earlier LSP/formatting checks, 4123 source console assertions,
438 console-driver checks and 92 exact normal/O2 JIT/independently linked AOT
records. Source display linked-AOT driver retains 16 checks; display reload passes
275 and operator/format compatibility/cache/reload 248. Runtime/GC/native console/
LLVM/IR operations and dependency versions/licenses are unchanged. Console source
changes are documentation only; stripped bytes equal the previous accepted snapshot.

Actual VS Code1.143.0 passes trusted/restricted/multi-root profiles and its existing
unit test; Neovim0.13.0-dev and actual Vim9.2 patches1–858 pass shipped adapters with
automatic/explicit entry plus five real PTY automatic-menu prefixes each. Initial
VS Code fixture ordering/rendered-space mistakes and a sandbox Unix-socket refusal
are retained separately from final acceptance; no product check was disabled.
The earlier Clang reproducer/status and Step37 observations remain unresolved.
The unchanged 72-level regression and prior cost/evidence are preserved, not rerun
in this tooling-only increment. Broad F12/F2/hover/@param/@return work remains deferred.

Future native debugging, then JIT and DAP milestones are **documentation only** in
DEVELOPMENT_STRATEGY.md; no stage number or debugger implementation. Native Windows/
macOS, fresh Release and full four-system/final acceptance remain open. Stage10 is
**INCOMPLETE**. This increment stops for Golden Wrench review.

Exact commands, source hashes, editor/LSP responses and failures:
[console/composite tooling evidence](engineering/console-tooling-results.json).
Next concrete source work: extend tests/stage10/operator_integration.cpp and its
native fixtures with completed console, collection and enum/result consumers for
remaining four-system integration, then complete the outstanding acceptance matrix.
No Git mutation, hosted CI, publication or public root README modification.

## Step40 final integration checkpoint — 23 October 2026

Extending the existing operator integration and source console harnesses with one
shared generic private/const model, legacy/application wrappers, composite temporal/
zone output, captured property compounds, exact side-effect traces, readonly/value/
managed aliases, failed hooks/operators, active cycles, structural invalidation,
scoped key modes and native partial progress. Existing Fake/probes/barriers provide
GC/allocation/reload/shutdown evidence. No production architecture/dependency changes.
Development and focused acceptance are in progress; final evidence follows.

Boss reports that **F2 rename works in a real VS Code scenario**. This updates the
observed scope only: no claim that every rename target is supported, no general
rename expansion and no closure of the deferred F12/F2/hover/@param/@return backlog.
Tooling evidence's inherited deep-test wording is corrected: the 72-level regression
was preserved, not rerun during tooling-only acceptance. Original JSON is retained.
Automatic Debug cache UX, native-size/IANA compaction, measured incremental research
and native/JIT/DAP debugging directions remain documentation-only deferred work.
Step37 observations and the transient Clang19.1.7 reproducer remain unresolved.

Step40 development recovery: shared source model is
`tests/stage10/operator_console_model.void`; legacy/application wrappers and
partial-pipe/mode/workload fixtures use it or the existing operator_systems library.
`console.cpp::four_systems` reuses Fake, managed probes and existing barriers for
snapshots, failures, allocation retries, generation replacement and closing I/O.
Developer-only --four-system passed 337 checks before adding the final pure-hook/
clock-effect compatibility candidate; full final CTest remains required.
`operator_integration.cpp::four_system_policy` passes 26 development checks with
private/readonly/key/format/exact-operator/worker/construction/forged-target negatives.
The existing console.py driver now exercises exact normal/O2 JIT/linked-AOT output,
live prompts, normal/failure PTY restoration, real partial pipe progress, production
native build, combined cold/hit/corrupt/body/spec cache and composite workloads.
Its first expanded development run is pending; no final acceptance claim yet.

A Debug-only development probe was terminated after a symbolized stack showed the
test unwinding while its I/O barrier remained held: it called console_context()
after Runtime::close correctly denied new admission. Capture the existing context
before closing and release the barrier before asserting. No Runtime protection was
changed. Raw first and symbolized second stacks are retained. A missing test include,
invalid fixture construction/propagation forms and diagnostic spelling expectations
were corrected to existing contracts. Native/GC/LLVM production code is unchanged.
Existing test deadlines expand only for the added scenarios (source console600s,
process/JIT/AOT console900s); no assertions or sanitizer checks are disabled.

Development process-driver budget evidence: the first expanded Debug run reached
900s during the normal scoped-key PTY phase, with completed object/executable
artifacts proving continuing progress. It timed out, not passed. The added mode-
failure, positive pipe-progress and cache probes still followed it. The test budget
is therefore 1800s for this expanded driver (source C++ console600s). No semantic
check is removed. An early strict run using the old900s budget was deliberately
terminated before completion and restarted from the final source freeze; neither
preliminary run is counted as acceptance. Final results will record actual durations.

Final Step40 acceptance recovery: frozen code/test manifest is
engineering/step40-final-code-freeze.json; verified library snapshot is
step40-final-stdlib-snapshot.json. `/tmp/step40-final-accept.py debug` and `san`
run 25 selected suites at CTest -j2, then focused sum45, native formatter workloads
and Debug1MiB depth probes. Exact commands/environment/timings are recorded as each
phase completes in step40-final-{debug,san}-commands.json. Matching build/test/detail,
console output and operator-workload records use the same prefix. Current run is
pending; source production files remain unchanged. This is focused Step40 evidence,
not Step41 consolidated acceptance. Stop for Wrench review after final result JSON.

## Step40 stack/proof repair checkpoint — 23 October 2026

The first expanded Debug/strict focused runs each passed25/25, sum45 and native
workloads; the subsequent unchanged Debug1MiB semantic-depth check exposed a real
frontend stack regression. These before-repair runs and source freeze remain in
step40-final-before-stack-*; they are not final-source acceptance.

GDB located legal124-receiver overflow in the recursive metadata visitor: a new
pattern-reference Expr temporary reserved space even for expressions without a
pattern. Move that cold work through the established frontend_case helper. This
restored legal chains, but400-receiver guard/error recovery still overflowed while
unwinding through member selection. Borrowing the cold editor name-probe expected
optional<Type> by const reference removes its caller's reserved copy storage.
The first reference-only attempt did not fix the independent metadata frame defect.
Both changes together pass the unchanged1MiB legal/deep receiver, numeric, string
index and type-parser/diagnostic-recovery probes. No nesting limits, stack budget,
assertions, diagnostic expectations or compiler optimization were changed.

Exact reproductions and failed/intermediate proof logs use
step40-final-stack-*; proof-3 passes. Only src/frontend/compiler.cpp required a
production repair, using existing cold-handler infrastructure. Final frozen-source
Debug/strict suites subsequently passed as recorded below. Previous source hashes
and all prior Clang/Step37/deep-conversion observations remain preserved.

## Step40 final four-system focused handoff — 23 October 2026

The historical step40-results.json remains the earlier partial-system checkpoint.
The new completed-system integration uses one shared private/const generic model and
small legacy/application/mode/error/workload wrappers in the existing operator and
console harnesses. Exact traces prove captured property owner/getter/RHS/operator/
setter and argument/control/display order, derived inequality once, independent
value copies and managed identity through readonly arrays/dictionaries/enums/results.
Primitive/custom/numeric/date/duration/explicit-zone text reaches real console output.

Final frozen-source Debug **25/25 (1161.93 s)** and strict ASan/UBSan/leak
**25/25 (1216.13 s)** pass, plus focused **45 sum-display checks each**, native formatter workloads
and unchanged Debug **1 MiB** proof/depth probes. Each configuration records
4463 source-console checks, 1169 operator GC/effect/failure checks (including 26
new policy negatives/proof checks), 616 console-driver checks and 120 exact
console execution records. Seven new main/PTY cases each agree across all four
normal/O2 JIT/independently linked AOT modes; four additional live integrated prompts
and four real positive-progress pipe probes pass. Normal/failure PTY modes restore
exactly; stdout/stderr, primary/cleanup diagnostics and nonzero application status
versus legacy result output are checked. Production native build/startup and the
combined cold/hit/corrupt/hook-body/spec cache cases pass in both configurations.

Forced GC, allocation/retry, no partial prewrite record, failed compound commit,
structural invalidation, recursive hooks/active cycles, entered native failure,
admitted shutdown and retained-generation replacement use existing probes/barriers.
Changed layout/visibility/operator sets/specifications/clock effects are rejected by
existing compatibility contracts. The unchanged 1 MiB probe exposed two frontend
stack-frame regressions. Cold pattern
metadata now uses the existing frontend_case helper; the name probe borrows its
expected optional<Type> instead of reserving a copy in every recursive frame.
Legal 124-call chains and 400-call diagnostic recovery now pass with unchanged limits.
Only src/frontend/compiler.cpp required production repair; other production/
Runtime/GC/backend/stdlib/dependency files are unchanged. Existing test harnesses,
two expanded CTest deadlines and internal records also changed. Runtime admission
after close was correct; the development test was repaired to retain its context
before closing. The first 900 s driver timeout and preliminary strict termination
are not acceptance; final 1800 s process/source 600 s deadlines retain every
assertion and sanitizer check.

New 2048-record actual composite stdout workloads, production native startup and
prior numeric/temporal workloads record exact bytes/hashes/times and available
JIT process RSS. These are observations with concurrent local checks, no thresholds
or optimization claim. The unchanged 72-level test/cost and earlier Clang reproducer
are preserved; the deep test was not rerun in tooling or this increment. Cold-path
frame fixes are covered by the unchanged 1 MiB semantic-depth regression and
final-source metadata/LSP/display checks.
Step37 observations remain unexplained. Tooling/Step38/matrix status and the Boss's
scoped real VS Code F2 success are reconciled; the broader editor-quality backlog
and all future compiler/native-size/incremental/native/JIT/DAP objectives stay deferred.

Stage10 remains **INCOMPLETE**. This bounded increment stops for Golden Wrench review;
Step41 consolidated Linux Debug/Release-relevant/strict acceptance is the next separately
reviewed increment, followed by actual native Windows/macOS and final-stage obligations.
Exact commands, hashes, outputs, workloads, failures and boundaries:
[Step40 final integration evidence](engineering/step40-final-integration-results.json).
No Git mutation, hosted CI, publication or public root README modification.

Next concrete work after this review is Step41: reconcile the existing CMake/CTest
targets with TEST_PLAN.md's consolidated Linux acceptance list, build the current
Debug/Release and strict configurations, and record that separately scoped evidence.
No remaining Stage10 step or native-platform acceptance is closed by this handoff.

## Step41 consolidated Linux acceptance checkpoint — 23 October 2026

Boss authorized the complete applicable Linux CTest suite, fresh Release, strict
ASan/UBSan/leaks, unchanged1MiB frontend/depth probes, existing workload checks and
actual Linux VS Code/Neovim/Vim acceptance. No features or Steps42–44 are included.
Initial configured inventories: Debug199, strict199, old Release175 (stale CMake
registration, to regenerate in a fresh official Release build). All registered
Linux tests will run without selection filters. The optional research-only LLVM
certification experiment configuration is OFF in the official builds, as before.

Frozen code/test identities: engineering/step41-code-freeze.json, matching Step40.
Verified isolated stdlib root and baseline hashes: step41-baseline.json. Installed/
relocated/default-root checks retain their own root selection. Step37 observations,
the original Clang reproducer, deferred compiler/editor/debugger directions and
all Step40 evidence remain preserved. Only inherited deep-regression wording in
the Step40 result JSON was corrected; original JSON is retained separately.
Acceptance is in progress, not a completed Step41 or Stage10 claim.

### Step41 initial failures and restart checkpoint

The first unfiltered Debug run reproduced modules_cli's existing assertion:
builtin result /= value failed at3:12 instead of its established3:5 LHS span.
compound_value used compound_token for builtin arithmetic as well as custom calls.
Restore the statement token for builtin operation failures; retain custom operator
call/glyph metadata. The old assertion must remain unchanged.

The combined console driver also hit its existing120s child deadline during O2
JIT while multiple full builds/tests ran. An exact reproducer completed successfully
in102s under that load. Host policy denied sibling GDB attach; parent-owned GDB
captured LLVM's post-RA instruction scheduler actively compiling, not a console
wait. Preserve original failure/probes; no timing limit or LLVM policy is changed.
Final configurations will run sequentially after all builds finish, then compare
behavior and elapsed times. This does not establish a repair of historicalStep37
or the separate Clang crash.

All initial old-source runners were interrupted before production repair; partial
results and actual successful editor checks are retained under step41-initial-*.
No initial full suite is counted as final acceptance. After repair, rebuild every
configuration, rerun all199 applicable tests per configuration, repeat final-server
editor/default-root/stack gates, and record matching final sources.

Step41 repair applied: builtin compound arithmetic emission again uses s.token;
custom operator calls retain s.compound_token. No test/timeout/guard was changed.
step41-code-freeze.json and step41-full-source-freeze.json (766 build/test/editor
inputs) record the repaired source; initial copies retain the old identities.
Final complete builds run through /tmp/step41-run.py <debug|release|san> build.
/tmp/step41-final-sequence.py waits for all builds, checks unchanged modules_cli,
then runs unfiltered Debug, Release and strict CTest sequentially, including the
full203 conversion/deep72 regression. Standalone frontend/depth1MiB gates and
native workloads follow. Final-server editors and six default-root probes are
queued separately after all builds. Exact running/completed phase records use
step41-<configuration>-commands.json; no pending phase is acceptance.

Initial-run evidence also retains stage10_operators_aot's existing60s child timeout
for the1000000-place measurement during overlapping builds/tests. It is not counted
as a pass or an independently repaired algorithm. All original child/suite budgets
remain unchanged for final serialized acceptance.

Step41 long-build recovery: final Debug and strict all-target builds have passed.
The fresh Release directory is still linking real pinned-SDK LLVM executables;
observed live ld.lld jobs use roughly6–7 CPUs each on this16-CPU host. This is active
linking, not a disabled/hung test; no link/optimization flags were relaxed. All
final tests deliberately remain queued until Release finishes. Initial Debug/strict
CTest logs at their old filenames remain historical copies until overwritten by
final runs; see step41-initial-* and do not count them as final.

Current execution handles: Release build86190; final sequential acceptance20870;
final editor runner43542; final default-root controls7480. The finalizer is prepared
at /tmp/step41-finalize.py but has NOT run and asserts all597 final outcomes before
writing a completion record. After final editor execution, capture VS Code's CLI
version with /usr/share/code/bin/code --version (the raw Electron --version yields
no text; /usr/bin/code is absent). Preserve old editor attempts separately. Full
build/test inputs remain frozen except the single documented builtin-span repair.

Step41 final build/editor checkpoint: all three complete builds passed (fresh
Release included). Unchanged modules_cli direct regression passed42.32s. Final
Debug unfiltered199-suite run is active; Release then strict follow sequentially.
Final Debug-server extension unit/typecheck/build and all six real editor phases
passed. VS Code1.143.0/Node24.19.0, Neovim0.13 development and actual Vim9.2 versions
are recorded. All six final default-root temporal/reload controls pass with exact
stdlib hashes. These results do not close historicalStep37 or native-platform gates.
Final CTest/stack/workload outcomes are still pending; do not count old filenames'
initial-run copies as final acceptance until matching final command/JUnit records.

Step41 Debug full-run checkpoint: current production code passes the unchanged
compound-span regression; full203/deep72 passed518.55s. Legacy strings_source has
an obsolete pre-Stage10 rejection of a valid triple-quoted literal. A14-case exact
CLI audit accepts only that approved form; every other former invalid literal and
reserved string function still rejects. Evidence: step41-legacy-string-rejection-
audit.json and the full Debug log. Preserve this attempted full run, then replace
that obsolete assertion with both exact normal/O2 positive execution and malformed
unterminated-triple rejection. No language rule should be weakened. Let this Debug
run finish to expose remaining neighboring issues before updating/rebuilding only
the affected string harness and repeating final-source acceptance.


Step41 first complete repaired-production Debug run:196/199 in1692.65s, with only
three stale expectations: strings_source valid triple rejection, frontend's old
synthetic expected-i32 wording for bool+=bool, and growable_arrays_cli's old
selected-write wording. The latter two still correctly reject unsupported operands
and invalidated storage; exact native failure/source behavior was preserved.
Keep this full attempt under step41-before-harness-debug-*; its frontend1MiB run
stopped at the same obsolete message assertion, not a stack-budget failure.

Repairs are confined to three test files: the triple now has exact-output positive
execution in both JIT modes plus unterminated-triple rejection; bool compound
rejection pins the current precise diagnostic and3:10 caret; growable failure
checks pin the shared ABI's full structural-modification class, status and source.
No meaningful rejection, stack budget, depth, timeout or production behavior was
weakened. All original Runtime/compiler source changes remain only the documented
builtin-span repair. Current full freeze includes these three harness updates.
All-target incremental rebuilds and direct three-suite/1MiB checks precede another
complete unfiltered Debug/Release/strict pass. Previous complete fresh Release build
records remain under step41-before-harness-release-*; this rebuild only updates test
inputs. Final editors/default-root controls remain valid only if production/server
artifact hashes match, which the finalizer checks explicitly.

Step41 corrected-harness direct acceptance: strings_source, frontend and
growable_arrays_cli all pass (3/3,27.37s). The complete frontend also passes under
the original1MiB limit, with no guard/depth changes. Complete final-source Debug,
Release and strict runs are active/queued again through execution86635. Production
compiler/Runtime/server hashes are unchanged by these harness-only corrections;
actual editor/default-root evidence is retained against those exact artifacts.


### Step41 resumed acceptance — security-boundary stop,23 October2026

No previous acceptance processes remain running. No duplicate builds or test
matrices were launched. Complete final Debug passes199/199 (1672.6967s command
wall), native formatting workload and both genuine1MiB probes pass. Fresh Release
completes198/199 (1068.1374s): source_cli fails its semantic-depth child with
SIGSEGV. Standalone semantic_depth.py likewise fails the400-call rejection; full
frontend under1MiB crashes at combined expression/constant depth. Release native
formatting workload passes. Root cause is not established; no source repair made
in this resumed session. Final strict CTest has NOT started; its old test log is
an interrupted initial attempt and must never be counted as final acceptance.

The known parent-owned GDB probe was attempted with the unchanged1MiB wrapper.
Platform response: `Could not trace the inferior process`, `ptrace: Operation
not permitted`, inferior startup127, `No stack` (GDB command exit1). Per Boss's
explicit security boundary, diagnostic/engineering execution stopped immediately.
No escalation, alternate debugger/program, policy modification or security bypass
was attempted. Only this recovery/evidence checkpoint was written afterwards.
Exact record: engineering/step41-release-stack-gdb.txt. This is a platform
restriction, not an automatic approval-review rejection.

Verified recovery: all766 frozen build/test/editor inputs match; root README,
prior Step40 acceptance artifacts and historical baseline artifact hashes match.
All six real-editor phases and six default-root probes pass against matching
artifacts. Preserve196/199 initial complete Debug evidence and all three corrected
legacy harness regressions; current Debug/Release attempts additionally archived
as step41-before-release-stack-*. step41-results.json records completed, failed
and not-started gates separately with exact command/JUnit/source/evidence hashes.

Safe next action requires owner resolution of diagnostic access/security policy,
then diagnosis/repair of the Release1MiB failure using unchanged limits, guards,
assertions and timeouts. If production changes, refresh freezes/rebuild/reverify
matching final-source acceptance and editor/artifact identities. Do not run
/tmp/step41-finalize.py: its all-pass prerequisites are false. Final strict matrix,
post-repair final artifact audit and Step41 acceptance remain open. Steps42–44
and native Windows/macOS remain separate. Stage10 remains INCOMPLETE. Step37's
unexplained observations and the original Clang crash evidence remain unresolved.


### Step41 Release repair resumed with Boss native backtrace

Boss supplied the missing host GDB backtrace: enum_expression above repeated
method_expression/expression_impl/expression. No debugger access is required or
attempted in this worker. No acceptance process remained running. Pre-repair
controlled cases reproduce Debug124 legal/400 recovery; Release124 legal but400
SIGSEGV, all under unchanged1048576-byte stack (step41-release-stack-before-cases).
Release object disassembly shows expression_impl reserves0x1038 bytes per recursive
frame, plus expression0x168/method0x328; enum_expression reserves0xf68 transiently.
The result_expression handler lacks the established noinline boundary and owns a
large Function for editor signature metadata. Added only its existing handler
annotation/comment; build and controlled post-change verification are pending.
Do not call this repaired until original full1MiB probes and acceptance pass.
Previous incomplete results/freeze and Debug/Release outputs remain archived under
step41-before-release-stack-*. No guards/limits/tests/optimization flags changed.


Step41 demonstrated minimal Release repair: annotating only result_expression with
the existing VOID_SEMANTIC_HANDLER moves the large Function signature temporary
out of every recursive dispatcher frame. Pinned-O3 object disassembly shrinks
expression_impl's %rsp reservation from4152 to1088 bytes; method/expression/enum
reservations are unchanged. Release124 legal and400 rejected/recovering now pass
under1048576 bytes; unchanged semantic_depth.py passes every receiver/numeric/
index/block/type case. Root-cause/control evidence: step41-release-stack-diagnosis.json.
This is not yet full acceptance. Matching builds are active: Debug62023, strict79691;
sequence12686 waits for target build34363 (completed), runs full Release build,
Release neighbors/full frontend1MiB, final editors/default roots, then unfiltered
Debug/Release/strict sequentially. Script /tmp/step41-after-stack-sequence.py and
step41-release-stack-verification-commands.json preserve phase state. All source
freezes updated for the same minimal annotation. No further debugger attempt.

Step41 build-launch correction: strict build79691 was mistakenly started in the
traced default worker. Its sanitized compiler helper aborted at runtime-normal.o
with LeakSanitizer's known cannot-work-under-ptrace error (build2), before final
CTest. Preserved under step41-traced-san-* and step41-traced-san-environment.json.
The established untraced execution profile was accepted for identical strict build
17726; leak detection, flags, limits, source freeze and test inventory unchanged.
This was not a debugger-access retry or changed security policy. New strict build
must finish before sequence12686 starts acceptance. No failed/partial build or
old-source test is counted as final acceptance.

Step41 current build checkpoint: Debug and corrected untraced strict all-target/
workload builds pass. Release rebuild remains actively linking existing pinned-SDK
targets; final repaired-source matrices have not started. Sequence12686 remains
active; /tmp/step41-after-stack-sequence.py will run focused Release gates, all
final editors/default-root controls and three unfiltered configurations. The
prepared /tmp/step41-finalize-after-stack.py is syntax-checked but NOT executed;
it requires597/597 and records the Release repair, legacy harness corrections,
traced-launch failure and preserved historical evidence. See current phase records
and step41-after-stack-recovery.json before resuming; do not duplicate builds.

Step41 Release link observation: pinned ld.lld printed an LLVM GVN/AnalyzeLoad-
Availability stack while processing SDK libLLVMGlobalISel IRTranslator.cpp.o.
At this checkpoint make did NOT report a failed command; both adjacent generics/
collections link targets reported success and further links continue. Preserve
step41-release-LLD-stack.txt and step41-release-LLD-observation.json with raw full
build log. Root cause/outcome is not established; do not assume this is the old
Clang crash, repaired behavior, or an actual failed phase before return status.
No retry was started; no LLVM version/optimization flag was changed.


### Step41 interruption recovery: original Release build succeeded

Boss authorized continuation after a reported ChatGPT content-safety interruption.
Read-only host process inspection found existing sequence4084084/session12686 and
Debug test runner4137341 still running. No duplicate build/test was started.
The original complete Release rebuild finished exit0 in4361.83s; no unchanged
retry was launched or needed. LLVM GVN stack emission is preserved/unexplained,
not a demonstrated failed build or repaired SDK defect. See updated
step41-release-LLD-observation.json and full original Release log.

Focused Release source_cli/frontend/stage10_tooling pass3/3 in4.37s. The entire
original frontend passes genuine1MiB in4.36s; standalone semantic_depth.py and
124/400/combined-depth probes also pass after the minimal handler repair. All
six final editor phases and six default-root controls pass. Final unfiltered
Debug acceptance is active; Release then strict follow in the same existing
sequence. No old-source run counts as final acceptance.

The requested +3/-1 compiler.cpp repair diff is preserved in
step41-release-stack-repair.diff, with reconstructed pre-edit bytes matching
the preserved freeze. All766 final inputs match. Matching build returncodes,
compiler/core/Runtime/server/test binary hashes: step41-after-stack-artifact-
checkpoint.json. The final all-pass/hash audit is still pending. No debugger,
Git/hosted/security-policy/Daybreak action was attempted.

Boss native corroboration preserved: step41-boss-native-success.txt/.json records
original Release GDB reproducer now emits VOID1107 at8:1028 and VOID2003 at10:5,
exits1 without SIGSEGV; No stack is expected. The transcript names our shared
Release binary and original step40-final-stack-small-stack.sh. Current binary
hash matches repaired full-build checkpoint; wrapper SHA matches preserved Step40
artifact and enforces unchanged1MiB. Original SIGSEGV log remains intact. The
host transcript has no separate execution-time digest; provenance is explicit.
No worker debugger was retried. Focused Release/full1MiB pass; final matrices
remain active/queued in sequence12686.

Additional focused strict check: original semantic_depth.py passes at genuine
1MiB with detect_leaks=1 and halt-on-error ASan/UBSan through the established
untraced profile (step41-san-semantic-depth-1m-direct.json/.txt). Final Debug
unfiltered matrix remains active, with full203/deep72 passed509.99s and source
console/AOT passed1108.98s; partial progress is not completed acceptance. Release
and strict follow automatically; no duplicate matrix. Matching complete builds,
focused Release, final six editor phases and six root controls are all passed.
Current recovery snapshot: step41-after-stack-recovery.json.

Step41 final repaired-source Debug acceptance passes199/199 in1649.48s, zero
failures/skips; native format workload and both original1MiB frontend/depth gates
also pass. Existing sequence12686 now runs final Release199-suite acceptance,
then strict; do not duplicate. All matching builds/editors/default roots and
focused Release/full1MiB are passed. Step41 remains INCOMPLETE until remaining
matrices, workloads and final hash audit pass; finalizer not executed.

Step41 final repaired-source Release acceptance passes199/199 in849.31s, zero
failures/skips. source_cli passes3.38s; native format workload and both standalone
original1MiB probes pass. Together with Debug199/199,398 final matrix outcomes
are complete. Existing sequence12686 now runs final strict199-suite acceptance.
No source changes or duplicate runs; finalizer remains pending full597/597 and
artifact/source audit. Stage10 remains INCOMPLETE.

## Step 41 consolidated Linux handoff — 23 October 2026

All 199 registered Linux CTest suites pass on the final repaired sources in each
configuration: Debug **199/199 (1649.50s)**, fresh Release **199/199
(849.33s)** and strict ASan/UBSan/leak **199/199 (1790.98s)**.
**597/597** unique configuration/suite outcomes, zero final failures/skips. No
selection filter or new suppression was used. All builds finish before sequential
final configuration runs. Initial interrupted runs are preserved, not acceptance.
Release was configured in a new directory from pinned LLVM/Clang 19.1.7, correcting
its old 175-test stale registration to the same 199-test inventory. Optional research
and historical-retention baseline controls remain OFF, precisely recorded.

Two bounded production repairs were made. One restores builtin compound diagnostic
anchors to the original statement token. Existing modules_cli exact3:5 assertions pass
normal/O2 JIT/linked AOT unchanged; custom call glyph metadata remains intact.
The other applies the existing noinline handler boundary to result_expression:
Release had inlined its large Function signature
temporary into every recursive expression_impl frame. The dispatcher reservation
shrinks from 4,152 to 1,088 bytes at unchanged-O3; original 1 MiB legal/rejection/recovery
probes and full frontend now pass. The Boss supplied the native GDB backtrace; no
worker debugger retry or policy change was used. Pre-repair Debug 199/199 and
Release198/199 are preserved separately and are not final-source acceptance.
The three legacy harness repairs retain positive triple-string execution and
precise unsupported-compound/invalidated-list rejection checks. No meaningful
test was removed or weakened. The initial console child timeout is retained with
a successful 102s reproduction and an active LLVM post-RA scheduler stack; final serialized runs pass without any
increased test limit or LLVM optimization-policy change. Sibling GDB attach was
denied by host ptrace policy; a parent-owned Debug probe captured the stack without
changing that policy. Strict sanitizer execution remains outside ptrace.

During the final Release build ld.lld emitted a GVN stack on SDK IRTranslator
bitcode. The original complete build nevertheless returned 0; no unchanged retry
was launched or needed. Signal/cause remains unexplained, separately preserved
for Wrench in step41-release-LLD-observation.json and the original full build log.
Boss's successful native GDB rerun is also retained beside the original SIGSEGV:
VOID1107 at 8:1028, VOID2003 at 10:5, exit 1 and no crash. Current shared binary hash
matches the final build, and the exact original wrapper reports 1,048,576-byte
soft/hard limits. No worker debugger or security-policy change was needed.

Both genuine 1 MiB frontend/depth gates pass in Debug/Release; source_cli retains
its own semantic-depth/UTF-8 probes in every configuration. The 124-call acceptance,
400-call rejection/recovery and Step 40 cold-frame repairs remain effective. Full 203
conversion suites, including unchanged 72-level collection regression, run in all
three configurations. Aggregate costs and existing workload compile/invoke/wall/RSS
measurements are recorded without an optimization claim. The raw operator harness
uses a historical Debug label for nonsanitizer builds; separately normalized
workload metadata records actual Release flags and preserves raw measurements.

Normal/O2 JIT, independent AOT, production native builds, installed/relocated source
libraries, exact combined output/failure traces, GC/fault/NativeSafe/lease/cleanup,
real PTY restoration, source identity, cache cold/hit/corrupt and compatible/
incompatible reload regressions pass. Final Debug-server VS Code trusted/restricted/
multi-root, extension units/typecheck/build, Neovim and actual Vim automatic/explicit
and real PTY menus pass with exact versions. Six default-root controls pass and
library bytes remain identical to the isolated source snapshot; historicalStep 37
observations and the original transient Clang crash/reproducer remain unresolved.

Stage 10 remains **INCOMPLETE**. Stop for Golden Wrench review. Native Windows/macOS
are unverified; Step 42 handles those explicit gates, then Steps 43–44 remain. Deferred
compiler/cache/size/incremental/debugger/editor directions stay documentation-only.
Exact inventories, commands, hashes, failures/repairs, timings, outputs and limits:
[Step 41 evidence](engineering/step41-results.json). No Git/hosted/publication/public
root README action or new dependency/framework/feature.


## Step 42 native acceptance preparation — 23 October 2026

Boss reports Golden Wrench accepted Step41. Before editing documentation, verified
all766 accepted build/test/editor inputs, all779 source/documentation manifest
entries, all256 Step41 evidence artifacts and24 accepted binary/archive/object
identities (8 per Linux configuration). No mismatch. Step41 remains historical,
byte-identical evidence; its source/document manifest is not rewritten for Step42.
Root README remains unchanged. No build/test processes were active at inspection.

This host is Linux x86_64; no native Windows/macOS host was supplied. No native
build, test, console experiment or hosted workflow was attempted. Wine is present
but unused. Native results, compiler configurations and binary identities are
**UNVERIFIED**, not inferred from accepted Linux runs or Stage9 CI95.

Audited the existing seven-job Compiler foundation workflow, Windows path probe,
phase23 stack probe, CMake/SDK/DIA/MSVC setup, POSIX/Darwin console paths and native
Windows console paths, Stage8 canonical identity and Stage9 repairs. The default
source-derived inventory projects194 Windows and199 macOS suites; actual native
CTest discovery is mandatory. Five real POSIX console test registrations are absent
on Windows. Injected wide-console tests do not establish ReadConsoleW/WriteConsoleW
or ConPTY acceptance; no current automated ConPTY driver was found. Those explicit
host/state/failure gates remain open even after an eventual green Windows CTest run.

Prepared exact Windows x64 and macOS ARM64 Debug/Release commands, source receipts,
unfiltered CTest/JUnit evidence, genuine1MiB probes (PE reserve on Windows, inherited
soft/hard RLIMIT on macOS), native workloads, Windows path controls, actual editor
steps and artifact closeout. Existing workflow dispatch cannot select only four
native jobs: its matrix includes three Linux jobs as well. No workflow, compiler,
Runtime, test, optimization, timeout, policy or source limit was changed. No new
production defect was reproduced; no speculative repair was made. Local preparation
checks are recorded separately from native acceptance in step42-results.json.

**Next concrete action:** Boss authorizes an exact native source snapshot and either
native hosts, the existing full seven-job workflow, or a reviewed native-only matrix
selection change. Then execute the prepared commands, preserve actual host and
binary identities/original failures, and repair only demonstrated native defects.
No dispatch/commit/push/branch is authorized by this preparation. Step42 is not
complete; Steps43–44 remain separate. No security restriction was bypassed.

Deliverables: [native commands and remaining gates](engineering/step42-native-commands.md),
[source-derived inventory](engineering/step42-inventory.json),
[preparation evidence](engineering/step42-results.json). Step37 anomalies, original
Clang crash/reproducer, deep collection cost, Release LLD GVN observation (original
build exit0, no retry, cause unknown), deferred usability/optimization and editor
observations remain preserved. Historical Step41 wording above describes its dated
checkpoints; the current checkpoint is this entry.


## Step 42 CI96 native repairs — 23 October 2026 (focused Linux verified)

Original source ef6017613ecf92de31380476ed5d9c3fc8cf3dbb and complete decoded
native job logs are preserved under engineering/ci96/. All seven pre-repair source
files match the committed Git blob identities. Windows Debug/Release each190/194,
four identical failures; macOS ARM64 both configured but failed stop_token compile.
Those original results are not repaired-source acceptance.

The test now explicitly expects unavailable Windows native caching, retaining
behavior/edit/reload/lease checks and no persistent files. No speculative Windows
cache was added. The source console cache regression also avoids iterating a
nonexistent store and runs hook/spec edits through fallback. Only the legacy
CLI/printf result trailer expects native Windows CRT CRLF; source print stays raw
UTF-8/LF with independent JIT/AOT exact-byte assertions. Relevant Python fixture
reads/writes now specify UTF-8; the original cp1252 failure reproduces, while the
fixed fixture passes144 checks against the accepted Linux server.

Temporal civil_seconds now promotes sys_days to64-bit sys_seconds before adding
hours/minutes. MSVC's32-bit minute representation narrowed the old intermediate:
the precise width model maps9999-01-01T23:59:59 to1832-11-16/-4327186561 seconds;
the repair gives9999-01-01/253370851199. This is an ABI-width reproduction, not
native component output absent from original check1195. All original round-trips
remain, with richer diagnostics and explicit year1/year9999 epoch endpoints.

Pinned libc++190107 hides stop_token behind its experimental gate. The same pinned
headers reproduce the missing types on Linux; its supported experimental opt-in
compiles, links and runs cross-thread callbacks without an experimental archive.
Native Apple CMake now compile/links the standard probe, then only if necessary the
pinned opt-in, propagating its definition through existing void_options. Unsupported
compile/link remains fatal. Existing std cancellation, lifetime, buffer and shutdown
machinery is unchanged. Actual Apple SDK compile/link/run still needs native access.

Fresh build/step42-ci96-debug and -san configurations preserve all accepted Step41
binaries and archives. Focused Linux Debug18/18 and strict18/18 pass on matching sources; exact phase
commands/statuses are in engineering/ci96/linux-commands.json and
linux-untraced-commands.json. Both unchanged genuine1MiB probes pass. The evidence
recorder Path serialization error after the first frontend probe is preserved;
its repair reran both probes with complete exit records. No strict build/test
started under the traced profile. A small strict
probe reproduced the known sandbox LeakSanitizer/ptrace issue; the established
untraced execution profile passes unchanged ASan/UBSan/leak options. No debugger,
security policy/suppression, native-host simulation credit or Git/CI action was used.

Current handoff: engineering/ci96/REPAIR_HANDOFF.md, repairs.diff, source-changes.json
and767-input final-source-freeze.json. Final local evidence: Debug18/18, strict18/18;1311 temporal core,248 compatibility/
cache/reload and144 source-formatting LSP checks per configuration, plus both1MiB
probes. All767 frozen inputs,256 historical Step41 artifacts and24 accepted Linux
binary/archive/object identities match; original compiler.cpp and root README are
unchanged. Next: Wrench review, then Boss-controlled actual native SDK/chrono probes,
four-suite reruns and remaining native acceptance. Step42
and Stage10 INCOMPLETE; Steps43–44 separate. Historical Step37/Clang/LLD observations
and deferred objectives remain unchanged.


### Historical CI96 verification recovery checkpoint (superseded below)

Debug18/18 passes (1336.82s), zero failures/skips. Both original standalone1MiB
probes now have clean recorded exit0: frontend9.48s and semantic-depth0.61s, soft/
hard1048576 bytes. The evidence recorder PosixPath serialization error after the
first frontend run is preserved and fixed; no strict build/test had started there.
Actual strict configure passes under the established untraced profile; selected
targets are building in build/step42-ci96-san. Local tool session12014 is active;
do not start a duplicate build. See engineering/ci96/recovery.json and
linux-untraced-commands.json. After strict completion and verified18-case JUnit,
run engineering/ci96/finalize.py to aggregate final hashes and handoff. It refuses
incomplete acceptance. All767 code inputs remain frozen; no repaired native source
has executed. Step42/Stage10 INCOMPLETE.


### CI96 final bounded repair handoff

Focused Linux Debug18/18 and strict18/18 are complete, zero final failures/skips.
Both original1MiB probes pass. The active acceptance runner has finished; no new
build/test should be started to repeat these results. Final source/binary/evidence
hashes and native limitations are in engineering/step42-results.json. All767
frozen inputs,256 Step41 artifacts and24 original binary/archive/object hashes
match their appropriate current/historical manifests. Repaired Windows/macOS
source remains UNVERIFIED. Next: Golden Wrench review, then Boss-controlled native
SDK/chrono probes, four-suite reruns and remaining Step42 gates. Step42 and Stage10
INCOMPLETE; Steps43–44 remain separate.


### CI96 Ubuntu job-budget correction — Boss review

Boss confirms all three CI96 Ubuntu jobs (Debug, Release, Sanitizers) were
cancelled at approximately three hours while still linking native targets, before
CTest started. These runs do not establish acceptance or CTest failures. The
earlier in-progress CI96 job snapshot and all original failure logs are preserved.

`.github/workflows/ci.yml` now allows360 minutes instead of180. Exact byte
comparison verifies this is the only workflow change: the seven-job matrix, all
CTest deadlines, original1MiB limits, sanitizer settings, assertions and optimization
flags remain unchanged. All767 frozen production/test/build inputs and historical
repair deliverables match their hashes. No local build or test matrix was repeated.
See `engineering/ci96-timeout/verification.json` and `workflow.diff`.

Golden Wrench accepts the native repairs for re-verification, per Boss. Next:
Boss reviews this workflow-only correction and controls Git/hosted re-verification.
Repaired-source Windows/macOS acceptance remains UNVERIFIED. Step42 and Stage10
remain INCOMPLETE; Steps43–44 remain separate. No Git mutations or CI launch.


## Step 42 CI97 native console repairs — 24 October 2026

Boss continues the existing mission following Golden Wrench's review of main
CI97 and ThinLTO experiment#2. MainCI97 at96bac2ea independently confirms Windows
Debug/Release194/194 each and Ubuntu Debug/Release199/199 each, with original
frontend1MiB gates. macOS Debug/Release each195/199 fail native transfers/input/
keyboard and stage10_console_aot; Ubuntu Sanitizers198/199 reports Escape1b rather
than Up Arrow1b5b41. The experiment at8485e640 separately repeats that symptom.
Hosted job metadata, both failing macOS logs, main strict log, experiment Debug
log and success excerpts are preserved under engineering/ci97/. No cache cause,
fix or performance result is inferred; main and experimental workflows are intact.

POSIX adapter review found no fcntl mutation or close of borrowed handles: only
owned wake-pipe descriptors receive those setters. Apple XNU source shows that a
successful host write sets FWASWRITTEN, exposed by F_GETFL. Transfer/input fixtures
now distinguish addition of that specific Darwin bookkeeping bit from changed
configured flags. FD_CLOEXEC, access mode, O_NONBLOCK, O_APPEND, errors and all
other bits remain checked, with raw before/after snapshots and a Runtime-free
write control. No production descriptor mutation was added or excused.

XNU also adds PENDIN on a non-flushing return to canonical input. The previous
mode verifier could turn this successful transition into syntheticEIO. Apple-only
verification now accounts for that kernel state while checking every configured
attribute/cc/speed. Nested saved snapshots, LIFO ownership, actual native errors
and cleanup reports are preserved. No input flush, PENDIN clearing or second
collector/input architecture was introduced. Python native/source PTY assertions
share the narrow comparison and retain raw snapshots separately from configured
restoration. Actual hosted deltas/return codes were not in the old logs; Darwin
source/model evidence is not a claim of repaired native macOS success.

Original real-PTY repetitions passed20/20 in both local configurations before the
repair. A read wrapper captured the entire103-byte key batch in one native read,
including complete arrows, with no observed delivery fragmentation in those two
batches. A virtual-clock regression then reproduced the bug against both original
Debug/strict archives: capability lookup advanced31ms while all bytes were already
queued. Before: key4/1b; after: key5/1b5b41, same four-byte batch and virtual time.
The decoder now consumes buffered continuations before expiring a native wait;
unbuffered expiry, cancellation, all original keys and the30ms limit remain.
A companion guard proves that later readers retain the untouched[Aq suffix.
No arbitrary sleep or timeout increase was used.

Focused repaired native-console suites pass11/11 Debug and11/11 strict; original
native-keyboard repetitions pass20/20 each. Independently linked source
console_keys normal/O2 passes exact original output and terminal restoration in
both configurations. The emitting compiler is the accepted CI96 binary, not a
freshly relinked compiler/JIT. Only console.cpp.o differs in copies of the accepted
Runtime archives. Full CMake, full stage10_console_aot and native Windows/macOS
acceptance were not rerun. Raw LSan/ptrace launch failure and one unproven strict
compiler diagnostic are preserved; final strict builds/tests use the established
untraced profile with ASan/UBSan/leaks unchanged. A new guard-test max-read
assumption and a recorder filename collision were corrected and recorded; no
original key expectation was weakened. Original CI96 binaries, Step41 evidence,
stack scripts/limits, root README, CMake and both workflows match their hashes.

Deliverables: [exact source diff](engineering/ci97/repairs.diff),
[diagnosis and native observer commands](engineering/ci97/DIAGNOSIS.md),
[source/binary preservation verification](engineering/ci97/verification.json),
updated engineering/step42-results.json and raw reproductions. Golden Wrench
review is pending. Boss controls native macOS Debug/Release observer/tests and
the next hosted run. No Git mutation or CI dispatch. Step42 and Stage10 remain
**INCOMPLETE**; Steps43–44 are separate.


## Step 42 CI98 final macOS Release investigation — 24 October 2026

Baseline9a4985f89f40c56b8eaa984775faf18c52b69f19. MainCI98/run40091828693 and
ThinLTO#3/run40091853317 each pass6/7 jobs, all except macOS Release. The independent
remaining failures are main runtime_console_native_terminal and experimental
runtime_console_native_transfers. Exact hosted logs/job snapshots are in
engineering/ci98/. No cache cause or production failure is inferred from their
occurrence in different workflows.

The terminal fixture launches a child and then writes redirected input, while the
child deliberately polls once with timeout0. Deterministically withholding delivery
reproduces the exact empty native report. A same-Context READY-barrier probe then
shows pipe classification, unsupported cursor query, initial would_block/empty
read and later intact7265646972656374656400ff. No input loss, hidden queue drain or
classification defect is demonstrated. The ordinary pipe is now prefilled before
child launch; parent-owned ends are closed correctly and every original byte,
no-output and mode assertion remains.

The cancellation fixture's admission/wake-fd checks precede native write progress;
macOS also lacks Linux's syscall wait. An existing Adapter/Runtime model proves
admitted1/completed0/closed is legal under that ordering. The native test now first
checks stdout writable, then waits for poll0/no-revents while the parent retains
its fresh pipe reader without draining it. This writable→not-writable transition
proves actual writes used the pipe's writable capacity before GC and cancellation.
The observer opens no fd and cannot replace the deliberately missing stdin.
Original EBADF, positive partial prefix, exact parent-observed bytes, GC roots,
borrowed flags, cancellation and cleanup assertions remain. Actual Linux Debug
and strict records show initial1/POLLOUT4, later0/revents0 and65536 acceptedW bytes
for both cancellation modes. That count is local, not a macOS capacity claim.

Only three test files changed; production Runtime, both workflows and all previous
builds/evidence remain hash-identical. The changed C++ test driver alone was rebuilt
against accepted Runtime archives. Focused Debug11/11 and strict11/11 pass; each of
the two native fixtures passes20/20 in each configuration. Both deterministic
ordering reproductions pass, with unchanged ASan/UBSan/leak settings and the
established untraced strict profile. Exact diff, source freeze, raw byte/progress
reports, commands and preservation checks are retained. Original Step41 evidence
and genuine1MiB protections were preserved, not rerun or promoted to new acceptance.
No full production rebuild, JIT/AOT matrix or native macOS run occurred here.

Deliverables: [diagnosis/native requirements](engineering/ci98/DIAGNOSIS.md),
[exact source diff](engineering/ci98/repairs.diff),
[preservation verification](engineering/ci98/verification.json), raw reproductions
and updated engineering/step42-results.json. Stop for Golden Wrench review before
Boss commits or runs native Release verification. No Git mutation, CI dispatch,
new architecture or next-step implementation. Step42 and Stage10 **INCOMPLETE**;
Steps43–44 remain separate.
