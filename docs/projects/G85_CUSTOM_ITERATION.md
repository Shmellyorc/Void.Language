# G85 — User-defined custom iteration

Candidate based on accepted G84 `886c63155c1cf55c7e4b4c61eb0282185894c4a0`.
Prepared for independent Golden Wrench review; this report does not grant its own
acceptance. Linux qualification is distinct from Windows/macOS execution, which
remains **UNQUALIFIED**. No staging, commit, branch, push, installation or
publication is authorized or performed by this assignment.

## Implemented contract and decisions

The [canonical iteration guide](../CUSTOM_ITERATION.md) and Language Bible §35
define the delivered protocol. `iterator()` / `reverse_iterator()` return concrete
non-nullable managed or value state. `next() -> option<T>` combines advancement,
completion and value delivery. A second loop binder requires `index()` and takes
its exact type and API-defined meaning. Managed state with `deinit()` uses existing
`with` cleanup; non-resource managed/value state uses its ordinary ownership.
Factories and protocol methods are accessible synchronous zero-parameter receiver
methods. Owner generics specialize normally. There is no registry, per-collection
compiler special case, ordinal synthesis, reverse copy, new allocator or iterator
Runtime ABI.

Research was bounded by three decisions: separate collection/state creation,
a single tagged next-value operation, and deterministic resource cleanup.
Official [Rust IntoIterator](https://doc.rust-lang.org/std/iter/trait.IntoIterator.html)
and [Iterator](https://doc.rust-lang.org/std/iter/trait.Iterator.html) distinguish
collection conversion from optional next values.
[Swift IteratorProtocol.next](https://developer.apple.com/documentation/swift/iteratorprotocol/next())
provides the same combined completion/value precedent.
[C# foreach specification](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/statements#13952-the-foreach-statement)
provides a useful scoped-disposal precedent; VOID avoids its separate current-value
operation because `option<T>` already represents completion without an invalid
current state. Rust's
[DoubleEndedIterator](https://doc.rust-lang.org/std/iter/trait.DoubleEndedIterator.html)
illustrates explicit reverse capability; VOID chooses an independent reverse
factory with the same `next` contract. These are design references, not imported
patches or evidence of VOID correctness. No upstream change was adopted.

The frontend expands checked custom loops into ordinary receiver calls, an option
pattern loop and an optional resource scope. Existing typed IR independently
validates calls, capabilities, roots and cleanup activation. Existing LLVM JIT/AOT
lowering handles those operations. Built-in loop operations remain shared and keep
their version checks. Generic constraints and collection interface views may return
concrete iterator state; erased iterator state is rejected because lifecycle
requirements cannot be hidden behind an interface in the current language.

The source and factory run once. `index()` runs once after each `Some`, before the
body. `None` skips it and terminates traversal. Value yields copy; managed yields
preserve rooted identity. Factories must retain the storage their state needs.
Resource activation follows completed acquisition; normal/early/failing exits
unwind exactly once. Ordinary readonly provenance applies across all generated
calls. Custom implementations own revision checking, aliases and synchronization;
the checked example yields a typed error on invalidation, including completion.
Built-in next-access/completion invalidation and reverse remove-current rules are
unchanged.

Runnable examples:

```text
build/llvm23-debug/voidc run examples/custom_iteration/main.void --no-cache
build/llvm23-debug/voidc run examples/custom_iteration/checked.void --no-cache
build/llvm23-debug/voidc run tests/custom_iteration/imported/main.void --no-cache
```

## Defects found and corrected

- The initial expansion retained large semantic/lowering statement frames at each
  nested loop. Debug failed at 12 custom loops on a 1 MiB stack. Separating the
  recursive handlers fixed checking, then lowering; condition loops no longer
  reserve built-in iteration temporaries. A managed/deferred depth-48 probe exposed
  the remaining lowering frame and verified that final separation. The existing
  64-block parser guard also needed its loop handler separated from unrelated
  declaration temporaries so excessive nesting produces its existing source
  diagnostic. No guard or stack requirement was weakened.
- Incomplete recovered `for ... in values.` headers could reach capability analysis
  without initialized binder slots. The collection-element projection then treated
  a partial type as a collection and crashed. Loop binder validity now gates that
  projection. Original protocol frames and the native backtrace are retained.
- `defer print(...)` crashed the untouched accepted Release compiler and the early
  candidate. Deferred capture had replaced normalized display wrappers with names,
  while print lowering expected the wrappers. Capture now snapshots their raw
  arguments and retains rendering metadata; bare null stays constant. A focused
  regression covers source argument effects, null and named separator/end values.
- Parameterless ordinary method signature requests returned null. Compiler probes
  now finish for a zero-argument call, and LSP emits an empty parameter list without
  an invented active parameter. Existing documented-active-parameter policy for
  other ordinary calls is preserved. Actual editor widgets are tested.

Original failing inputs, process results, backtraces and intermediate test-harness
failures remain in the evidence directory/archive. Earlier G84/G83 failure records
and archives were not rewritten or removed. Superseded qualification/builds were
stopped when the parser stack correction changed the frozen source; those partial
runs are not counted as final qualification.

## Qualification

All ten selected Linux gates pass on the same frozen production sources and final
23.1.4 artifacts. Native test variants include the final twelve-mode synchronous
and async fixtures. Final results:

| Profile | CTest gates | Elapsed | G85 source records | G85 LSP receipts |
| --- | --- | --- | --- | --- |
| Debug | 10/10 | 138.98 s | 115 | 31 |
| Release | 10/10 | 55.83 s | 115 | 31 |
| Fatal ASan/UBSan/leaks | 10/10 | 175.98 s | 115 | 31 |

Actual **VS Code 1.144.0** and portable **VSCodium 1.143.1** each pass all six
maintained editor observations with the final Release server at a 1 MiB stack:
inferred generic/item/key hover, visible parameterless signature widget, imported
navigation/references, semantic tokens, actual completion popup in an unsaved
loop header, and invalid reverse/next edits followed by repair with a live server.
Recorded protocol token payloads additionally classify iterator/next calls as
methods and item/key declarations and uses as variables. `npm run build` passes
for the maintained editor harness. Existing stable tools/extensions were not
replaced.

Final compiler/server SHA-256:

| Profile | `voidc` | `voidls` |
| --- | --- | --- |
| debug | `0498c756bf1bdc5ed8a97ad3181a089b34130cbf137f30bb6a433dbf9e7e1091` | `60cd316a19642572faab1ca0a50be9487dfbb3c80e313f90dfa6641c9e4933d8` |
| release | `ccad1de9366e9ea3c2c77839c964166b9a2c8c2bc5211ee346e1197e9bfbc5ca` | `29967f0318739c3d1737ded229cce78d0b2a0a94eaf80a45742e01f2f9691f3f` |
| strict | `2da89ba0ca4f2df488e0b303c922879bd5589b6fe890740d17253a26926ba747` | `db53a5f717a441143c57e5c22f3efc75657d73356d8c63836dd5473080724204` |

Evidence directory:
`/home/shmellyorc/Documents/Void.Language-work/g85-xbyfkr9w`.
Review archive:
`/home/shmellyorc/Documents/Void.Language-archives/g85-custom-iteration-2026-11-20-xbyfkr9w.zip`.
The adjacent `.sha256` and `.verification.json` identify and verify that ZIP.
The archive embeds original-path/member hashes, the complete 30-file candidate,
a baseline patch with checked replay, frozen production hashes, tool identities,
final receipts and preserved original/intermediate failure evidence. Routine
compiled images and editor profiles are excluded. Archive verification checks
CRC and every extracted member SHA-256; replay reproduces every candidate byte.
`git diff --check` and new-file whitespace checks pass. Accepted G84 HEAD remains
unchanged and the index is empty.

The maintained selection is ten CTest gates:

```text
custom_iteration_source
lsp_custom_iteration
custom_iteration_runtime
custom_iteration_aot_normal
custom_iteration_aot_optimized
growable_arrays_source
dictionaries_source
generic_interfaces_source
fallible_initialization_source
lsp_editor_recovery
```

The G85 source driver performs 115 records: thirteen positive programs in fresh
JIT/AOT O0/O2 (91 check/build/run records), twenty source rejections and four
expected built-in invalidation failures. This includes empty/single/multiple
values, semantic string keys, generic forward/reverse cursors, imported constraints
and interface views, readonly rejection, value copies, nested state, scoped/deferred
cleanup, typed propagation, inaccessible factory/next/destructor methods, and
invalid protocol shapes. Three positives use 48 nested loops with value state,
managed state and deferred cleanup. Excessive nesting retains the original guard.

Native probes observe 12 exit modes with forced GC both enabled and disabled.
They cover completion, continue, break, return, propagation, body failure, failing
completion `next`, failing `index`, value state with managed fields, values escaping
iterator destruction, failing destructor and failed state acquisition. They check
closed counts, all observed precise roots, publication, empty frame chains and zero
remaining objects after observer-root release. Each profile runs 48 synchronous
JIT invocations, 48 async-JIT invocations across body suspension, and 24 invocations
in each independent normal/optimized runtime-only AOT executable: 144 total.

The LSP driver records 30 protocol/exit observations plus a final process receipt, including inferred item/key types,
readable generic cursor/option hover, zero-parameter signatures, imported definition
and references, semantic tokens, incomplete header/body completion and invalid
protocol/repair sequences with clean server exit.

All source/compiler/server/native test children use **1,048,576-byte soft and hard
stack limits**, with core generation disabled. The actual editor servers also run
through a task-local `prlimit` wrapper at the same limit. Electron itself uses its
ordinary host limits. Final qualification uses LLVM/Clang/LLD **23.1.4** exclusively
and disk-backed scratch, preserving G84's original tmpfs linker failure evidence.
No unrelated full-repository test run was performed.

Fatal sanitizer settings:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
LSAN_OPTIONS unset
```

Reproduction uses the existing configured `build/llvm23-debug`,
`build/llvm23-release` and `build/llvm23-strict` environments. Build `voidc`, `voidls`,
`void_custom_iteration_tests`, `void_custom_iteration_aot_normal` and
`void_custom_iteration_aot_optimized`, then run `ctest --test-dir <build>
--output-on-failure -R '^(custom_iteration_(source|runtime|aot_normal|aot_optimized)|lsp_custom_iteration|growable_arrays_source|dictionaries_source|generic_interfaces_source|fallible_initialization_source|lsp_editor_recovery)$'`
under the stated stack/environment. Use disk-backed `TMPDIR` and an external
`VOID_TEST_DIAGNOSTICS_DIR` to retain source/protocol receipts.

`editors/vscode/src/test/customIterationHost.ts` is the maintained real-editor
harness. It uses the existing installed extension and candidate server in isolated
profiles/workspaces; it never installs or replaces the stable tools or extension.
The final editor launcher, workspaces, settings and receipts are preserved with
the evidence. Both fixture files are checked unchanged on disk after unsaved edits.

## Limits and review boundary

Only concrete iterator state is supported. There are no associated types, iterator
interface erasure, async factories/next, yield generators, implicit typed-error
propagation or value destructors. These are explicit protocol limits, not hidden
fallbacks. Existing source async functions may use synchronous iterators across
awaits; the native regression exercises that integration. G85 adds no custom
collection version counter, locking policy or reverse-remove-current exception.
Implementers document and enforce their mutation/resource policy.

Windows and macOS execution remain unqualified. Local Linux/editor evidence does
not qualify those platforms, hosted CI, installation, release or independent
Golden Wrench acceptance. The accepted G84 HEAD and empty index remain authoritative
until Boss separately authorizes integration.

## Exact changed-file inventory

The candidate changes exactly **30 files** relative to accepted G84.

| File | Purpose |
| --- | --- |
| `CMakeLists.txt` | Register five focused G85 source/protocol/native gates and AOT drivers. |
| `docs/ARRAY_OPERATIONS.md` | Custom wrapper responsibilities and unchanged built-in invalidation. |
| `docs/CUSTOM_ITERATION.md` | Canonical protocol, state ownership, mutation, interfaces and limits. |
| `docs/DEVELOPMENT_STRATEGY.md` | G85 candidate status with independent review pending. |
| `docs/DICTIONARIES.md` | Explicit custom keys and unchanged native dictionary traversal. |
| `docs/GENERICS.md` | Owner specialization and loop item/key inference. |
| `docs/INITIALIZATION.md` | Completed state acquisition, resource activation and value lifetime. |
| `docs/INTERFACES.md` | Concrete iterator result requirements and collection-view/constraint integration. |
| `docs/LANGUAGE_BIBLE.md` | Replace Section 35 open protocol with the delivered contract. |
| `docs/LANGUAGE_SERVER.md` | Zero-parameter signature policy amendment. |
| `docs/README.md` | Guide and engineering-report navigation. |
| `docs/projects/G85_CUSTOM_ITERATION.md` | Decisions, original failures, qualification, limits and exact inventory. |
| `editors/vscode/src/test/customIterationHost.ts` | Real Code/Codium provider, visible UI and unsaved-recovery checks. |
| `examples/custom_iteration/checked.void` | Runnable custom revision checking with typed error yields. |
| `examples/custom_iteration/main.void` | Runnable generic forward/indexed/reverse collection. |
| `src/frontend/capabilities.inc` | Skip element projection until recovered loop binder slots are valid. |
| `src/frontend/compiler.cpp` | Shared parser/semantic/lowering integration, small recursive handlers, deferred print capture and hidden editor state. |
| `src/frontend/iteration.inc` | Checked factory/next/index protocol and expansion into existing loop/resource statements. |
| `src/lsp/main.cpp` | Expose zero-parameter signatures without an active-parameter requirement. |
| `tests/custom_iteration/aot.cpp` | Runtime-only AOT observer entry. |
| `tests/custom_iteration/async.void` | Synchronous protocol across async body suspension and exit paths. |
| `tests/custom_iteration/cleanup.void` | Nested scopes, defer, break, continue, return and propagation. |
| `tests/custom_iteration/editor.void` | Imported generic iterator editor fixture. |
| `tests/custom_iteration/imported/api.void` | Generic factory/state/interface declarations and visibility controls. |
| `tests/custom_iteration/imported/main.void` | Imported interface views and dependent generic constraints. |
| `tests/custom_iteration/lsp.py` | Live protocol, readable types, signatures, navigation and malformed-source recovery. |
| `tests/custom_iteration/proof.hpp` | Shared forced-GC, cleanup count, root and reclamation assertions. |
| `tests/custom_iteration/roots.void` | Managed/value state, escaped values and twelve native exit/failure modes. |
| `tests/custom_iteration/source.py` | 115 JIT/AOT, diagnostic, invalidation and 1 MiB nesting records. |
| `tests/custom_iteration/tests.cpp` | Synchronous/async JIT and AOT emission host. |
