# Combined sprint implementation and Wrench handoff

Scope and all public rules are in [CONTRACT.md](CONTRACT.md). The concise
[receipt](evidence/RECEIPT.md) records the final local selection, exact outcomes,
commands, hashes and benchmark samples. This is implementation evidence for
independent Golden Wrench review, not a declaration of final platform acceptance.

## Sequential engineering

1. Audited the Language Bible, Wrench governance/design review, accepted console,
   documentation, standard-library handoff and binary/memory contracts, actual
   compiler, Runtime lists/typed buffers/roots, display adapters, generics and all
   four initially installed source modules. No AGENTS.md was present. Measured
   the original linked-O2 sequential writes before modifying memory source.
2. Added init-free managed `error.error` with exactly three documented fields.
   Verified defaults, inherited named construction, ordinary object upcasting,
   exact result propagation and native/JIT behavior before migrating APIs.
3. Migrated every installed public error payload, retaining categories/native
   progress. Native temporal kernels remain unchanged. Added one explicit
   canonical temporal-result display bridge, preserving the accepted string-based
   built-in formatting protocol and all ordinary user hook signatures.
4. Added ordinary source console input conveniences over checked endpoints, with
   complete public documentation and readable constructors/control flow. Existing
   advanced console source/native behavior remains under the preserved regressions.
5. Implemented the full convert surface, with a narrow canonical-module scalar
   parser and checked result extraction. Numeric grammar/range rules live in one
   small Runtime translation unit; ordinary source owns derived errors and result
   policy. Enum tables reuse the already validated closed declaration cases.
6. Added transactional byte bulk overwrite/append to the existing Runtime list,
   exposed only inside canonical memory source. Source prepares a replacement
   state before mutation; capacity growth uses existing typed buffers and precise
   roots. Added independent IR capability proofs and focused failure tests.
7. Reproduced the newly reported completion crash on the actual Debug server.
   An isolated pre-repair Debug reconstruction supplies the debugger stack. Installed
   all lazy global resolvers before declaration analysis and prevented pure constant
   candidates/typed anchors from starting runtime-global inference. Added persistent
   malformed-edit/recovery tests with the verbatim game and minimal const/global
   fixture, including actual 1 MiB stack execution.
8. Ran integrated local gates and completed the public metadata/module/error,
   formatting, ownership, performance and source-hash audit recorded below.

## Necessary compiler/Runtime support

* Exact `?` now compares nominal E directly. The previous general `expect` accepted
  managed subtypes and could defer an invalid widening to IR validation. Ordinary
  object inheritance remains valid; explicit negative/positive tests retain this.
* Generic typed-anchor inference now recognizes enum case expressions using the
  same declaration lookup as ordinary enum checking. This supports ordinary
  `convert.to_string(mode.case)` without evaluating an argument during inference.
* Four private typed IR operations cover scalar parse, checked result extraction,
  memory bulk write and canonical temporal display adaptation. Validation checks
  operand/result types, flags, canonical owners where applicable, and enum payload
  shape. Independent source/IR capability analysis preserves unwrap provenance and
  checks memory mutation authority; these are not public language intrinsics.
* Temporal hook validation admits only the canonical time module's managed error
  result. Built-in display adapters convert that explicit boundary; arbitrary
  user hooks still require the accepted string result protocol.
* Runtime u8 writes validate before mutation and retain geometric capacity. Growth
  stages complete backing with BufferBuilder and read leases. In-place writes have
  no fallible operation after memmove. Source has no fallible step after successful
  byte publication. Existing allocation-budget probes verify this boundary and
  retry, including spare-capacity append/overwrite cases.
* `from_chars` provides locale-independent actual-width numeric conversion. No
  numeric representation codec, UTF-8 decoder, formatter, allocator, exception
  framework, collection framework, general reflection or public syntax is added.

## VOIDLS completion repair

The original Debug server returned completion error -32603, printed
`Internal language server failure: bad_function_call` and exited 2. The debugger
stopped at `std::__throw_bad_function_call`, called by the empty
`std::function<void(Global&)>` in `Semantics::editor_candidates`, reached through
`analyze_constant` and `validate_graph`. This confirms the reported initialization
order defect, including explicitly annotated globals whose types were not prepared.

The resolver closures are now installed for every unit before semantic declaration
passes. Their initializer evaluation remains in the existing later phase. Pure
completion and typed-anchor lookup respect the existing prohibition on reading
runtime globals from constant expressions. Local runtime contexts retain lazy
inference. No broad exception handler, server restart, or extension workaround was
added. Repeated requests and corrected overlays must recover in one server process.

The regression includes file/local constants, inferred/typed/forward/imported globals,
cycles, invalid annotations, defaults, fixed extents, the supplied complete guessing
game, missing API diagnostics, and three cursor positions in the minimal fixture.
[Original failure](evidence/voidls-crash-before.log),
[actual debugger stack](evidence/voidls-crash-stack.log), and final per-profile
receipts distinguish reproduction from verification. The stack executable was
reconstructed from the exact Debug compile/link commands in an isolated /tmp tree,
reverting only the three repair sites after the original server was rebuilt; it did
not alter the checkout or managed build objects. The downstream VS Code pending-edit
error occurred after server termination; this patch repairs the confirmed compiler
cause.

