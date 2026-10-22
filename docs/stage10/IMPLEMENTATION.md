# Stage 10 engineering progress and permanent handoff

Updated 22 October 2026 (America/Los_Angeles). **INCOMPLETE.** The entire
44-step mission remains authorized. This is a recovery record, not a request for
new authorization, completion claim, or completed Wrench review.

Baseline HEAD: `e25816bdb80e3f32613edf77c9cf3258232af966`. Existing dirty Stage 9
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
  field/display/interpolation and syntax acceptance passes. VS Code unit/typecheck and native trusted, restricted and multi-root host profiles pass. Initial host attempts exposed test assertions that serialized MarkdownString as an empty object; direct value access fixed the assertion. The `vim` executable here is Neovim,
  so a real Vim acceptance run remains unavailable (a targeted script is saved).

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

**Missing source systems:** print and all console/terminal APIs; collection/enum/result
composite display; owner operators and custom-equality key exclusion. Nominal/custom hooks and source temporal APIs have focused JIT/O2/AOT, editor
and hook-reload acceptance, not complete cross-product/full-stage acceptance. No unimplemented feature
is reported as unsupported hardware.

## Dependency identity and license

See `third_party/stage10-versions.json` for downloaded archive SHA-256 digests.
Ryu commit `72e18e8d14d4392a4a3261f37cde248e4fbedbec` uses the Boost Software
License 1.0 option (upstream dual-license texts retained). Date v3.0.3 commit
`5bdb7e6f31fac909c090a46dbd9fea27b6e609a4` uses MIT notices preserved by file in
`third_party/date/NOTICE`. IANA 2026e source data is public domain as described by
its included LICENSE. `tools/embed_tzdata.py` reproducibly generates the embedded
header with per-file digests and portable byte escapes.

## Current verification

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
| 9 | Field compatibility/metadata/LSP and focused VS Code/Neovim checks pass; native Vim and full acceptance open. |
| 10 | Checked builder and shared numeric/layout/temporal parsing implemented; composite consumers remain open. |
| 11 | Pinned Ryu and all integer native/source primitive rendering integrated; composite/custom consumers open. |
| 12 | Native/source primitive numeric patterns, result errors and literal interpolation diagnostics implemented; full acceptance open. |
| 13 | Primitive and nominal/custom conversion/defaults implemented; composite display remains open. |
| 14 | Ordinary readonly custom hooks, effects, roots and recursion guards implemented; 77 focused checks; full cross-product acceptance open. |
| 15 | Constant primitive conversion, formatting and interpolation share Runtime numeric kernel; full display types open. |
| 16 | Typed interpolation supports primitive, nominal/custom and temporal values/specs; composite types remain open. |
| 17 | Triple strings, multiline holes/comments, escapes and source spans implemented; broader editor acceptance open. |
| 18 | Primitive/custom/temporal holes use ordinary calls, roots and checked join/failure cleanup; composite integration open. |
| 19 | Validated source date/time/local/instant/offset/duration/zone/zoned values and factories implemented; focused acceptance only. |
| 20 | Checked wide seconds/fraction arithmetic, UTC/native resolution and per-Runtime monotonic context implemented; native Windows/macOS unverified. |
| 21 | Embedded IANA 2026e, canonical zones, explicit overlap choice and gap rejection integrated with source APIs; full historical/platform matrix open. |
| 22 | Shared temporal token grammar, defaults, alignment and literal diagnostics implemented; focused acceptance only. |
| 23 | Not implemented / not accepted. |
| 24 | Not implemented / not accepted. |
| 25 | Not implemented / not accepted. |
| 26 | Not implemented / not accepted. |
| 27 | Not implemented / not accepted. |
| 28 | Not implemented / not accepted. |
| 29 | Not implemented / not accepted. |
| 30 | Not implemented / not accepted. |
| 31 | Not implemented / not accepted. |
| 32 | Not implemented / not accepted. |
| 33 | Not implemented / not accepted. |
| 34 | Not implemented / not accepted. |
| 35 | Not implemented / not accepted. |
| 36 | Not implemented / not accepted. |
| 37 | Field compatibility and native dependency archive identity updated; hook dependency/reload generation tests pass; operator integration open. |
| 38 | Field/primitive/nominal/temporal LSP and native Neovim checks pass; remaining systems and latest native VS Code/Vim acceptance open. |
| 39 | Dirty conditional/comment calls, interpolation physical spans, literal specs, rename and source formatting regressions pass in Debug; remaining systems open. |
| 40 | Combined private const fields + primitive formatting/interpolation JIT/O2/AOT and native-format workload pass; four-system integration open. |
| 41 | Focused Linux checks passed; consolidated full Stage 10 acceptance not run. |
| 42 | Native Windows/macOS unavailable locally; targeted obligations retained. |
| 43 | Engineering progress and evidence recorded; final feature documentation/examples open. |
| 44 | Incomplete. Not a completed Stage 10 handoff and not independently Wrench-reviewed. |

