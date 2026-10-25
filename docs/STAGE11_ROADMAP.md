# Stage 11 proposed roadmap

25 October 2026. **Research complete; engineering remains unauthorized.**
For independent Golden Wrench review and Boss approval.
[Verified findings, detailed semantics and decisions D1–D5](STAGE11_RESEARCH.md).

Baseline: Stage 10 tested `d507240484f94aa4782bdb8828ea1ba0017bcda6`, locked by
`909c0c33d882f4f3f2b3f25f656271d42968c0bf`, seven jobs and 1,383/1,383 main CTests.
Keep LLVM/Clang/LLD 19.1.7, current source/IR/Runtime and existing release exclusions.

## Recommended scope and dependency order

Deliver one coherent Stage 11: file-level runtime vars in every module, plus a
small useful file/packet/text library. Do not replace the whole catalog with a
framework or schedule every future module as a mandatory milestone.

Recommended initial modules:

| Import | Physical source | Proposed responsibility |
|---|---|---|
| `use io` | `stdlib/io.void` | Recoverable whole-file helpers, owned file readers/writers, EOF/errors and resource lifetime |
| `use io/memory as memory` | `stdlib/io/memory.void` | Owned in-memory stream/reader and independent packet-byte snapshots |
| `use encoding/binary as binary` | `stdlib/encoding/binary.void` | Shared exact-width byte codecs and little/big endian policy |
| `use text` | `stdlib/text.void` | Small literal prefix/suffix/split/join/replace/scalar-substring set |

`console` and `time`, print/string/format, numeric abs/clamp/limits, UTF-8 conversion
and existing collection methods remain canonical. No required `encoding.void`
facade, reexport support, repeated-name standard module entry, runtime reflection,
external package manager or hardcoded editor lists.

**Globals should come first as a semantic contract, then integrate before the
first stateful library defaults.** Pure text/codec work and explicitly owned I/O
can proceed alongside the implementation: they do not fundamentally require a
global variable. In particular, do not create a default PRNG/logger/router singleton
as a reason to rush global lifetime/reload rules.

D4 proposes blocking initial file I/O. Boss's preference for explicit paired
`_async` APIs remains a future requirement, not a claim of present support. If
Boss requires those pairs in Stage 11, include the optional dependency work below
in the authorized scope up front; do not silently drop it during engineering.

## Decisions before implementation

1. **D1, state/lifetime:** once per program Runtime, persistent across calls and
   compatible reload, fresh for a new application; explicit owner-thread access;
   independent initial JIT compiles must not accidentally share state.
2. **D2, startup/migration:** approve eager import-component/source-order startup,
   synchronous initializer effects, unready-access failure and no retry. Approve
   hoisting outermost initializers before entry statements, globals coexisting with
   main/legacy, and global-only selected entry exiting zero.
3. **D3, visibility:** choose normal qualified imports rather than implicit root
   runtime globals; preserve current const/no-shadowing behavior.
4. **D4, library scope:** approve the four module responsibilities and explicit
   blocking-first boundary, or add the real native async completion dependency.
5. **D5, observable I/O policy:** select recoverable error/progress records,
   host-path rules, endian/framing/caller-budget rules and nontransactional writes.

These are actual behavior/scope decisions. Helper names/factoring that do not alter
them are routine engineering choices, not another approval gate per edit.

## Ordered work

These are dependent engineering tasks within the stage, not separate approval/CI
sprints. Consolidate neighboring tasks when the implementation is smaller.

