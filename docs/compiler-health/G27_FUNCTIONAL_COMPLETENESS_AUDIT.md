# G27-R — functional completeness and integration audit

13 November 2026. Starting and final code baseline:
`5aa51bdd1edbf1146fb64379497b2a1fb0b78561`. Research only. Production sources,
tests, build configuration, root README and Git index were not modified. No
repository, package or hosted CI was published or dispatched.

## Executive conclusion

**VOID is already functionally usable for ordinary synchronous applications in
its qualified Linux x64 and Windows x64 environments.** This audit ran a combined
application through source checking, normal/O2 JIT, independently linked native
execution, package verification and execution after relocation and moving its
source directories. It used modules, a local generic library, a package, runtime
globals, managed identity, fixed-array copies, dictionaries, closures, results,
numeric conversion, Unicode, formatting and application arguments.

The smallest justified next milestone is **finish the reusable-library authoring
workflow and reconcile current onboarding/status documentation**. Two local
changes suffice for the exercised workflow: emit the already-known library
starter as `package.entry`, and, if Boss approves extending maintenance to library
consumers, remove the application-only `add` guard. A disposable Release candidate
demonstrated both without changing resolution, publication, security or Runtime
machinery. This is feasibility evidence, not production acceptance.

No new P0 functional blocker or P1 correctness defect was demonstrated. That is a
bounded audit conclusion, not a proof that the entire compiler is defect-free.
Async execution exists, but an ordinary CLI application still cannot host an
async root. That explicit, consequential hosting limitation needs an owner scope
decision; removing its safety guard would not be a repair. Public release and
unqualified platform obligations remain separate from functional usability.

## Evidence and scope

The investigation read the current [Wrench instructions](../WRENCH_CLANKER.md),
[strategy](../DEVELOPMENT_STRATEGY.md), [historical completeness audit](../LANGUAGE_COMPLETENESS_AUDIT.md),
[milestone register](../roadmap/MILESTONE_REGISTER.md), [local libraries](../LOCAL_LIBRARIES.md),
[G26 qualification](../projects/G26_PACKAGE_MAINTENANCE.md), and relevant current
language, source/compiler, entry, async, collections, generics, native and editor
contracts. It compared those records with the frontend, project parser/resolver,
creation/publication, LLVM entry paths, Runtime and LSP source and maintained
fixtures. Old proposal tables are not current missing-feature inventories.

All three existing Linux `voidc` binaries match the final G26 compiler SHA-256
receipts byte for byte. Both native Windows compilers likewise match G26.
`--version` reports LLVM **23.1.4** on all five. Exact identities are retained in
the research evidence. No active compiler build was replaced or rebuilt.

Linux is PikaOS x86-64. The running VirtualBox guest labeled “Windows 11” reports
actual Windows 10 build **19045**. Fresh Windows probes used the x64 VS 2022
developer environment: VSCMD **17.14.42**, MSVC **14.44.35207**, SDK
**10.0.19041.0**. Its compilers target `x86_64-pc-windows-msvc`. Test files were on
private guest NTFS storage created with the existing native staging verifier;
the shared folder transported scripts and receipts only.

This was not a complete requalification, sanitizer proof of generated JIT code,
performance campaign, public-origin test or macOS test. Accepted G13–G26 results
are reused with their original scopes. No debugger/ptrace was needed.

## Current capability and developer-journey matrix