## Every feature-completeness matrix row

| Capability | Current state |
|---|---|
| Strict UTF-8, NUL, scalar indexing, owned byte conversion | Existing source remains; new console integration absent. |
| Named/default binding and written-order evaluation | Existing source remains; print binder absent. |
| Heterogeneous print | Open: no source print implementation. |
| No-newline output and prompts | Open. |
| Standard endpoints and byte/text access | Open. |
| Character/text/line EOF/errors | Open; immediate-CR policy is recorded only. |
| Redirection and Windows Unicode | Open; Stage 9 path/argv corrections preserved. |
| Terminal input and presentation | Open; cursor-channel policy recorded only. |
| Concurrent output / normal exit | Open. |
| Complete interpolation | Primitive, nominal/custom and temporal typed holes/specs and multiline/triple parsing implemented; composite display remains open. |
| Numeric/bool conversion | Primitive source/default/constant conversion and formatting implemented with focused tests. |
| Numeric formatting | Native/source APIs, constant evaluation and primitive interpolation share implementation; complete four-system acceptance open. |
| Deterministic rounding / special floats | Native tests pass; full shared-consumer acceptance open. |
| Temporal values and formatting | Native kernel/source APIs/hooks/clocks implemented with focused JIT/O2/AOT tests; native Windows/macOS and full-stage acceptance open. |
| Custom display / format hooks | Nominal ordinary-call implementation and 77 focused checks; reload and LSP tests pass; complete cross-product acceptance open. |
| Private fields | Core implementation and focused source/IR/tooling tests; full acceptance open. |
| Readonly fields | Core implementation and focused source/IR/tooling tests; full acceptance open. |
| Copies, readonly views, initialization | Focused old/new tests; full Stage 10 cross-product open. |
| Inheritance/composition/interfaces/generics | Focused field cases; other systems and complete matrix open. |
| Owner arithmetic/equality/order | Open, including custom %. |
| Derived compounds | Existing builtin compounds remain; custom operations and %= integration open. |
| Dictionary equality/hash coherence | Existing data-only behavior remains; custom-equality exclusion open. |
| Compiler-owned LSP / editor integrations | Field/primitive/nominal/temporal LSP and native Neovim pass; remaining systems, native Vim and latest VS Code acceptance open. |
| Reload/cache/JIT/AOT | Field contracts and focused JIT/AOT; full operator/format/cache/reload acceptance open. |
| Git package manager | Separate, unmodified, outside Stage 10. |

## Continuing the already-authorized mission

Next bounded engineering step: **32**, add owner-contained operator declaration
metadata (`Function`/`Structure`) and owner-body parsing in
`src/frontend/compiler.cpp`, including `%`, then focused positive/negative parser
checks. No operator implementation has landed yet. Continue subsequent exact
selection, ordinary-call lowering, captured-place compounds and dictionary-key
exclusion as separate recoverable steps. Composite display, all console/terminal
APIs, remaining field/tooling cross-products and final acceptance remain authorized
and open. Do not restart completed dependency research or implementation.

The latest owner clarification explicitly permits bounded engineering turns and
context windows; the 44-step authorization remains active across them. Run the
consolidated Stage 10 pass only after all four systems are implemented. Boss
retains Git/hosted/release control.
