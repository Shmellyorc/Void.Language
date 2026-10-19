# Stage 5: persistent Debug cache proposal

18 October 2026. **Delivered Stage 5 subset is hosted-verified and owner-LOCKED** at
`4627687b3a18349633715e4294bc915cf74ddd41`, [CI #70](https://github.com/Shmellyorc/Void.Language/actions/runs/39193640484):
seven successful configurations, 82/82 suites each (574/574 total), verified in
read-only job metadata and all seven CTest logs. Windows persistence bypass,
automatic-watching deferral and the other documented limitations remain.
The original implementation/research baseline below is retained as history.
Baseline: `483e67e407b65674bdc82d44767f29bd83d73ae8`,
[CI #68](https://github.com/Shmellyorc/Void.Language/actions/runs/39174408997),
seven successful configurations, 80/80 suites each. Stage 4 is hosted-verified and
locked. The approved goal is in DEVELOPMENT_STRATEGY.md; existing language,
Runtime, GC and reload contracts remain authoritative.

## First production implementation

The owner approved this design and authorized all four implementation milestones.
The implementation uses the existing compiler and Runtime; no experimental driver
or alternate compiler is required. The investigation and prototype measurements
below are retained as historical design evidence, not production timings.

### Use and ownership

`voidc run project/main.void 7 --cache` enables Debug caching. It reports
`Debug cache: miss`, `hit`, `unavailable` or `disabled` on stderr; the program's
ordinary result remains on stdout. `--cache` selects the existing no-O2 guest
configuration and is accepted only for `run`. Uncached execution, optimized JIT,
and Release AOT retain their existing paths. Debug guest code can be compiled by
a Debug, Release or sanitizer-built compiler; each build has its own identity.

Embedding hosts opt in with `Jit(DebugCache{project_root, build_identity})` and
`compile(ValidatedAnalysis, false)`. They must provide the identity of their actual
linked compiler/Runtime; the CLI embeds the generated `compiler_build_identity()`.
That function is supplied by the `void_cache_identity` CMake target; hosts with
additional compiler/Runtime inputs must identify those inputs too.
Raw typed-IR compilation, optimized compilation and JITs with custom native symbol
bindings bypass persistence. The host's existing serialized compilation/publication
contract remains in force; no background compiler or watcher is added.

### Identity and admission

CMake hashes the actual CLI object files, frontend/backend/Runtime archives and
linked LLVM libraries, together with compiler/LLVM versions, build configuration,
flags and sanitizer/RTTI settings. It generates an embedded identity before linking
the CLI, avoiding a self-referential executable hash or Git/mtime-only identity.
A build with an unidentified shared LLVM compiler library disables persistence.

Each artifact key hashes length-delimited fields for the format/build identity,
actual ORC target triple, CPU/features, DataLayout, existing default codegen mode,
selected entry, compatibility/constant contracts, requested Runtime mode and the
complete ordered reachable module/import graph with canonical paths and exact
source bytes. Module/import counts prevent grouping ambiguity. Machine restarts
reuse artifacts only when all those inputs still match. Moving the source root or
changing target/build bytes causes a safe miss. Specializations, interfaces,
layouts and constants are covered by full source validation and exact graph bytes.

Every hit first performs normal discovery, parsing, declaration/body checking,
concrete specialization and the shared entry-signature check. Warnings and entry
errors retain the uncached source locations and wording, including unused generic
entry declarations. A hit skips typed-IR/LLVM reconstruction; a miss uses the
existing validated lowerer and LLVM compilation unchanged.

The existing ORC object-transform hook copies the freshly generated relocatable
object. Cold source modules and persisted objects share generation loading: a new
JITDylib links to the main Runtime-import dylib followed by LLVM platform/process
default links, with its own ResourceTracker. CI #70 accepts the Windows native
helper resolution repair without weakening source-generation isolation.
The trusted raw-IR API retains its existing main-dylib exported-symbol linking
contract; it does not use persistence.
Canonical cache symbols can coexist in multiple live generations. Relocated
`RuntimeTypeRecord`/`RuntimeWitnessRecord` globals are admitted through existing
`vd_runtime_admit_metadata`; nothing serializes heap objects, roots or executable
addresses. NativeCode leases retain each JITDylib/session until ordinary retirement.
The candidate lease is allocated before committing Runtime metadata, preserving
the original cold-path allocation/publication ordering.
No custom native-object verifier or duplicate metadata catalog is introduced.

`Jit::replace` reuses the existing exact compatibility and Runtime identity checks.
The ToolingSession overload rejects stale/failed analysis before compilation and
rechecks its revision before publication. Candidate rejection retires only that
candidate. Existing old code/closures remain usable through their leases; an
incompatible layout/constant/source contract still requires restart. There is no
automatic filesystem-watch loop in this first implementation.

### Private disposable storage and failure behavior

POSIX persistence uses `.void/cache/v1/<64-hex-key>`. Private child directories
must be owned by the effective user with no group/other permissions. Descriptor-
relative no-follow opens pin the directories; files must be owned regular files
with private permissions. Windows currently bypasses persistence and compiles
normally: a verified ACL/path/publication implementation is deliberately absent.
macOS uses the POSIX implementation; CI #70 verifies its hosted cache suites.
Windows hosted suites verify the intentional fallback, not persistent disk reuse.

The binary envelope is 64 ASCII hex bytes of SHA-256 of the remaining body,
64 ASCII hex bytes of the key, one coordination byte (`0`/`1`), and the native
object bytes. Readers take an owned exact-length copy and verify the whole envelope
before passing its object to LLVM's ordinary loader. This detects accidental
corruption; it is not a signature or a sandbox for hostile code from the same user.

Writers exclusively create private same-directory temporary files, write and fsync
them, then publish with non-replacing hard-link creation and unlink the temporary.
A complete concurrent winner is preserved; readers never observe an incomplete
published artifact. Directory fsync is attempted. Unsupported primitives, unsafe
permissions, memory/filesystem errors and ordinary LLVM load errors fall back to
the existing compilation path. The process file-size limit is checked before
writing so optional persistence cannot trigger SIGXFSZ. Publication accepts a
non-allocating view of captured bytes; all envelope allocation stays inside the
writer’s guarded failure boundary. No production size quota
is introduced. Abrupt death can leave an ignored temporary; manual cache removal
is safe. A corrupt existing key remains a miss until removed rather than adding
locks or replacement ownership machinery. Physical power-loss durability remains
unverified. CI #70 supplies native Windows/macOS acceptance for the delivered
subset, without claiming Windows persistence or power-loss durability.

### Milestone completion and evidence

1. Content build identity and common relocated-metadata admission are implemented.
2. Real cold writes, fresh-process hits, exact-source/build invalidation and safe
   corruption/filesystem fallback are implemented.
3. Existing compatible replacement and ToolingSession revision acceptance share
   the cached compilation path and generation retirement.
4. New C++ and real-process regressions exercise isolation, old leases, callbacks/
   events, roots, collection, allocation rollback, diagnostics, imports/generics,
   concurrent writers, malformed native objects and filesystem failures. The final
   commands, production measurements and configuration results are in WORK_LOG.md.

No owner architecture decision remains open for this delivered subset. The owner
completed hosted acceptance at CI #70. The implementation and subsequent Stage 6
investigation did not trigger a hosted workflow.

### Production measurements

Measured the actual `build/ci-release/voidc`, LLVM 19.1.7, Debug/no-O2 guest code,
on Linux x86-64 / AMD Ryzen 7 1700X / 32 GiB. These are production CLI results,
not the historical research driver. All 34 launches exited successfully with
asserted results and expected hit/miss statuses. No compiler builds or test suites
ran concurrently; ordinary desktop processes remained present.

Every sample is a new process. Wall time covers process launch through successful
exit, including analysis, first execution and teardown; it is not an instrumented
first-frame measurement. Cold means an empty native cache, not flushed OS pages
or a machine reboot. Warm columns give the median and range of three launches;
uncached/cold/edited/corrupt each have one sample. No cross-platform timing or
latency guarantee is inferred.

| Workload | Uncached s | Cold miss s | Warm hit s, median (range) | Edited miss s | Corrupt miss s | One artifact bytes |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 2k (2,223 lines) | 1.895 | 1.875 | 0.064 (0.062–0.066) | 1.911 | 1.891 | 1,023,137 |
| 21k (21,243 lines) | 20.017 | 20.525 | 0.587 (0.576–0.632) | 19.878 | 20.110 | 10,184,697 |
| 105k (105,243 lines) | 142.332 | 142.689 | 5.681 (5.519–6.606) | 145.964 | 144.940 | 51,141,049 |
| combined (49 lines) | 0.188 | 0.200 | 0.011 (0.011–0.011) | — | — | 84,233 |
| 2MiB (11 lines) | 0.520 | 0.539 | 0.493 (0.486–0.497) | — | — | 21,265 |

The generated workloads contain ten imported modules with managed/value structs,
fixed arrays, generic specialization and branch/call work, reaching 5,000 concrete
step functions and 105,243 source lines. Results were 1,200 / 12,000 / 60,000;
changing one helper added one to each result. The real combined advanced-language
program returned 47; the real 2 MiB heap-array/capture program returned 17.

Peak child RSS (Linux wait4, MiB):

| Workload | Cold miss | Warm hit median |
| --- | ---: | ---: |
| 2k | 86.7 | 50.6 |
| 21k | 444.5 | 172.2 |
| 105k | 2020.6 | 940.0 |
| combined | 52.7 | 39.2 |
| 2MiB | 62.1 | 52.2 |

The 105k unchanged launch improved from 142.689 s cold to 5.681 s warm, about
25.1× in these samples; mandatory source analysis and object admission remain.
An edit or corrupt entry still costs a whole compilation. The 2 MiB workload
spends most of its time executing its array operations, so cache startup savings
are small. This does not justify unrelated Runtime tuning.

Artifacts include the 129-byte envelope. The 105k artifact is 51,141,049 bytes;
keeping the original and edited graphs used 102,282,098 bytes before the deliberate
corruption probe. No automatic eviction is implemented. Corrupt keys are not
replaced, so manual removal is required to restore hits for that exact graph.

Reproduction uses ignored `build/stage5-implementation/measure-production.py`,
copying the generated research source fixtures into private benchmark roots,
then invokes real `voidc run SOURCE 7 --no-opt` / `--cache`. It edits the first
helper once and corrupts the final cache files before their respective samples.
Raw commands/results, CPU time, peak RSS and object bytes are in
`production-measurements.json` / `.log`; focused correctness results and exact
build/test commands are in WORK_LOG.md. Production phase-internal timings were
not instrumented; the earlier research breakdown below remains labeled research.

## Historical architecture recommendation

Cache **one relocatable native object for the complete reachable source graph and
selected entry**, using the current Debug compilation configuration. Store it in
`.void/cache/`. Always run current source discovery, parsing, semantic checking,
concrete generic specialization and the existing entry-signature rule before
accepting an object. On an exact hit, reuse the native result of the previous
validated typed-IR/LLVM compilation; skip rebuilding typed IR, LLVM IR and native
code. On a miss, run the complete existing typed-IR validation and shared LLVM
lowering/verification/codegen path unchanged. No unvalidated or changed source
may reach the hit path.

This choice is supported by the experiment: retaining typed-IR construction and
backend IR preflight on a 105,243-line hit took **41.10 s**, versus **5.69 s** with
full current-source analysis followed directly by native-object loading. Repeating
IR work is unnecessary when all compiler/target/input bytes match a previously
successful checked compilation. This is reuse of that compilation result, not
replacement of the semantic checker or an alternate source compiler.

Use the same object admission/publication path for a newly compiled object and a
hit. Give each live generation its own ORC JITDylib, ResourceTracker and ordinary
NativeCode lease. Re-admit the object's existing data-only metadata into the
current Runtime. Continue whole-generation compatible reload; do not mix rebuilt
functions with another generation's private helpers or dispatch catalogs.

This first implementation deliberately recompiles the whole reachable graph after
any source-byte change. It accelerates unchanged fresh-process launches, returns
to previously cached revisions and compatible reloads to an existing artifact.
It does **not** promise cheap first-time one-function edits. Finer partitioning
would require native-unit boundaries that the compiler does not currently have.
No bitcode tier, lazy JIT, profiles, extra dependency graph or serialized AST/IR is
recommended. Release AOT and its object emission remain unchanged.

## Pre-implementation architecture inspected during research

| Path | Current behavior and implication |
| --- | --- |
| `src/frontend/compiler.cpp`: SourceGraph / validated_project | Loads root-relative canonical imports, rejects aliases, owns exact source snapshots and re-discovers/compares disk bytes before validation. Reuse this path; filenames, mtimes and watcher notifications are insufficient evidence. |
| validate_graph / generic specialization | Resolves the complete reachable graph; concrete instances use the existing checker and deterministic safeguards. Unused generic bodies retain the approved specialization-time checking contract. A cache must not demand new checks or hide existing diagnostics. |
| AnalysisAccess / ValidatedAnalysis | Owns sources, imports, declarations, references, exact compatibility and constant contracts; retained trees lower without filesystem rereads. No disk-safe native artifact identity currently exists. |
| `src/frontend/tooling.cpp`: plan_invalidation / ToolingSession | Existing declaration/reference/import closure and immutable revision acceptance handle change analysis. The plan describes affected work, not permission to publish native code. It does not currently supply independently linkable source modules. |
| lower_graph | Produces one typed module from all reachable source modules, concrete functions, synthetic capture/default/construction helpers and interface bindings. Private cross-file calls are resolved together. |
| `src/backend/llvm/backend.cpp`: lower / finish | Shared JIT/AOT lowering validates IR, outlines defaults, determines target layouts/roots, emits failure paths, source excerpts/debug paths, metadata and generation-local dispatch. `finish` verifies LLVM IR and optionally runs O2. |
| Jit::compile / Impl::load | Builds one version-suffixed module, adds it to the main JITDylib under one ResourceTracker, looks up the entry, admits host metadata and constructs a leased NativeCode. Host runtime imports are absolute symbols bound for the current process. |
| Function values / interface and virtual dispatch | Function discriminators are assigned from the sorted **whole module** function map. Dispatch, interface contracts, synthetic environments and outlined helpers are generation-wide. Source-file granularity alone cannot establish independent native validity. |
| `src/runtime/abi.cpp`: vd_runtime_admit_metadata | Already converts relocated RuntimeTypeRecord/RuntimeWitnessRecord catalogs into owning Runtime descriptors and witnesses. Reuse it instead of persisting another host metadata representation. |
| `src/runtime/coordination.cpp`: Runtime::admit_metadata | Admission requires NativeSafe state and validates/owns metadata transactionally. Buffer schemas continue through existing Runtime facilities; no heap objects are stored in the cache. |
| `src/runtime/reload.cpp`, Jit::replace | Exact compatibility and Runtime identity gate publication; active NativeCode leases retain code, diagnostics and the execution session. Tracker retirement happens after the last lease, outside ReloadSlot's publication lock. |

The current CLI calls compile_project even for `check`; it is not a parser-only
baseline. `--no-opt` skips the LLVM middle-end O2 pipeline, while LLJIT's target
machine still uses its existing default code-generation optimization level.
Debug cache policy must name this actual configuration rather than silently
changing optimization to improve a benchmark. A Release-built compiler running
Debug/no-opt guest code is different from a C++ Debug build of the compiler.

## LLVM 19.1.7 facilities and choice of artifact

Research began with the [LLVM 19 ORC design document](https://releases.llvm.org/19.1.0/docs/ORCv2.html).
APIs were then checked against installed **19.1.7** headers and the exact
`llvmorg-19.1.7` source retrieved through GitHub's read-only contents API.

- LLJIT accepts a MemoryBuffer object with an explicit ResourceTracker, and can
  look up symbols in a chosen JITDylib. Use those native facilities for hits and
  per-generation symbol isolation. See [LLJIT.h, 19.1.7](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/include/llvm/ExecutionEngine/Orc/LLJIT.h).
- Its ordinary compiler produces relocatable native objects. A narrow
  ObjectTransformLayer callback can copy the completed bytes on a miss without
  creating an AOT `main` wrapper or a second lowering path. Keep capture alive
  through materialization; do not put disk I/O under Runtime/publication locks.
  See [LLJIT.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/ExecutionEngine/Orc/LLJIT.cpp)
  and [ObjectTransformLayer.h](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/include/llvm/ExecutionEngine/Orc/ObjectTransformLayer.h).
- LLVM's ObjectCache and SimpleCompiler/ConcurrentIRCompiler support module-object
  reuse, but their getObject hook receives an already constructed LLVM Module.
  Using that hook alone would retain typed-IR and LLVM lowering on every hit.
  Direct object admission after current source validation avoids those costs. See
  [ObjectCache.h](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/include/llvm/ExecutionEngine/ObjectCache.h),
  [CompileUtils.h](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/include/llvm/ExecutionEngine/Orc/CompileUtils.h)
  and [CompileUtils.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/ExecutionEngine/Orc/CompileUtils.cpp).
- ResourceTracker removal and JITDylib removal are existing ORC operations, not a
  reason to introduce a VOID code-ownership registry. See [Core.h](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/include/llvm/ExecutionEngine/Orc/Core.h).

Native objects avoid repeat LLVM codegen. Bitcode would still require target
codegen/linking and a persisted metadata/identity policy, and would add another
artifact path. It is not portable across arbitrary compiler/runtime contracts.
No measured need for that extra tier was found; no bitcode speedup is claimed.
ORC's support for ELF/COFF/MachO does not establish that VOID's proposed load,
metadata and retirement sequence is verified on all three platforms.

## Identity and invalidation

Use LLVM Support's SHA-256 over a versioned, deterministic, length-delimited
encoding. Do not use std::hash, timestamps, concatenation without lengths, or
hot-reload compatibility as a native-code content key. Encode fixed-width numbers
with an explicit byte order; never serialize C++ object memory.

The input identity includes:

1. Cache-format and code-generation/runtime-contract versions, and a compiler
   **build-content fingerprint**. A Git commit alone misses uncommitted or
   differently configured binaries. For the CLI, generate an embedded
   build-content ID from CMake's actual compiler/runtime/LLVM link inputs and build
   configuration, after ordinary objects/libraries exist and before the final
   compiler link. Exclude only the identity object itself to avoid a cycle. Reuse
   CMake's authoritative target inputs, including CLI objects and ABI/sanitizer
   configuration; do not maintain another source-file/dependency list. Include
   relevant shared compiler/runtime library identities, or disable reuse when
   their actual loaded build cannot be identified. This covers incremental and
   uncommitted compiler builds, not just release commit names. The running image
   reads its embedded ID: reopening argv[0] could identify a replacement executable
   rather than the running compiler. Never substitute mtime/size for build content.
   The prototype did not implement this build step. A runtime whole-executable
   digest was also measured and rejected as the default: it adds 234 ms here,
   exceeding the tiny example's entire 170 ms cold compilation. The build-time ID
   is justified by that measurement, not a second language dependency system.
2. Exact LLVM 19.1.7 and relevant host Clang/SDK ABI identity; target triple,
   object format, pointer/endian/data layout, effective target CPU/features,
   relocation/code model, machine-code optimization settings and relevant target
   options. Obtain these from the actual LLJIT target configuration. Host features
   may change after a machine restart or VM migration; mismatch is a miss. Capture
   the effective JITTargetMachineBuilder configuration before passing it to LLJIT,
   then its actual DataLayout; do not infer these from the generic AOT machine.
3. Selected root-relative entry/function identity and checked entry signature;
   requested Runtime coordination (automatic versus an explicit/current Runtime
   owner, which currently forces coordinated lowering);
   Debug/O2 policy, debug-info/source-path policy, instrumentation and entry
   sanitizer magic/hash where enabled. Keep O2 experiments separate; initial
   production policy is Debug only, with Release AOT untouched.
4. The complete sorted list of **freshly discovered reachable** canonical module
   identities, resolved import edges, exact source bytes/content digests, and
   physical source paths/root used by debug information and embedded diagnostics.
   Relocating a project initially causes a miss. Source errors or missing imports
   reject the current program before object acceptance.
5. Existing source/constant compatibility contracts as additional cross-checks,
   derived from the current analysis, never loaded as authority from the cache.
   Source content plus exact compiler/target identity covers specialized bodies,
   defaults, helpers, signatures, effects, captures, interfaces and layouts without
   a second manually maintained dependency model.

Every source change, including trivia affecting locations, invalidates this whole
unit. Functions, types, constants, generic definitions/arguments, constraints,
interfaces, layouts, imports, visibility and cleanup contracts therefore cannot
accidentally retain stale code. An unrelated file outside the discovered graph
does not invalidate it. Path, option or toolchain changes invalidate it even when
source text matches. A live compatible body edit normally produces a **new cache
key** while still passing the existing **reload compatibility** check.

Do not persist Runtime addresses, native bindings' addresses, AST/Type pointers,
heap contents, Frame/Slot homes, roots, code leases or source-analysis owners.
The CLI's fixed runtime imports are covered by its build fingerprint. Initially
leave arbitrary embedding-host Jit instances uncached: existing bind_address has
no persistent host-library contract. Extend eligibility later only if a real
host can supply its existing exact ABI/build identity; do not invent a registry.

## Storage, ownership and interruption

Suggested layout: `.void/cache/v1/<key-prefix>/<key>.vdc`, under the entry project's
canonical root. One complete file contains a small versioned header, canonical
input descriptor, object length, compiler-produced entry/actual-coordination
facts and one SHA-256 covering those fields plus the object bytes (excluding the
digest slot itself). The input key identifies expected inputs; the envelope digest
detects accidental corruption of code **or** loading facts. Actual coordination is
an output of the cold pipeline, not inferred from catalog count: a scalar array
can need Runtime with no nominal types. These small artifact facts let a hit
preserve existing Runtime ownership without rebuilding IR. Metadata remains
**inside the object**, in the lowerer's existing
`__vd_types`, `__vd_type_count`, `__vd_witnesses`, `__vd_witness_count` globals.
Do not create metadata sidecars or retain process-specific relocated records.
The initial format need not persist ASTs, typed IR, source diagnostics, cache hit
counts or a mutable database/index.

Read an owned byte buffer once; validate exact header/version/key, checked lengths
against file size, envelope digest, target and normal LLVM relocatable-object
format before ORC admission. Do not reopen different bytes after validation. Unknown/trailing/
truncated data, read errors or allocation failure are misses. Counts and lengths
must be bounded by the actual file, not an arbitrary source-array size limit.
Use normal LLVM object parsing, not instruction certification. No validation of
an object file can make attacker-chosen native code safe to execute.

The cache is a **local executable build-output trust boundary**: create private
cache directories/files, reject unsafe symlink/reparse redirection and foreign or
shared-writable cache locations using platform filesystem checks. Digests are not
signatures; an attacker able to replace both bytes and digest can forge them.
Do not accept downloaded/shared cache bundles or promise safe use in an
attacker-writable project. If the private local-cache boundary cannot be
established, compile normally without persistent reuse. Remote distribution or
authenticated caches are outside this design.

Publication sequence:

1. Generate and materialize the object off to the side. Finish normal LLVM checks,
   linking, required-symbol lookup and Runtime metadata admission successfully.
   Failure must not publish a candidate to ReloadSlot or disk.
2. Write a uniquely and exclusively created private temporary file **in the final
   cache directory**; write the complete descriptor, length/digest and payload.
   Check write/flush/close results. Reuse LLVM filesystem helpers, not a lock daemon.
3. Publish with LLVM `sys::fs::create_hard_link(completed_temp, final_key)`
   and then unlink the temporary name. This installs a complete immutable file
   without replacing an existing winner. Do **not** use `create_link`, which can
   create symlinks on Unix. An existing valid entry wins; competing writers
   discard their own temporary names. Never truncate or delete a good final entry.
   Unsupported hard links or any publication failure simply skip persistence;
   execution uses the valid in-memory generation. No cross-process lock is needed.
4. Ignore orphan temporary files during reads. Initial cleanup can be manual
   removal of this disposable cache; there is no PID registry or pruning daemon.
   A corrupt existing key is a miss until cleared, rather than racing to replace
   a possibly repaired file. Every such miss still compiles normally.

There is no cross-process JIT/Runtime ownership: each process owns its read buffer,
linked memory, Runtime and generation leases. Disk entries are immutable disposable
outputs, not live-code leases. Copy/read bytes rather than retaining a mutable
file mapping; deleting the cache must not affect executing code.

Do not promise universal power-loss durability. Flush/sync the completed file
before linking and use platform-supported directory durability where available;
failure or power loss may lose an entry and cause recompilation. Length/digest
checks reject incomplete entries. Filesystems without hard links run correctly
without persistent writes; Windows NTFS is the initial documented candidate,
not every Windows volume or network share. No cache failure may turn valid source
into a failed build or hide an actual current-source/code-generation error.

This publication choice avoids LLVM's documented possible missing-destination
interval for Windows file replacement. The exact 19.1.7 Unix implementation uses
`link`, and Windows uses `CreateHardLinkW`; neither requires a VOID filesystem
transaction system. See [LLVM FileSystem.h](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/include/llvm/Support/FileSystem.h),
[Unix implementation](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Support/Unix/Path.inc),
[Windows implementation](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/lib/Support/Windows/Path.inc)
and [CreateHardLinkW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createhardlinkw).

## Fresh loading and hot reload

1. Select an immutable current source revision using the existing analysis/session
   path. Re-discover imports and fully validate current source/specializations,
   entry and compatibility records. Emit current warnings on hits as on misses.
   Hash those captured inputs rather than rereading files for
   a separate dependency graph. A watcher is only a reason to refresh/select a
   new revision, never proof of source identity.
2. Construct the normal fresh LLJIT and bind current runtime imports. For each
   candidate generation create a JITDylib with the import JITDylib in its link
   order, its own ResourceTracker and scoped entry lookup. Use a deterministic
   symbol suffix in disk artifacts rather than the process serial `__vN`.
   Different generations may then load **the same object** concurrently without
   duplicate exported symbols or changing its bytes. Keep private linkage as-is;
   do not make private functions externally visible for caching.
3. On miss, lower current analysis to typed IR and run all existing IR checks,
   lower/finish and native compilation; capture that ordinary native object. On
   hit, use LLJIT::addObjectFile directly. Both paths must converge on the same
   entry lookup, metadata admission and NativeCode construction. Factor the existing
   entry-signature rule so resolved analysis declarations and uncached IR share
   it with identical diagnostics; do not copy the scratch probe check into a
   second checker. Preserve coordinated/uncoordinated Runtime ownership and the
   existing NativeSafe guard on both paths. Use the digest-protected cold coordination
   fact and the requested coordination context in the key; a reload with an existing
   Runtime must not reuse a differently lowered automatic/uncoordinated artifact.
   This is a small backend admission refactor, not a second compiler. Avoid linking an AOT object
   with its source-program `main` wrapper into the JIT.
4. Look up the four existing metadata globals in that generation after relocation.
   Pass records/counts to vd_runtime_admit_metadata in NativeSafe state.
   The miss path should stop requesting duplicate host_catalog/host_witnesses
   vectors once both paths admit these same relocated catalogs. The ABI helper copies
   descriptors/witness strings/reference offsets into Runtime-owned storage.
   The lowerer already checks host/target record layout on generation. The cache
   key must pin that ABI; fresh loading cannot reuse pointers from a prior process.
   Dynamic buffer/schema admission remains with the existing helpers and Runtime.
5. Construct NativeCode with the current ValidatedAnalysis, current compatibility
   contracts, current Runtime identity and the candidate generation owner. For
   replacement, run existing preflight, then recheck selected source revision and
   ReloadSlot compatibility at publication. Incompatible edits leave old code and
   state active and request manual restart. A valid disk object is never permission
   to change layouts/constants/captures in a live Runtime.
6. Retire the ResourceTracker and its JITDylib only after the final existing code
   lease is released. Keep the LLJIT session/imports alive through that owner.
   Native callbacks retain their established registration/quiescence guarantees;
   closures/events retain current whole-generation behavior. Do not equate GC
   object reachability with executable lifetime or claim per-function live rebinding.

If object parsing/linking/lookup/admission fails, destroy the candidate tracker and
JITDylib, keep the active slot unchanged, and make **one** ordinary compilation
attempt. Report a failure from that real compilation normally; do not retry
forever or suppress defects. Disk read/write failure itself is nonfatal. Metadata
admission and revision freshness must succeed before publishing either path.

Automatic watching can feed ToolingSession::refresh/overrides and existing
plan_invalidation; it must not become another dependency implementation. Initially
its invalidated native work is the single whole graph. Preserve stale-result
rejection and whole-generation transactions. Watcher scheduling/debounce and any
future fine native partitions are separate implementation details, not evidence
that a cached object is valid.

## Measurements

Environment: Linux x86-64, AMD Ryzen 7 1700X, 32 GiB RAM, PikaOS 4;
Clang/LLVM/LLD **19.1.7**. Host C++ compilation was Release `-O3 -DNDEBUG -frtti`;
guest Debug used the existing `optimize=false` JIT path. Research certification
was OFF. No source/Runtime/LLVM behavior was changed in tracked code.

Scratch copies of the actual frontend/backend added scoped steady-clock timers.
The backend used LLVM's existing TMOwningSimpleCompiler through a timed wrapper
and ObjectTransformLayer to save its ordinary object. A separate executable
loaded those bytes into a fresh LLJIT, bound current runtime symbols and admitted
the existing relocated metadata globals. It used the same compiler/runtime
libraries, not an independent lowerer. The experiment accepts an explicitly named
trusted object; **it does not implement cache identity, persistent-cache lookup,
crash recovery or production error fallback**.

The real `examples/advanced/combined.void` combines properties, generics, results,
patterns, closures, events, cleanup, lists and dictionaries. Generated scale
fixtures have 10/20 imported modules, value/managed structs, a concrete generic
identity function, checked fixed arrays, branches and arithmetic; every helper
executes through the selected entry. Each helper has 20 actual source lines;
module runners call all helpers. The largest fixture has 5,000 helpers, 5,041
concrete functions, 105,243 lines and 2,380,496 source bytes. It is a generated
mixed-feature scaling workload, **not** a claim about a maintained 100k-line game.
Collection occurs under the normal Runtime allocation budget; outputs are checked.

Three independent fresh processes per measured mode; medians below. A separate
capture launch writes the object and is excluded from the cold medians. Filesystem
pages were warm; no reboot, administrator page-cache flush or CPU isolation was
performed. Times start at main and end at the first verified entry result.

| Workload | Cold pipeline (s) | Object reuse after current source + typed IR (s) | Object reuse after full current source analysis (s) | Native object bytes |
| --- | ---: | ---: | ---: | ---: |
| Real Stage 4 combined, 1,512 source bytes | 0.1701 | 0.01293 | 0.00357 | 82,648 |
| Generated 2,223 lines, 100 helpers | 1.894 | 0.2110 | 0.04256 | 1,032,840 |
| Generated 21,243 lines, 1,000 helpers | 19.931 | 2.836 | 0.4824 | 10,283,416 |
| Generated 105,243 lines, 5,000 helpers | 142.888 | 41.095 | 5.694 | 51,530,744 |

For 105k, cold range was **140.554–143.853 s**, full-IR-hit range
**40.725–42.035 s**, source-validated object-reuse range **3.653–5.833 s**.
Object linking varied substantially; report the range rather than assuming the
best run. An unmodified production `voidc run ... 7 --no-opt` on the 21k fixture
took **19.800 s whole-process wall time**, consistent with the instrumented
**20.011 s** median whole-process cold wall time.

Large-workload phase medians, milliseconds; nested rows are not additive:

| Measured phase | Cold | Source-validated object reuse |
| --- | ---: | ---: |
| Source read/owned SourceFile creation | 12.72 | 12.28 |
| Lexing | 124.37 | 121.56 |
| Parsing | 260.76 | 263.61 |
| Semantic checking, including specialization work | 2,194.28 | 2,236.10 |
| Analysis/dependency/compatibility records | 156.32 | 152.89 |
| Whole analysis including path discovery and disk consistency pass | 2,760.61 | 2,790.43 |
| Typed-IR construction including its validation | 19,217.80 | skipped, previous checked result reused |
| LLVM construction including lowerer's IR checks/layouts | 20,637.20 | skipped |
| LLVM finish/verification, no middle-end O2 | 771.75 | skipped |
| LLVM native code generation | 77,567.20 | skipped |
| Entry lookup/materialization, including cold codegen/link | 80,232.90 | 2,873.88 (link/lookup only) |
| Fresh LLJIT setup and runtime symbol binding | 0.64 | 0.62 |
| Runtime metadata admission | 0.054 | 0.056 |
| First useful entry execution | 42.16 | 39.09 |

Reading via MemoryBuffer::getFile may map the object: its near-zero open timer is
**not** a disk-throughput measurement; page touches occur during parsing/linking.
The proposed owned-read buffer, payload digest and cache filesystem checks were
not in these load timings. Source read/lex/parse include the existing consistency
rediscovery; generic body parsing during specialization is inside semantic time.
Runtime construction is under 0.01 ms here, not a demonstrated startup bottleneck.
Median peak RSS on 105k was about **2,013 MiB cold**, **1,229 MiB** with IR retained
on a hit, and **878 MiB** for source-validated object reuse. These include analysis,
LLVM/ORC and objects, not just executable memory. Native disk payload alone is
**49.14 MiB**; source size does not predict cache size closely.

Five separate Linux launch probes using a shared monotonic clock measured
**7.37 ms spawn/loader/static initialization to main**, **5.65 ms main to first
result**, and **15.58 ms whole-process lifetime**, medians for the small combined
object. Whole-process time includes teardown; the large table is main-to-result,
not a claim to have measured a post-reboot startup or a game frame.

### Integrity and build-identity cost

LLVM 19.1.7 Support SHA256, five in-memory runs per input:

| Input | Bytes | Median digest time |
| --- | ---: | ---: |
| Current Release compiler executable | 59,722,912 | 234.01 ms |
| Length-delimited 105k source identity/path/byte records | 2,383,242 | 9.32 ms |
| Combined native object | 82,648 | 0.322 ms |
| 21k native object | 10,283,416 | 40.64 ms |
| 105k native object | 51,530,744 | 201.63 ms |

These measurements justify the **build-time embedded compiler fingerprint** above.
Rehashing the compiler each launch would regress the tiny example before any
cache-format work. A build-content constant avoids that repeated work without
trusting mtimes or a stale adjacent version file. Its build-system integration
still needs implementation and rebuild coverage; it is not present in the probe.
Source/payload digest costs remain necessary. Adding those measured CPU costs to
the large direct-load median gives approximately **5.90 s**, an estimate before
owned reads/header/filesystem overhead, **not a measured complete cache launch**.
Do not infer equivalent cold-storage or reboot performance. Python/OpenSSL digest
sanity measurements were also retained, but their accelerated throughput is not
used for this LLVM Support estimate.

### Granularity comparison and edits

A fresh 21k compilation after a one-helper body change took **19.601 s**; adding
a field in an imported managed type took **20.358 s**. Both remain whole-unit
misses in this design, even though the first can be reload-compatible and the
second requires restart. The experiment compiled the edited real source graphs;
it did not pretend a persistent invalidator was already implemented.

For a deliberately favorable partition lower bound, isolate one generated source
module with its runner selected as entry, using the same compiler pipeline:

| Isolated unit | First result | LLVM codegen | Object bytes |
| --- | ---: | ---: | ---: |
| 50 helpers from the 21k workload | 0.897 s | 0.721 s | 520,648 |
| 250 helpers from the 105k workload | 4.671 s | 3.706 s | 2,601,104 |

This shows a real potential benefit for independently reusable small native units.
It is **not** a working partitioned cache: these isolated modules have no imported
native call edges, shared callable catalogs or cross-unit witnesses. A real edit
still needs whole current-source analysis (about 2.8 s at 105k here), linking,
metadata and a compatible generation transaction. Multiplying the isolated
numbers is an estimate, not incremental startup evidence. Per-function units
would increase object/link/metadata overhead further; they were not implemented.
The present whole-module helper/catalog ownership makes whole-graph caching the
simplest correct first delivery. Accept its first-time-edit cost explicitly;
finer reuse remains a measured opportunity, not an already delivered property.

### Executable correctness evidence

- Fresh-process native-object reuse returned the expected results for combined
  Stage 4 normal/O2 code, real interfaces/witnesses, cross-module calls and a
  captured **2 MiB** fixed array. The large array also loaded/executed under the
  existing **1 MiB** stack setting.
- The same object loaded into two live JITDylibs without symbol collisions;
  replacement and old-generation retirement passed. A separate body-change
  experiment published the new result (48), retained old result (47), rejected
  changed layout, survived destruction of the Jit host and collected cleanly.
  That experiment used Runtime allocation budget 1 to collect during live work.
- Current invalid source was rejected by the ordinary semantic checker before
  object loading; LLVM rejected a truncated native object. These probes test
  rejection/API behavior, not a nonexistent production cache fallback.
- Twelve competing processes through LLVM create_hard_link produced one complete
  winner and eleven existing-entry refusals. Unlinking every temporary name left
  the winner intact. This is Linux filesystem evidence, not a power-loss test.
- Existing Release focused suites: **7/7 passed** (analysis, tooling, sessions,
  reload, gc_schemas, advanced_source, advanced_cli). Strict ASan/UBSan/leaks:
  **5/5 passed** (analysis, tooling, sessions, reload, gc_schemas), using the CI
  failure settings. No production test/assertion was edited or disabled.

Exact commands, raw measurements and reproduction scripts are recorded in the
Stage 5 WORK_LOG.md entry; ignored research material is under
`build/stage5-cache-research/`. Full production/cache implementation, actual reboot,
crash-injection recovery and hosted ELF/COFF/MachO acceptance remain future work.

## Implementation milestones after approval

These are implementation/testing boundaries inside one delivery, not repeated
owner-approval gates:

1. Add the build-derived embedded compiler identity using existing CMake target
   inputs, with rebuild/configuration-change regressions. Extract one shared object admission path; isolate generations with JITDylibs,
   retain current ResourceTracker/NativeCode ownership and reuse embedded metadata.
   Prove cold behavior/reload/retirement unchanged before adding persistence.
2. Add deterministic build/target/input identity and the single-file private local
   store; capture ordinary Debug object bytes on misses and safely admit hits.
   Add bypass/removal behavior and cache outcomes to the existing development
   diagnostics path. No Release AOT changes or public embedding cache promise.
3. Connect current source/session validation and existing invalidation/reload
   preflight/publication; verify fresh-process reuse and compatible/incompatible
   edits, interrupted writers and concurrent processes. Watch notifications must
   enter the same source-revision pipeline.
4. Run consolidated local tests and equivalent cold/fresh-hit/edit measurements;
   let the owner run the unchanged seven-platform/configuration acceptance gate.
   Revisit finer granularity only with measured edit latency and a separate design
   for the actual shared dispatch/layout/specialization dependencies.

## Required regression coverage

Extend existing tests rather than adding a certification or benchmark framework:

- Independent processes: initial miss, valid hit, deletion, return to old revision,
  repeat launches and a machine-restart smoke test. Verify identical useful result,
  current warnings, error excerpts and no previous-process addresses.
- Current invalid source despite a previously good object; changed/missing/added
  transitive imports; exact-case/alias rules; overlays and stale revisions; a body,
  constant/default, type field/base, generic instance/constraint, property, capture,
  enum/result payload, interface witness/effect or selected-entry change. Unused
  generic semantics remain unchanged. Compare diagnostics with bypass enabled.
- Changing an included frontend fragment/header, CLI/backend/runtime object, link
  input or ABI/configuration must regenerate the embedded compiler ID; an unchanged
  build must keep it. Missing identity or an unidentified loaded compiler/runtime
  dependency disables reuse. Do not trust a stale adjacent version file.
- Key changes for compiler/runtime/LLVM/host ABI, CPU features, target/layout,
  optimization/debug/instrumentation, requested/actual coordination, source root
  and native import eligibility. Include pure numeric source, scalar arrays with
  empty nominal catalogs and reloads retaining an existing Runtime owner.
  Test normal/O2 generation separately where supported, sanitizer entry metadata
  and unchanged independently linked Release AOT.
- Full Stage 4 combined programs, precise roots/safepoints, collection during live
  allocation, nested fixed value copies, shared collections/references, failure
  injection/rollback/cleanup, 2 MiB arrays and existing 1 MiB stack regressions.
- Cache-hit and miss generation coexistence; same artifact loaded twice while an
  old lease executes; compatible replacement and old diagnostics; incompatible
  layouts/constants/captures; callbacks/events, native callback quiescence,
  runtime admission failure and last-lease tracker/JITDylib/session retirement.
- Corrupt/truncated header/length/digest/object, unexpected arch/format/missing
  entry/catalog symbols, version mismatch, missing/unreadable/read-only cache,
  disk-full/write/close/link-publication errors, allocation failure, unsafe cache paths,
  competing writers, reader during publication and process termination at each
  write boundary. Every case either rejects the current program for a real
  diagnostic or executes ordinary compilation safely; no invalid publication.
- Local Debug/Release compiler builds, strict ASan/UBSan/leaks, editor/LSP source
  revision behavior and packaging through established workflows. Owner-controlled
  Windows COFF/macOS MachO hosted tests are necessary; local ELF reuse is not their
  substitute.

## Historical risks and owner approval request

Whole-unit edits remain cold compilations. Mandatory current-source analysis,
integrity checking and linking set
a nonzero warm-start floor. Source-root-sensitive artifacts intentionally miss
when moved. Native object size includes debug information; disk reclamation can
initially be manual because the store is disposable, with no pruning service or
performance quota invented here. Runtime metadata/source snapshots and active
leases still consume memory even after codegen is skipped.

The experiments do not implement a durable cache, test actual reboot/power loss,
prove adversarial native-code safety or establish Windows/macOS cache support.
Production error recovery, cross-platform filesystem handling and JITDylib/session
retirement still require the regression work above.

**Subsequently approved by the owner (historical request):** accept this initial whole-graph native-object design,
including full current-source validation on hits and the complete typed-IR/LLVM
checks on misses, embedded build identity, per-generation JITDylibs,
existing object metadata admission and a private local disposable cache; then
explicitly authorize its production implementation. No language-semantic choice
is needed. A function-granular cache or remote/shared artifact distribution is not
part of this recommendation.
