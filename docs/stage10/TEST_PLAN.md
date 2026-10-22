# Stage 10 focused tests and acceptance evidence

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


**Approved acceptance plan, not a claim of executed results.** Current focused
engineering evidence is recorded separately in IMPLEMENTATION.md.
The repository's existing CTest, C++ source harnesses, Python subprocess helpers,
LSP client and real editor runners are sufficient. Extend those facilities; do
not build a certification service, new validation framework or CI workflow.
[Main roadmap](../STAGE10_DESIGN.md).

## Test execution discipline

For each implementation slice, run the changed source/Runtime suite and relevant
existing neighboring suites on Linux. Add normal/O2 execution and linked AOT for
observable source behavior. Use the existing sanitizer configuration for memory,
concurrency handoff and lifetime changes; use deterministic barriers/fault adapters
for shutdown and partial I/O. Repeat broader checks after failures or integration
changes, not after every documentation edit or unchanged step.

After all four systems integrate, run the complete locally available consolidated
Linux suite once, then targeted native platform tests. Windows **Debug and Release**
both require console/Unicode/error-path evidence; a Release result alone does not
cover Debug. macOS needs its own terminal/time-zone/pipe evidence. Missing local
platform access is recorded explicitly and handed to Boss for a later authorized
run; it is never inferred from Linux. No full seven-job hosted run is required
between engineering steps. No hosted run is authorized by this document.

## Source behavior matrix

Every executable fixture below must pass source checking, normal JIT, O2 JIT and
an independently linked native executable unless a row explicitly exercises only
analysis/tooling or Runtime injection. Compare exact bytes, values, side-effect
logs, status and diagnostics, not just exit success. Keep legacy entry-result CLI
output distinct from application stdout.

| Area | Required positive cases | Required rejection/failure cases |
|---|---|---|
| Print | Zero/one/many heterogeneous values; Unicode/NUL; separator/end/file/flush; custom display | Unknown/duplicate controls, positional after named, wrong control type, no-result/unfinished argument |
| Evaluation | Written argument order, value snapshots, managed alias identity; holes left-to-right | Throwing/failed display stops later hooks/holes; expression effects retained; no partial print record before write |
| Numeric defaults | All signed/unsigned widths, minima/maxima; f32/f64 normal/subnormal ±0 | No widening, locale dependence, ACP conversion or loss of signed zero |
| Digit patterns | `000`, `0.00`, `0.00#`, `#,##0.00`, `+0.00`; carry/sign/grouping | Invalid grouping/scaling commas, optional-before-required fraction placeholders, unknown tokens |
| Rounding | Exact 2.5→2 and 3.5→4 at precision 0; f64 2.675→2.67 at precision 2; percent ties | No double rounding; independent of host rounding mode and locale |
| Bases/scientific | Every integer width; signed negatives; X/x/B/b; padding; exponent carry | Float D/X/B rejected; no accidental sign extension or overflow on signed minimum |
| Special floats | nan/±inf/negative zero default and patterned/aligned | Canonical spellings; NaN payloads not accidentally printed as numbers |
| Resources | Long literals/output, large precision/padding, checked extents | Unrepresentable width/precision rejected before allocation; allocation faults clean roots; existing constant limits retained |
| Composite display | Ordered arrays/dicts, enums/results, nullable, opaque types; cycles | No private-field or native address leak; mutation-invalidated iteration fails safely |
| Custom hooks | Managed/value/generic, readonly receiver, inherited/using selection, custom domain errors | Wrong signature, mutating/async hook, inaccessible/ambiguous hook, recursion, allocation failure |
| Interpolation | Calls, indexes, dict/result/enum expressions, if/match, nested strings, multiline holes/triples | Empty/unmatched holes, ambiguous colon with helpful parentheses diagnostic, malformed spec/quoting |
| Escapes | `{{`/`}}`, `\u{7b}` literal brace, quotes/backslashes/NUL, CR/LF/CRLF | Decoded braces never reparsed as holes; literal bytes/spans remain original |
| Dates/times | Valid factories, leap years (2000 yes, 1900 no), month names, 12/24h, fractions | Invalid components, 12h without AM/PM, wrong-type tokens, no leap-second invention |
| Duration/instant | Negative fractions, i64 extrema, checked arithmetic, monotonic elapsed | Overflow, calendar projection outside 1..9999, calendar months treated as durations |
| Zones | UTC/fixed offset; two geographic zones; exact historical-second offsets | Missing/corrupt/version mismatch; overlap reject/earlier/later; gap rejects; no ambient local zone |
| Private fields | Owner init/method/accessor/operator/closure reads; internal/public cross-module rules | Same-file free helper, derived access, component friendship, hidden-member fallback, unauthorized field pattern/label |
| Const fields | Pure defaults, visible creation args, direct own init assignment; alias behavior | Post-init store/compound/nested mutation/mutable escape; helper/closure/base-phase grant abuse |
| Copy authority | Reference-free independent copies; reference-bearing value snapshots | Declared const fields remain const after whole-value copy; embedded readonly references never thaw |
| Object integration | Managed/value + generics + inheritance + using + properties + interfaces | Missing setter, ambiguous promotion, staged identity escape, incomplete required references, foreign-owner methods |
| Operators | All approved unary/binary glyphs; mixed exact types; generic specialization | Duplicate tuple/result-only overload, wrong arity/result/owner, implicit conversion, unsupported glyph, async/virtual, mutable operands |
| Selection | Direct owner imports, explicit base cast, nullable exact matches | Two-owner ambiguity, hidden exact target, transitive imports, inheritance/promotion discovery, reversed operands |
| Compounds | Locals/fields/properties/indexed values; LHS/index/getter/RHS once | Failed operation does not commit; captured owner not rebound; readonly and structural-version failures retained |
| Equality/hash | `!=` invokes `==` once; old keys preserve existing equality/hash | Custom equality keys rejected recursively; no callbacks stored in dictionary descriptors; patterns do not invoke custom `==` |