| Order | Task | Required result |
|---|---|---|
| 1 | Wrench reviews this proposal; Boss resolves D1–D5 | Written approved scope and explicit script/async decisions |
| 2 | Capture focused baseline and inventory existing neighboring tests | No stale rejection/history treated as current acceptance |
| 3 | Parse/register outermost vars as declarations across all source domains | Preserve block vars, fields, constants, import placement and source spans |
| 4 | Add owned global metadata and source lookup/visibility/type inference | Canonical module/name identity; alias/diamond deduplication; no-shadowing |
| 5 | Extend reference/capability and effect origins for globals | Global readonly leaves, stores/copies/returns and transitive worker restrictions |
| 6 | Represent globals and initializer functions in existing typed IR | Explicit ordered declarations/initializers; no extra expression checker |
| 7 | Add independent IR policy/readiness/effect validation | Malformed hand-built global operations cannot bypass source checks |
| 8 | Add Runtime-owned program state using existing roots/cell storage | Global-bearing programs are coordinated even with scalar-only variables |
| 9 | Establish fresh-instance/replacement Runtime admission | Distinct JIT compile/cache-hit owner paths do not merge separate programs |
| 10 | Add deterministic eager startup and per-cell readiness/failure state | Reachable import SCCs, canonical tie order, lexical declarations, no lazy recursion |
| 11 | Integrate script extraction and application/legacy selection | Approved interleaving change explicit; globals + main/entry; no new main signatures |
| 12 | Lower global access using existing heap/place/copy/root machinery | No raw address or thread-local frame retained across calls/await/reload |
| 13 | Complete global shutdown and failed-startup resource cleanup | Drain callbacks/tasks/providers, release handles before Runtime destruction |
| 14 | Extend exact compatibility and existing cache identity | Globals/init helper closure changes restart; body-only replacement keeps state; no live data in artifacts |
| 15 | Finish global compiler-owned tooling | Completion/hover/references/rename and invalid-overlay recovery use the same bindings |
| 16 | Implement one fixed-width binary codec kernel and source module | All integer widths/f32/f64, bits/endian checks, exact width; no second file codec |
| 17 | Implement memory streams with owned byte storage and one cursor | Read/exact/EOF/write/seek/snapshot semantics and failure-atomic buffer growth |
| 18 | Extend existing native resource lifecycle narrowly for files | Opaque source identity, acquisition cleanup, close/deinit/Runtime close, no general FFI expansion |
| 19 | Implement checked native file transfer/path adapters | POSIX and Windows wide operations, no managed borrows while NativeSafe, partial progress |
| 20 | Expose file stream and whole-file source APIs | Convenience delegates to shared stream/text conversion; errors handled inside ordinary result-returning helpers |
| 21 | Share typed stream codec and raw/Pascal UTF-8 operations with memory | Selected endian, u32 byte prefix, malicious length/truncation/progress checks |
| 22 | Implement the small literal text set using current UTF-8/builder | Scalar substring; explicit empty-pattern policy; no repeated whole-string copies |
| 23 | Package/source-doc the actual modules and run import/editor integration | Installed roots, nested completion, signatures, readonly standard navigation, no adapter semantic fork |
| 24 | Run the focused matrix and combined game-like scenarios | Globals + asset bytes + packet codec + hot reload + explicit cleanup |
| 25 | Consolidate evidence/contracts and return actual diff for Wrench review | Delivered scope complete; unexecuted platform results explicit; Boss decides Git/final hosted CI |

Do not add source globals merely as unrooted LLVM global variables. Code-cache
hits must rebind Runtime operations and create the same instance state as cold
compilation. Linked AOT must admit metadata before evaluating initializers and use
the same startup/failure rules; no reliance on C++ static constructor order. Ordinary
native executable publication/security and legacy status printing stay unchanged.

### Conditional dependency if async file I/O is approved now

Insert after native file transfer and before library acceptance:

1. Add a private native-file-job variant to the existing bounded executor and
   completion/admission path; owned buffers/results, no generated source callbacks
   on the I/O worker, no per-call native thread and no new scheduler.
2. Add suspend/resume/result publication and cancellation draining using current
   rooted task records and code leases. Keep cancelled operations alive until the
   underlying native request actually completes; preserve partial-write facts.
3. Deliver explicit paired whole-file APIs. An owner-loop heartbeat must continue
   while a controlled I/O job is held. Never pass by compiling an `_async` wrapper
   around a blocking owner call.
4. If async open/resource methods are included too, settle private operation leases,
   acquisition publication, close-versus-pending-I/O and public task-transfer policy.
   Existing source worker/resource prohibitions stay intact except for explicitly
   approved internal admission. Do not treat this as an automatic blanket exemption.
5. Test through the existing explicit async host adapter; extending CLI main entry
   selection is a separate language decision, not a hidden consequence of a library.

The sync implementation still remains the one OS/codec implementation. Async adds
admission/completion, not a second file semantics engine. If this extension proves
larger than approved, bring a concrete change/cost to Boss; do not claim paired
async acceptance or replace it with thread-per-request behavior.

## Focused acceptance criteria

Use existing C++/CTest source harnesses, Python subprocess helpers, allocation fault
tests and LSP/editor runners. Tests must assert observable behavior and safety,
not mirror new helper implementation. No new validation framework.

### Globals: source checking, normal/O2 JIT, linked AOT

| Test family | Minimum behavior established |
|---|---|
| Scope | Any module accepts outermost var; function/loop/conditional vars remain local; sibling branches reject; fields unchanged; no shadowing/duplicate declaration bypass |
| Visibility/identity | Own/qualified/private/internal/public, standard/project boundaries, alias and diamond identity; root constants unchanged; selected D3 behavior and collision diagnostics |
| Types/copies | Every supported storable family, nullable/default/nondefaultable, reference-bearing values, fixed snapshots, managed/list/dict identity, deep readonly and generic field/profile propagation |
| Entry | Global-only script, globals + main, globals + legacy entry, actual top-level statements + main rejection, interleaved initializer effects, explicit returns/status/overrides unchanged |
| Once/order | Multiple imports/calls and unused-but-reachable globals initialize once; source order/import order documented; unimported files never initialize; checks/LSP never execute initializer |
| Cycles | Function-only cycles, independent-global SCC, direct/indirect self-read and multi-module cycle; clear error, no partial default, recursion or deadlock |
| Failures | Initializer failure stops entry/later initialization; stored Err is normal data; subsequent calls do not retry; failure/source text remains valid after generation/source retirement |
| Isolation | Two separate programs with equal VFS names get separate state; repeated invocation/replacement uses intended state; no global symbol/hash accidentally authorizes cross-project sharing |
| Effects/concurrency | Worker reads/writes, helper/property/operator/hook indirection and foreign callback access reject safely; allowed owner-snapshot arguments still work; owner async interleaving follows current order |
| GC/lifetime | Force collection between initialization steps and after calls; every reference shape survives; failure closes acquired native resources; no source destructor after code teardown; handles gone at Runtime destruction |
| Reload/cache | Body-only change retains values/old leases; declaration/init/type/effect/helper closure changes require restart; cold/hit/fresh-process state agrees; no cached root/native pointer |
| IR rejection | Forged global IDs/types/owner/readonly, unguarded unready reads, worker global effects, incomplete publication and incompatible admission all fail independently |