| Workflow / capability | Current implementation and evidence | Disposition |
| --- | --- | --- |
| Create an application | `project_creation.cpp::plan_creation/publish_creation`; manifest/physical-root validation and exclusive publication. Fresh baseline S5 process fixture passes all 93 invocations. | Already implemented |
| Select and run ordinary source | `project_selection.inc` selects a unique eligible synchronous main under manifest roots, with explicit override and legacy standalone adapters. No mandatory filename in manifest mode. `FIRST_PROGRAM.md` describes the actual path. | Already implemented |
| Functions, exact numeric types, conversions and control flow | Shared Parser/Semantics, concrete generic specialization, typed IR validation and LLVM lowering. Current calls/numeric and advanced-language contracts supersede the 20 October audit. Fresh combined conversion fixture exercises integer extremes, typed results, enums, callbacks and errors in normal/O2/AOT. | Already implemented within documented type rules |
| Structs, properties, interfaces, closures, cleanup | Existing construction/capability/inheritance handlers and independent IR checks, precise Runtime roots and code-generation leases. Maintained language/ownership tests and accepted integrated milestones provide wider coverage than the fresh fixture. | Already implemented; not all advanced OOP proposals are supported |
| Arrays, dictionaries and algorithms | Fixed-array independent values; shared list/dictionary identity; checked operations, slices, iteration invalidation and readonly. `stdlib/collections.void` reuses those types. Fresh combined fixture verifies fixed copies and shared managed/dictionary values. | Already implemented; no new collection engine needed |
| Results, patterns and recoverable errors | `pattern_*`, ordinary result propagation and shared checked Runtime bridges. `tests/combined/conversion.void` exercises nested results, nullable payloads, exact error records and callback payloads. | Already implemented |
| File-level globals | Frontend global registration/lowering and Runtime owner/init/root rules; the imported conversion fixture's counter initializes and runs correctly in all freshly exercised execution modes. | Already implemented; not unrestricted worker-shared state |
| Async/tasks/workers/timers/native async IO | Existing async lowerer, `compile_async`, `emit_async_object`, Runtime Scope/Work and completion ownership. G10 and HTTP/IO evidence are retained. Ordinary CLI still uses the synchronous adapter. | Implemented host API; intentional CLI limitation, finding F3 |
| Standard library | Actual source includes console/time/text/encoding/convert, collections, random/math, JSON, channel/log, io/path/os/process, archive/crypto/url and net address/TCP/UDP/HTTP modules. These are not proposed empty module names. Qualification remains module/platform specific. | Already implemented; no new stdlib prerequisite identified |
| Ordinary local source libraries | `lib/...` and quoted imports, project-owned source, no manifest required. Fresh generic `lib/util` composes with installed `pkg/...`. | Already implemented |
| Generate an importable package | `new --kind library` checks successfully but omits explicit `package.entry`, which G26 `add` requires. | P2, F1 |
| Add dependencies to a reusable library | Resolver/check/tidy support library dependency graphs, but `add_dependency` explicitly admits only application consumers. | P2 command boundary, F2; owner approval to extend |
| Local and pinned Git acquisition | G26 add/tidy, D3 full-pin/proof and whole-repository limits, copied external local sources versus live in-project paths, immutable Git views. Local path journey freshly exercised; Git tests reused from accepted G26 to avoid new Git mutations. | Already implemented; no registry or implicit native authorization |
| Read-only dependency validation | `tidy --check`; G26 compares bytes, times, membership and administration state. Fresh consistent local graphs pass. | Already implemented |
| Native libraries and C import | G25 derives physical source/descriptors/adapters from pinned Clang; G3/G4/G5 ownership/callbacks and D4 admission remain authoritative. G24 contains 600 usable raylib interfaces with explicit adaptations. | Already implemented in qualified profiles; exclusions below |
| JIT/O2/native build | One validated semantic/IR path, existing ORC and native builder. Fresh source output agrees exactly across normal/O2 JIT and independently linked AOT. | Already implemented |
| Package and relocate | D5 package manifest, dependencies/notices and verification. Fresh package verifies before/after relocation and runs after its source roots move. G26 supplies stronger accepted native-provider relocation coverage. | Already implemented for supported targets |
| Editor workflow | Compiler-owned semantic metadata, immutable sessions, source/manifest ownership, version checks, G13 independent open-document analysis, G20 physical-root recovery and G21/G22 lifecycle repairs. G23 packages through existing trusted project tasks; G26 real VS Code consumer checks passed. | Already implemented; no second resolver/editor engine needed |