Numeric expected values should include checked-in bit-pattern vectors generated
from an independent high-precision reference during engineering, plus upstream
Ryu test vectors. Test formatter bytes against known vectors, not against a second
call to the same implementation. Use exact bit inputs to avoid confusing source
literal rounding with display rounding. Re-run vectors with changed fenv and
non-English locale in the native harness; never change source numeric semantics.

## Console OS and Runtime tests

| Mechanism | Minimum evidence |
|---|---|
| Redirected streams | Independent stdin/out/err pipes and files, exact UTF-8/NUL/newlines, no BOM or terminal escapes, no terminal attached |
| Shared input | Alternate byte/char/text/line/key reads; CR followed by non-LF, empty line, final line, EOF, every multibyte split; queued lookahead survives switching |
| Encoding errors | Overlong/surrogate/out-of-range/invalid/truncated input; error partial bytes and recovery position; Windows unpaired UTF-16 console events |
| Transfer faults | Inject short success, EINTR, zero write progress, would-block, timeout, error after prefix, full-disk equivalent, broken pipe; verify exact completed byte counts |
| SIGPIPE | Native process with ordinary/custom/preblocked/prepending host signal state; failed write returns result without global handler changes or consuming preexisting signal |
| Concurrent writes | Multiple host callers/Runtime contexts; shared stdout/stderr destination; no intra-record interleaving among VOID writers; external-writer limitation documented |
| Blocking + GC | Hold native read/write/stream-lock wait while another Managed participant allocates and collects; no stale managed pointer or GC rendezvous deadlock |
| Shutdown | Barrier-held admitted read; closing wakes/drains it; new calls reject; no host handle close/cancellation; native buffers and leases released after actual completion |
| Terminal session | Real PTY on Linux/macOS; native console/ConPTY on Windows; capability detection, key/modifier/repeat/unknown sequences, cursor/size/color/clear/title |
| Restoration | Normal exit, return, Runtime failure, nested mode scope, primary+cleanup failure, Runtime shutdown; prior modes restored and primary failure preserved |
| Redirected terminal APIs | All unsupported operations return explicit error before control output/mode changes; never open hidden terminal to bypass redirect |
| Prompt | Explicit no-newline record and flush observed before blocking read; no implicit prompt/read flush; redirected prompt follows explicit caller choice |