No Runtime-owned cell representation is accepted on scalar-only tests. Preserve
existing constructors, properties, interfaces, composition and source-callable
capture behavior; do not turn a global store into an ownership escape hatch.

### File/packet/text library

| Test family | Minimum behavior established |
|---|---|
| Simple game use | Read settings text/asset bytes; handle not-found without crashing; write/read save bytes; initialize/reset an asset global in explicit boot/shutdown |
| Transfer/error | Partial/zero progress, EINTR, access denied, closed, early EOF, read error after prefix, write/close failure; preserve primary error/progress and owned partial bytes |
| Text | UTF-8, supplementary scalars, NUL in file content, strict malformed text errors, empty files/lines, final unterminated line, CR/LF/CRLF and mixed line/byte lookahead |
| Paths | Unicode/space/#/% and genuine Windows long paths, relative/absolute semantics, denied permissions, share/delete policy; runtime file operations do not alter VFS import protections |
| Codec | All ten numeric widths/types, signed minima/maxima, f32/f64 exact bit patterns including ±0/NaN; little/big endian vectors, exact buffer sizes, no host-layout dependence |
| Framing | Raw strings have no prefix/NUL/newline; Pascal prefix counts bytes, endian matches selection, u32 overflow rejected before output, truncated/malicious lengths and caller limits |
| Memory | File/memory byte equivalence, alias cursor, checked seek/count growth, independent snapshots, readonly input and allocation failure cleanup |
| Resource | with/deinit/manual close exactly once, aliases/use-after-close, failed acquisition/publication, read/write blocked while another participant collects, Runtime shutdown and no leaked file handles |
| Text helpers | Scalar substring bounds, Unicode literals, empty-pattern decision, split empty fields, join order, nonoverlapping replace, large linear-output workloads |
| Conditional async | Only if D4 includes it: owner heartbeat, queue bounds, pre-admission/running cancellation, completion before free/close, partial-write reporting and unchanged worker transfer rejections |

The proposed text subset is complete when its named operations behave correctly;
it does not claim Unicode case folding, grapheme segmentation, regex or every
candidate handoff operation. Likewise a blocking-only I/O acceptance is valid only
if Boss explicitly approves that scope; no `_async` feature may be marked complete
by omission.

### Tooling, installation and platforms

Extend the existing compiler-owned metadata tests plus actual stdio server tests:
global LHS/RHS completion and hover, aliases/definitions/references, collision-safe
rename with document versions, unsaved add/remove/repair, stale-result rejection,
supplementary UTF-16 positions and standard source readonly protection. Current
Vim safe-rename limitations remain. Installed module discovery must use actual
files/root overrides and nested paths, not a copied editor list. Invalid imports
and invalid initializer bodies produce diagnostics without executing source.

Use current real VS Code/VSCodium/Neovim/Vim runners where installed, recording
actual product/version/results separately. A protocol-only pass does not certify
an unexecuted native editor. Preserve source formatter/token/literal identity.

Linux focused Debug/Release and relevant strict sanitizers are development gates;
normal/O2 and linked AOT test the same source programs. Native Windows/macOS file
and resource behavior needs actual targeted native evidence when Boss authorizes
it. Do not require repeated seven-job hosted CI or claim Linux proves those OS
boundaries. Main/experimental CI and source limits are unchanged by this proposal.

## Stop/finish conditions

Before Keyboard starts, Wrench reviews the concrete recommendation and Boss approves
scope/semantics. During engineering, failure/resource/thread safety must be repaired
within authorized scope, not documented away or hidden by weaker tests. At handoff,
record delivered APIs, exact executed checks, original failures, unavailable platform
observations and any changed scope requiring approval. Then Wrench reviews the
actual implementation; Boss alone controls commit/push/branches/final hosted CI.

This research creates only this roadmap and [the research report](STAGE11_RESEARCH.md).
It implements no global storage, source API, Runtime job, native provider, package
system or workflow, and does not modify root README or Git history.