## Evidence and corrected intermediate failures

The first foundation executable passed ordinary/O2 JIT and linked AOT. Initial
memory probes passed 9,496 checks across 12 operations; two extra spare-capacity
scenarios were subsequently added. Integrated public metadata initially verified
268 declarations and zero warnings; final hierarchy assertions additionally prove
all five module errors derive from the exact three-field canonical base.

Intermediate failures were retained and repaired before final gates:

* Source-native call results cannot infer the private generic temporal bridge's
  payload; explicit concrete private type arguments fixed this without changing
  public inference. Single-line function declaration headers are existing grammar;
  attempted wrapped headers were restored. Calls/constructors retain readable
  multiline arguments. No new continuation syntax is claimed.
* Both source and independent IR display-hook validation needed the explicit
  canonical temporal exception, alongside the message bridge. No implicit result
  conversion or general hook-signature expansion was introduced.
* A generated signed-minimum negative test accidentally classified -128 as outside
  i8; its intended below-minimum spelling was corrected, retaining the assertion.
* Existing integrated source used a capture named `error` with unqualified time
  imports. It now uses `reason`, following existing no-shadowing rules. The initial
  focused log retains that failure; final gates use the repaired fixture.
* LSP queries initially selected an earlier matching expression, and error-kind
  construction fixtures omitted required enum fields. Query positions now select
  the intended trailing call and constructors explicitly name their category.
* The CLI has no async root adapter. Async verification uses the existing explicit
  C++ scheduler/JIT and linked native harnesses instead; no CLI language feature was
  added. The source still passes ordinary CLI checking.
* The new ordinary console `read` declaration conflicted with one legacy LSP local
  binding. That fixture now uses `received`, retaining the same checks.
* A console PTY harness included complex Debug JIT compilation in a 60-second input
  deadline. An isolated run measured 55.869 seconds under link load before producing
  the exact expected bytes and restoring terminal state. Its process deadline now
  matches the existing 120-second runner; key reads retain their 2-second timeout.
  The full console selection has a 2400-second outer limit. No assertion was removed.
* The final temporal ownership extension initially expected an imported custom
  hook to become visible inside convert’s generic definition. That contradicted
  existing module visibility. It now checks caller-scoped `string(day)`, explicit
  managed-error formatting, and the built-in string-error adapter under forced GC
  and allocation failures. The helper’s ordinary visibility limit is documented.
* The wider fixture audit found three remaining time-to-string propagation sites
  in operator async/workload/cache fixtures. They now explicitly retain the temporal
  error message in their existing string error channel, preserving worker-transfer
  types and every original success/failure assertion. Existing operator GC/effect,
  allocation recovery, reload/cache and full JIT/native AOT suites verify the boundary.
* Console installation checks caught documentation edits made after their initial
  library snapshots. Production sources were frozen and the complete affected
  console gates rerun with the installed-source equality assertion intact. These
  failures remain in the integrated logs; corrected runs are separate receipts.
* Adding two ownership cases initially inserted their boolean invariant branch
  into the i32 dispatcher too; that duplicated test-only block was removed before
  final failure sweeps. Original operation assertions remain intact.

## Completeness and remaining platform limits

The installed inventory is exactly error, console, encoding/binary, io/memory,
time and convert. The automated metadata audit checks every public declaration,
explicit parameter and meaningful return plus all five derived error bases. The
contract table lists every extra error field. No public source result exposes an
unintended string/enum error payload; private native/display bridges remain internal.
The console contract comparison retains every prior advanced operation and adds
only the two ordinary convenience functions. Conversion covers all ten numeric
primitives, bool, payload-free enums, display and both generic result helpers.

All evidence is local Linux x86-64 with the pinned official LLVM/Clang/LLD 19.1.7.
Windows/macOS, physical terminals/keyboards, native big-endian hosts, and physical
multi-GiB u32/i64 growth limits were not tested in this sprint. Existing guards
remain source-audited; no fabricated native or allocation-size pass is claimed.
ASan/UBSan/leaks are strict; vptr is unavailable with the SDK's accepted RTTI-off
ABI. Benchmark samples are local wall-clock measurements, not a cross-platform
performance guarantee. Numeric decimal rounding and finite-only parsing are
explicit contract choices. No new automatic error display or prelude is introduced. `convert.to_string`
retains ordinary generic-definition hook visibility; it does not inherit caller
imports. Caller-scoped `string(value)` remains available for those custom hooks.

Root README, Git refs/index/history and release controls are untouched; no hosted
CI ran. Historical binary/memory acceptance evidence remains intact with current
contract links. The local receipt records final green cases and all earlier
corrections. Stopped for Golden Wrench source/evidence review; Boss retains commits,
pushes and the final platform matrix.