A small injectable native-operation adapter is justified for transfer faults and
clock deadlines that real pipes cannot reliably force. Keep it inside the existing
Runtime unit-test boundary; do not expose source fault controls, add a production
validation subsystem or replace real terminal tests with mock-only acceptance.
Use real pipes and a native platform probe for the OS boundary itself.

## IR, lifetime, snapshots and compatibility

Extend existing malformed-IR tests with unauthorized private reads, const writes,
forged initializer phase/owner, skipped readonly projection, incorrect operator
signature/target and custom-equality dictionary admission. Ensure the IR checker
derives necessary capability facts instead of trusting source success/effect flags.

Force GC during collection display/custom hooks, string building, temporal/error
result publication and constructor/operator returns. Sweep allocation failure
points across builders, result/enum materialization, staged construction, native
buffers and metadata admission; output roots/frames must be valid after failure.
No C++ exception may unwind through generated source frames.

Retain an old successful analysis, delete/edit its files, and prove diagnostics,
format spans and later lowering still use that snapshot. Change private/const,
operator set/signature/effect, selected hook and format constant syntax: each
appropriate compatibility change must require restart. Compatible hook/operator
body edits publish a new generation while old calls retain old code; no callback
address or code lease enters GC/dictionary/zone descriptors. Cache cold/hit/corrupt
paths must rebind Runtime operations and account for formatter/rules versions.

## Compiler-owned tooling and real editors

Use `tools/lsp_client.py`, existing LSP CTest suites and `tests/editors/run.py`;
extend current VS Code tests and the Neovim/Vim adapters without a second semantic
engine. Cover:

1. `use console` / `use time` discovery from installed source metadata; real API
   docs and parameter help; no hardcoded editor completion list.
2. Completion/hover/signature help and navigation in typed interpolation holes,
   including numeric/temporal spec suggestions and user format/operator docs.
3. Private access contexts inside/outside owners and const-field readonly hints;
   owner-aware patterns/construction labels; operator glyph references.
4. Invalid unsaved format/operator/field source, diagnostic repair, disk refresh,
   close fallback and late old-revision result rejection. No source execution.
5. Supplementary Unicode before a hole/spec, CRLF and multiline literals; exact
   UTF-16 diagnostics, references and versioned edits from the same snapshot.
6. Guarded field/parameter rename and deliberate operator-glyph rename rejection;
   installed standard source remains readonly; preserve Vim's current rename boundary.
7. Source formatting is idempotent and token/literal-preserving for new grammar;
   no whitespace rewrite changes indentation inside expression bodies or literal text.
8. Real VS Code/VSCodium/Neovim/Vim integration on installed versions where available;
   record each actual product/version and startup/repair/shutdown result separately.

## Performance and completion record

Measure representative large print/interpolation output, many small scalar reads,
large redirected lines, cyclic/nested collection display, repeated temporal format
and changed-document editor queries. Record workload, build/optimization, machine,
elapsed time and peak/live memory separately. Compare behavior/output first; no
performance claim without measurements. Check linear output construction and
bounded-by-input parsing, not a speculative microbenchmark threshold.

The engineering acceptance document must list exact commands, pass/fail counts,
locally unavailable checks, dependency versions/digests and each completeness-row
status. A skipped platform probe is not a pass. All four systems and required
cross-system cases must be complete before the Stage 10 completion claim. Wrench
reviews evidence, and Boss controls Git, hosted CI and release decisions.