The fresh combined application does not cover every row's entire contract. For
example, it does not exercise arbitrary interface families, retained native
callbacks, GUI devices or cancellation. Those claims rest on the specific
maintained implementations and accepted records, not on its successful exit.

## Ranked findings and smallest corrections

### F1 — P2: generated libraries need an avoidable manual manifest edit

**Source:** [project_creation.cpp](../../src/frontend/project_creation.cpp),
`plan_creation`, lines 977–1009, especially 1002–1003;
[project_resolution.cpp](../../src/frontend/project_resolution.cpp),
`add_dependency`, line 815. The parser already accepts `package.entry` in
[project.cpp](../../src/frontend/project.cpp), lines 646–656.

Reproduction, with an admitted empty parent directory:

```sh
voidc new app
voidc new geometry --kind library --package-name geometry
voidc check --project geometry
voidc add geometry --project app --path ../geometry
```

The last relative path is resolved against `app`. Equivalent absolute-path probes
failed on all five profiles with exit 1 and
`Source must be a VOID library with an explicit package.entry.` Checking the
generated library succeeds. Adding `entry = "src/geometry.void"` under
`[package]` makes the same add/tidy sequence succeed.

This is an integration usability gap between S5 creation and G26 consumption,
not evidence that library checking or package admission is broken. S5 intentionally
generated minimal metadata before G26. The maintained S5 process test at
`tests/project/creation.py:144` explicitly expects only package ID/name; G26's
helper at `tests/project/maintenance.py:57–68` manually supplies package entry.
Those tests cover their original contracts but conceal the unedited command-to-command
journey. The manual workaround and manifest-free local libraries justify P2 rather
than a claim that no basic application can be built.

**Smallest tested candidate:** immediately after serializing the library package
name, serialize `entry = toml_string(*starter)` iff `!app && starter`.
One conditional statement; no parser change, resolver inference or file search.
The actual default/custom starter path is already owned by the creation plan.
Do not set `project.entry`, invent main, or guess an entry for `init`.

Risk is small and explicit: generated manifest bytes change. Update the exact
expected manifest and test the new→add journey rather than disabling that
assertion. Existing collision/identity/security/publication behavior stays in
the same implementation. No further architectural research is required.

### F2 — P2: add is unnecessarily narrow for library authors, but deliberately guarded

**Source:** `project_resolution.cpp::add_dependency:781` rejects any consumer
whose manifest kind is not application. `tidy_dependencies:871–900` has no such
restriction and uses the same Resolver and publication machinery.

All five baseline profiles reject an otherwise valid library consumer with
`add selects an application manifest.` The G26 CLI synopsis does not prominently
explain that restriction. Existing manual dependency declarations remain usable.
This is not an observed type-system or resolver correctness failure.

**Smallest tested candidate:** remove that one guard, retaining all root checks,
existing-lock validation, source validation, D3 capture and
`publish_package_edit` calls. The disposable candidate created a library depending
on another library, checked/tidied it, installed it into an application and ran
the resulting transitive graph in normal/O2 JIT and native/D5 execution. The
unchanged production compiler subsequently accepted the same graph in four
tidy/check/run invocations. Existing machinery already provides the capability.

An initial candidate fixture added the same copied local package both directly
and through the consuming library. It correctly failed **VOID5007 dependency
origin conflict**: `libs/geometry` and `libs/consumer/libs/geometry` are independent
editable physical sources despite carrying the same package ID. This is a useful
negative control, not justification to merge identities or weaken origin checks.
The successful fixture uses one transitive origin. There is no automatic public
re-export or permission to depend on an undeclared transitive alias.

**Owner decision:** include library consumers in `add`'s supported contract, or
document its application-only scope and retain manual library maintenance. Recommend
the extension because the local experiment required no new architecture. Native
and Git library-consumer cases, process interruption and all native candidate
profiles still need focused engineering qualification. A successful local-source
experiment does not establish every acquisition/provider combination.

### F3 — Intentional limitation: async roots need an explicit host adapter

**Source:** [main.cpp](../../src/main.cpp), lines 611–620 calls synchronous
`jit->compile`; [backend.cpp](../../src/backend/llvm/backend.cpp):2562 and 2784
reject async/synchronous adapter mismatch. Existing `Jit::compile_async` is at
2712; `emit_async_object` at 2998 sets the async-root path.
`project_selection.inc:529–530` requires synchronous automatic main.

Fresh Release/Debug/strict source probes checked this program successfully:

```void
async func entry(input: i32) -> i32:
    return input
```

Ordinary run then fails with `Async entry requires the explicit scheduler root
adapter`; ordinary build fails with its AOT equivalent. Source check validates
a declaration without promising host entry compatibility. Thus check/run
differences here are not by themselves a semantic inconsistency. Async `main`
also parses/checks as standalone source; it does not satisfy automatic manifest
main selection. The fresh Windows probes did not exercise this boundary.

The [async contract](../ASYNC.md) explicitly preserves host-only adapters.
Existing IO/HTTP/worker fixtures already execute through them; their existence
must not be reported as missing async implementation. The task-address index and
completed-epoch sweep guard are also present in `runtime/async.cpp:194,449,510`;
the old blanket retention diagnosis is superseded.

This limitation matters if “ordinary developers” includes standalone asynchronous
network clients/servers using source async entry. Recommend an explicit product
decision after the small library workflow correction. A future approved hosting
change must reuse these adapters and define application exit/args, root drain,
cancellation, failure, cache identity and native-main integration. Removing the
mismatch checks or renaming an async function is unsafe. No async candidate,
new scheduler or implicit source wait API is proposed by this audit.

### F4 — P2: current navigation still presents superseded implementation status

Concrete examples:

- `DEVELOPMENT_STRATEGY.md:3–13` says all project/application features remain
  unimplemented. Its current table still describes TCP/UDP as a future track.
  The actual project code, `stdlib/net/{tcp,udp}.void` and accepted G07 contradict
  reading those entries as present-day status.
- `SOURCE_COMPILER.md:1–45` says project sessions, metadata and general C ABI are
  later slices; G23–G26 source and tests already use them.
- `roadmap/MILESTONE_REGISTER.md` still describes G25/G26 as locally uncommitted,
  while this task's clean starting commit contains them. Preserve the original
  reports' dates and local qualification scope; update current navigation only.
- The Bible's arrays/dictionaries sections and old completeness audit retain
  “not implemented” statements later explicitly superseded by their linked
  contracts. They must not drive a new compiler work list.

The current `FIRST_PROGRAM.md` gives a working manifest/check/run/build path and
correctly restricts automatic main to synchronous form. The correction is a
small current-status/onboarding reconciliation, with links to accepted scopes
and the library workflow; not a rewrite of historical evidence or root README.

## Intentional boundaries and work not justified by this audit

| Boundary | Current consequence / disposition |
| --- | --- |
| Library check versus library executable | Libraries have no fabricated executable root. Direct run/build of library kind is intentionally unsupported. `package.entry` describes imports, not `project.entry`. |
| Type-system exclusions | No ordinary overload sets, generic interfaces, higher-kinded types or universal implicit coercions. Specialization-time checking is the approved generic model. First-class tuples and async/function-value multiple returns remain outside G15. P3 expansion only with a concrete need. |
| Equality / collection keys | Tagged enums use patterns rather than automatic structural equality; G18 improves guidance. Dictionary key restrictions, custom equality/hash exclusions, iterator invalidation and readonly are real contracts. Do not loosen them to make examples compile. |
| Local source ownership | External `add --path` copies editable source; in-project paths remain live. Copied packages with the same ID but different origins conflict. G26 is recoverable process-interruption publication, not a hostile filesystem or power-loss transaction. |
| Native APIs | C-compatible value imports and reviewed ownership/callback adapters exist. Arbitrary C++ classes/templates/exception transfer, unchecked pointers and arbitrary variadics are not promised. No new FFI or loader is needed to close F1/F2. |
| Public raylib acquisition | Canonical development and verified private HTTPS consumers work. The monorepo's public Git origin failed D3 limits. Standalone publication is still Boss-controlled and unperformed; no public GitHub success claimed. |
| Platform scope | Linux GNU x64 and Windows MSVC x64 managed-native/D5 scopes have accepted evidence. Earlier macOS application/context/editor acceptance does not qualify current managed native/D5, sockets or every later module. Those boundaries remain explicit. |
| Editor and debugging | VS Code project/manifest integration is ahead of Vim/Neovim parity; Windows PDB/IDE and JIT source debugging are not covered by accepted native DWARF/GDB work. Guarded rename/recovery subsets remain deliberate. |
| Performance and snapshots | G08/G19 rejected unsafe completion reuse; G21/G22 fixed lifecycle problems. Full capture/revalidation is not redundant merely because expensive. No new cache, background service or broad optimization justified here. |
| Historical incidents | Retain unresolved optimizer incidents, Windows symlink prerequisite error 1314, G18 executable cleanup refusal and external raylib/Gallium leak receipts. No fresh occurrence or causal repair was established. No retries erase them. |
| Public distribution | Existing release/licensing/platform/trust obligations belong to the current release records and Boss decisions. This audit neither clears them nor creates an additional release HOLD. |

## KISS/DRY and upstream comparison

The frontend remains one `SourceGraph`/Parser/Semantics implementation. Discovery
and execution have different admitted source sets, not independent language
parsers. `lower_graph` at `compiler.cpp:6911–7037` validates typed IR before LLVM.
Tooling consumes owning analysis records. `lsp/main.cpp:280–334` independently
publishes valid open-document diagnostics and clears previously published owners,
after snapshot acceptance; reverting to graph-only diagnostics would reopen G21.

The shared project creation plan knows exactly which source it generated.
Serializing that fact is simpler than adding package-entry inference. The shared
Resolver already handles the library graph. Reusing G26's one publication journal
is simpler than a library-specific maintenance implementation. No duplicated
frontend, IR, scheduler, source index, package cache or validation framework is
needed for the recommended milestone.

Relevant official upstream references, consulted 13 November 2026:

- [Cargo target configuration](https://doc.rust-lang.org/cargo/reference/cargo-targets.html)
  separates library source targets from executable targets and supports explicit
  manifest-relative source paths. This is a useful comparison for making generated
  library metadata consumable. It does **not** authorize importing Cargo discovery
  or relaxing VOID's explicit-entry/security contracts. VOID's own disposable
  experiment supplies the implementation evidence.
- [LSP 3.17 specification](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)
  remains the protocol reference for document synchronization and language
  features. Protocol compatibility is not proof of a particular client's native
  qualification; G21/G22/G26 records remain the evidence for VOID's behavior.

No Windows-specific failing filesystem API emerged from the fresh probes. The
same frontend condition fails before dependency publication on both operating
systems. Changing Win32 directory sharing, ACLs, path aliases or DLL search would
be unrelated to the observed failures. No external platform workaround was
substituted for native evidence.

## Disposable experiments and actual results

Candidate sources were copied outside the checkout. Only two statements differed:
F1's conditional serialization and F2's guard removal. The copied files compiled
with the existing Release compile recipes and linked against the unchanged
existing archives. Candidate `voidc`/`voidls`, object files, ThinLTO cache and
generated native products stayed in task scratch. Exact patches, recipes,
identities, fixture sources and results are retained.

| Experiment | Actual outcome |
| --- | --- |
| Baseline Linux Release/Debug/fatal ASan+UBSan with leak detection | 12 recorded commands per profile. F1/F2 failures reproduced; manifest workaround succeeds; async-host refusals as above. No abnormal compiler exit or sanitizer finding. |
| Native Windows 10 Debug/Release baseline | 19 commands total including private-root creation and version checks. Both F1/F2 failures reproduced, workaround/add/tidy succeeds. Exact compiler SHA-256 values match G26. Private scratch removed. |
| Unmodified maintained S5 process fixture, production Release | **93/93 invocations pass.** Includes creation, CLI/editor shared plan, native build/run, collisions, invalid paths, symlink/permission refusal and concurrent creators. |
| Same unchanged fixture, disposable candidate | **Fails the existing exact package-map assertion at line 144**, after 48 commands. Preserved, not reported as a passing unchanged suite. |
| Disposable copy of that fixture with only expected `package.entry` added | **93/93 invocations pass** against candidate compiler/server. No safety assertion removed. |
| Candidate creation boundaries | Four pass: default library entry, custom library entry, `init` library has no guessed entry, application still has no generated explicit entry. |
| Complete baseline application journey with explicit library manifest workaround | **15 recorded commands pass**, including the expected initial add refusal, then check/JIT/O2/AOT/package/verify/relocate/run. |
| Complete candidate transitive-library application journey | **19 commands pass** without manual entry/dependency manifest edits. Same execution and package path. |
| Unchanged compiler consuming candidate-created transitive graph | **4/4 pass:** library tidy/check, application tidy and execution. Confirms existing resolver/lowerer support. |
| Duplicate copied local package origins | Candidate refuses with **VOID5007**. Preserved negative result; no origin checks changed. |

Exact successful source output was
`conversion passed\nresult=049 args=2 unicode=雪😀\n` in normal JIT, O2 JIT,
linked AOT and relocated execution. The package executable is at its package
root, not an assumed `bin/` subdirectory. Moving source roots demonstrates source
independence, not an SDK-free clean-machine installation or isolated native
provider test. G26 retains the broader existing relocation evidence.

Three harness/setup errors were retained and corrected without product changes:
the first disposable link script removed the wrong `-Xlinker` pair; the first
stdout assertion omitted the maintained conversion fixture's own success line;
the first relocation harness assumed `bin/`. None was a compiler defect. The
duplicate-origin refusal was a real, intended product diagnostic, distinct from
those harness mistakes.

Reused accepted G26 evidence: Linux Debug/Release/strict **34/34** focused cases
per profile plus **2/2** final generated-native compatibility cases; native
Windows Debug/Release **18/18** plus **2/2**. G26 also retains local HTTPS pinned
Git, D3/D4/D5, G5 callbacks, G25 import and real VS Code qualification. These are
**prior accepted results**, not fresh G27 executions. Original device leaks and
earlier unfavorable receipts remain unchanged.

Not run: full suites, hosted CI, new Git acquisition, candidate Windows/Debug/
strict qualification, new graphics, current macOS, or a public raylib repository.
Candidate work is therefore suitable for review and a bounded implementation
assignment, not for declaring all-platform engineering completion.

## Recommended single next implementation assignment

**GO for Wrench/Boss review of library-authoring integration**, not implementation
authorization from this report. No new production code is installed.

1. Add F1's single conditional `package.entry` serialization to the existing
   creation plan. Amend the precise generated-manifest contract and add the
   unedited `new library → add → tidy --check → import` regression. Cover default
   and custom source paths, while preserving `init` and application discovery.
2. If Boss approves F2, remove only the application-kind guard. Reuse the existing
   Resolver and `publish_package_edit`; extend the same maintained G26 fixtures
   to library consumers and a transitive application. If not approved, document
   the restriction and manual manifest workflow; do not create a second command.
3. Reconcile current status/onboarding links for this workflow and G23–G26.
   Keep historical reports, failure evidence, qualified platform limits and the
   existing root README intact unless separately authorized.
4. Run focused creation/manifest/dependency/maintenance/import/LSP regressions on
   the three existing Linux profiles and native Windows Debug/Release. Test
   direct and transitive local sources, existing immutable Git fixture reuse,
   conflicting origins, stale locks, collision/case/alias/permission failures,
   native approval remaining separate, and the existing interruption/recovery
   checks. Preserve `tidy --check` nonmutation. No new CI campaign is required by
   this research.

Acceptance is behavioral: generated libraries are immediately consumable; approved
library consumers can maintain dependencies; exact output agrees in JIT/O2/AOT
and relocated D5; check/tooling never executes startup/native providers; the
existing security, publication, root and diagnostics guarantees remain intact.
The original manifest assertion must be deliberately revised, not bypassed.
New candidate failures need their original receipts and exact inputs.

Stop and review if implementation requires weakening origins/native approval,
guessing unknown entries, changing library/application identity, or duplicating
publication. Nothing in the tested path justifies those changes. Async application
hosting is a separate owner scope decision; it is not hidden inside this small
milestone. P3 language conveniences do not gate it.

## Storage, retention and final inventory

Inspected G26 footprint: `build/g26` **348 MiB allocated** and
`/home/shmellyorc/VM/g26-20261113` **6.4 MiB**. Most G26 storage is generated
native/relocated raylib products: approximately 140 MiB strict Git, 97 MiB Debug
installed Git, and 51–52 MiB in each Release Git tree. Those generated products
are potential cleanup candidates after checking active consumers and preserving
their identities/replay receipts. They are not unique source by themselves.
Retain the handoff JSON/XML, exact source/locks/catalog hashes, failure/device
logs and replay coordinators. Two small `/tmp/g26_{creation,maintenance}.inc`
files also need an ownership/replay comparison before deletion. **No G26 artifact
was deleted or changed.** No broad cleanup is justified by current free space.

Research scratch was `/tmp/void-g27r-rmfo3dn1`; Windows transport was
`/home/shmellyorc/VM/g27r-20261113`; private guest scratch was
`C:\Users\a\VoidG27Research-20261113`. The Windows receipt confirms that guest
scratch was removed. Selected G27 receipts, patches, recipes, exact source/manifest
fixtures and replay scripts are archived using the existing DEFLATE-9/checksum
policy; disposable candidate executables, objects, caches and package copies
are not permanent deliverables.

Verified archive:
`/home/shmellyorc/Documents/Void.Language-archives/g27-functional-audit-2026-11-13.zip`.
It contains **145 files**, **247,061 original bytes**, compressed to **87,293
bytes**, plus its embedded `_ARCHIVE_MANIFEST.json`. ZIP SHA-256:
`f264f3802f9df165391c3b138107da833369ac2771362debe2bf011a86ad4934`.
CRC and every extracted member's SHA-256 passed; original files were rechecked
before cleanup. The existing external `archive-index.json` records the archive.
Replay scripts select their own containing directory; run them from the original
repository baseline with its matching active builds. The Windows replay needs
the recorded native environment and compiler identities. Receipts retain original
absolute paths, which are historical rather than still-live destinations.

After verification, only G27's scratch and transport directories were removed;
the guest private root had already been removed by its coordinator. Linux scratch
held **135,774,208 allocated bytes**. `/tmp` available space increased from
**16,377,290,752** to **16,513,069,056 bytes**; root filesystem available space was
**362,376,323,072** before and **362,376,372,224 bytes** after. These filesystem-wide
figures include unrelated activity, not a guaranteed archive compression saving.
The external `g27-functional-audit-2026-11-13-cleanup.json` preserves the measurements.
Active builds, toolchains, G26 evidence and all unrelated work were retained.

The sole repository deliverable is this Markdown file. There are no staged
changes or production diffs; final status is the untracked report only. Starting
HEAD remains unchanged. Return to Golden Wrench and Boss for independent review.
