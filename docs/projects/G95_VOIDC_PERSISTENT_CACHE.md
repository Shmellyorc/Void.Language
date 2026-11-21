# G95 — VOIDC persistent cache and fast development loop

21 November 2026. Baseline `7ad96d86a7c4613b69e162fedd6b61ef1f7381c6`.
Corrected candidate for focused Golden Wrench re-review. LLVM/Clang/LLD remain **23.1.4**.
No integration, installation, release or publication is claimed.

## Focused native-content identity correction

Golden Wrench independently verified the original 17 file identities and evidence
archive, then returned **CORRECTION REQUIRED** for one P2: an unmanaged
conventional plan could name a provider/adapter without their content identities,
yet `dynamic()` alone admitted it to persistent reuse. Rebuilding its provider
changed the result from 42 to 44 while the object still reported a hit. The new
provider executed; no native authorization bypass was observed. The original review
is preserved at
`/home/shmellyorc/Documents/Void.Language-archives/g95-wrench-review-2026-11-21/review.json`.

The correction changes only the native cache eligibility boundary. Linked thunks
and conventional plans with an empty payload remain uncached (`unavailable` when
automatic persistence was requested). Parsed freestanding closures retain exact
identities for every reachable image. Parsed managed conventional plans require
both provider and adapter in their hashed payload; the existing parser rejects
missing identities, and the existing loader verifies contents before admission.
Those complete plans, including managed raylib, remain eligible. The cache-key
recipe, native loader/approval, Runtime, ten-entry retention and `.void/run`
publisher are unchanged by this correction. No partial fingerprint, file-time
identity, extra cache, watcher or performance optimization was introduced.

`native_cache_identity_processes` creates disposable Linux projects and separately
compiled provider/adapter DSOs with the pinned SDK. Its 17 run cases verify:
unmanaged first/unchanged/provider-rebuilt/adapter-rebuilt runs stay uncached;
managed unchanged versions hit; provider-only and adapter-only content changes
miss after explicit catalog/lock refresh; restoring either original version hits
its existing entry; changed-but-unapproved contents, missing provider/adapter
identities and missing execution consent are rejected. The original candidate
fails this regression at its first unmanaged admission; that failure is retained.
The existing G5 harness now expects `unavailable` for unmanaged conventional plans
and still requires actual miss/hit behavior for fully identified managed plans.

Original reviewed source identities, failures, timings and the sole ZIP are kept.
Correction receipts are loose files under
`/home/shmellyorc/Documents/Void.Language-archives/g95-native-identity-correction-2026-11-21/`;
no duplicate archive is created. Current qualification and compiler identities
are recorded below. The older measurements and broader
qualification below describe the **pre-correction candidate**, not new performance
claims for the corrected binary. They remain accepted historical evidence.
The reported 0.576 s / 0.606 s warm medians are build-preparation time, not whole
process time. Golden Wrench independently recalculated the original process
medians as approximately 1.358 s / 1.400 s, including application execution and
controlled window closure. This correction adds no new performance target.

## Correction qualification and focused re-review

| Corrected host build | Targeted checks | Driver wall |
| --- | ---: | ---: |
| Release | 4/4 | 15.63 s |
| Debug | 4/4 | 65.37 s |
| RelWithDebInfo + fatal ASan/UBSan/leak detection | 4/4 | 77.57 s |

The four checks are `native_cache_identity_processes`, `ffi_g4_jit_ir`,
`cache_retention_processes` and `project_managed_native`. All used hard/soft 1 MiB
main stacks. This rechecks the new 17-case regression in every profile, content-
identified native cold/warm/reload sharing and old-generation leases, ten-entry
retention/concurrent writers/failed writes, and native approval plus the accepted
`.void/run` fault/ownership/cleanup protections. No full-suite rerun was added.
The Release G5 callback harness also passed its original uncached and AOT lifecycle
cases and both repeated unmanaged cache attempts, which now correctly report
`unavailable` instead of pretending to warm an unidentified provider.

The corrected Release compiler build identity is
`851880c9152800aa549410022dbe78b8dc4aa893e9cf357070a0b4cd4bde52ff`;
its executable SHA-256 is
`5e96ad2348241db3b421f419663743e1585dcce0d41ac45aa20740d1c20c34ed`.
All three profile identities and binary hashes are in the correction receipt
`compiler-identities.json`. Normal compiler-content invalidation accounts for the
new build identities; the eligible native object-key recipe was not altered.

An unchanged-source actual TestGame smoke using this corrected Release compiler
reported **miss → hit → disabled** for two ordinary automatic-cache runs followed
by `--no-cache`. All three loaded `assets/logo.png`, unloaded their textures and
returned zero through normal window closure. The driver did not edit TestGame.
It verified `--no-cache` preserved persistent object bytes and mtimes. These three
functional-smoke timings are retained as such; they are not a replacement benchmark
matrix or a new throughput claim. The original accepted build-preparation and
whole-process measurements above retain their separate meanings.

The focused correction changes six originally reviewed files plus one new test:
`src/backend/llvm/backend.cpp`, `include/void/cache.hpp`, `CMakeLists.txt`,
`tests/ffi/callback_process.py`, the current cache policy, this report, and the new
`tests/cache/native_identity.py`. The other eleven reviewed files retain their
reviewed SHA-256 identities. The original ZIP, original raw evidence and Wrench's
`review.json` remain unchanged; the P2 awaits focused independent re-review rather
than being declared independently accepted by this engineering pass.

Replay from the checkout with a fresh private temporary parent outside `/tmp`:

```sh
export TMPDIR="$HOME/Documents/Void.Language-archives/g95-native-identity-replay-scratch"
mkdir -m 700 -p "$TMPDIR"
cmake --build build/llvm23-release --target voidc void_cache_tests void_g4_tests -j2
(ulimit -S -s 1024; ulimit -H -s 1024
 ctest --test-dir build/llvm23-release -V --output-on-failure \
   -R '^(native_cache_identity_processes|ffi_g4_jit_ir|cache_retention_processes|project_managed_native)$')
```

Repeat with the retained Debug and strict configurations; strict uses
`ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Native Windows/macOS qualification
is not claimed. No source watcher, install, hosted action or Git integration was
introduced. Bounded research used the deployed content-identity principle in the
[ccache compiler-check documentation](https://ccache.dev/manual/latest.html#_compiler_check),
not a new cache dependency or a source of VOID authority. The complete existing
plan/parser/loader contract supplies the identities; incomplete plans are bypassed.

## Cause and repair

Reproduced against the actual `/home/shmellyorc/Documents/TestGame`, with its
raylib dependency, scene sources and `assets/logo.png`. The installed compiler
and a freshly built baseline compiler both reported `Debug cache: unavailable`.
Neither created `.void/cache/`. The project `.void` and `.void/run` were already
private and owned by this account. The application loaded the logo, rendered,
unloaded its textures and returned zero after an ordinary Escape event.

The cause was `Jit::compile_analysis`'s unconditional early return whenever
`analysis.c_primitive_admission()` existed. It preceded construction of
`NativeCache`, so no directory or candidate was attempted. This was an eligibility
bypass, not eviction, a permission failure, a failed admission, or an unstable key.
The CLI already attempted caching automatically. `--cache` is the retained legacy
implicit Debug/no-IR-O2 selector and status display; it never overrode this bypass.

The candidate permits **verified dynamic native plans** through the existing
native-object cache. Arbitrary host-linked C thunks remain unavailable; arbitrary
`Jit::bind` imports remain disabled. There is no replacement cache implementation.

Native hits retain the full frontend, source/dependency recapture, approval and
provider verification paths. They load the current admitted dynamic binding,
install the existing transport symbols in the new generation, admit Runtime
metadata/globals, bind the provider to the current Runtime and retain its lease.
Fresh ordinary invocations get distinct Runtime/application state. Native target,
descriptor and Runtime authority checks also apply on a hit. The cold and warm
paths share the existing dynamic-binding preparation helper.

The existing non-native object-key fields, order, envelope and `v1` layout are
preserved, with one selection-key correction for a previously absent `.void`
described below. The newly eligible dynamic family additionally keys exact descriptor,
module, target and dynamic plan bytes, including payload hashes. No transport
address, live handle, native state or staging path is serialized. A fresh approved
`.void/run/native-*` path can therefore use an object compiled with another
verified stage. Its current root still participates in Runtime/reload authority
checks. Changing a live Runtime's provider root or closure is refused.
Compiler/link input identity still invalidates artifacts on compiler changes.

A fresh native callback fixture exposed a second, independent issue: its first
publication created `.void`, adding a root directory name to the project selection
key and causing another miss on the second unchanged run. The selection-key hash
now inserts that reserved root name in sorted order when absent. Projects already
containing `.void` produce the exact previous selection recipe. Actual captured
listings, source/boundary checks and race detection remain unchanged; real source
and import identities still participate. Tests cover stable selection before and
after storage creation and a fresh manifest project's immediate second-run hit.

## Ten-entry retention

The same `.void/cache/v1/` directory now retains ten recent entries with valid
envelopes, across source, profile and compiler versions. Successful materialization
refreshes access time without changing object bytes or modification time. New
publications are recent. This is a filesystem recency policy, with filename order
as a deterministic timestamp tie breaker; it is not a new journal/database.

An advisory lock on the held cache directory serializes publication and eviction.
Reads copy complete validated envelopes under a shared lock. Native objects and
active generation leases then own their in-memory bytes independently of the
disk name. Compilation does not hold the writer lock. Concurrent misses retain
the existing exclusive temporary creation, complete write, file synchronization,
no-replace hard-link publication and temporary unlink sequence.

Eviction considers only lowercase 64-hex names that are private, account-owned,
single-link regular files with a matching key and valid full-envelope digest.
It checks inode/device/type/privacy again before unlinking under the writer lock.
No directory traversal or recursive removal occurs. Links, shared files,
directories, corrupt envelopes, interrupted `tmp-*` files and unknown contents
are preserved. Hard-linked cache files are also refused for reuse.
The constructor's held-directory and no-follow policy is retained.

Healthy completed operations converge to ten validated entries. A kill between
publication and pruning, allocation failure, unavailable locking, read-only
storage or failed unlink can delay convergence; a later successful hit/publication
retries maintenance. Unknown/corrupt/interrupted files are outside the ten-entry
count and outside automatic deletion authority. No total-byte quota is added.
This deliberately avoids scanning or deleting arbitrary `.void` contents.
Older compiler binaries predating G95 do not participate in its retention lock.

## Reload audit and demonstrated scope

`ReloadSlot`, `Jit::replacement/replace`, `ToolingSession` revision checks and
generation leases already support explicit compatible live replacement. The
focused tests exercise old/new behavior, retained old leases, native resources,
stale revisions, incompatible layouts/contracts and provider-root/hash refusal.

There is **no automatic source-watching reload loop in `voidc run`**. LSP watched
file notifications refresh analysis; they do not replace a running application's
functions. No automatic hot reload or live TestGame scene-edit loop is claimed.

Successfully compiled explicit replacement generations already use
`compile_analysis`. The candidate extends that route to verified dynamic native
objects. Dynamic source lowering is always coordinated, so its new cache family
can use the same identity for an automatically created Runtime and an explicitly
supplied compatible reload Runtime. Existing non-native automatic/forced Runtime
keys remain separate. The G4 test demonstrates both O0 and O2: cold miss, isolated
warm hit, body replacement, another replacement hit, and an ordinary fresh JIT
reusing the replacement object. It also demonstrates reuse after copying the
verified provider to a fresh stage, and refusal after on-disk provider tampering.

Compatibility still requires the existing signature, source/constant/global/native
contracts. Structural or authority changes require restart. This work adds no
watcher, reload CLI, editor command or alternate artifact store.

## Original reviewed timings and acceptance evidence

Evidence and original diagnostics are retained under
`/home/shmellyorc/Documents/Void.Language-archives/g95-2026-11-21/`.

The original measurements use the Release-built pre-correction compiler, build identity
`64a7aabd95161ffc6216486f91dccb83ca03fa51588c18ace1eb0fdee37e7ba4`,
executable SHA-256
`a9c9c4b256963a7c65aff314604e23154403f3ee268e19ef3650d1ff9db026f6`.
Debug below means the **project profile**, not a Debug-built host compiler.
Each profile has one absent-object cold run and four unchanged warm samples,
after task builds/tests finished. OS filesystem caches were not flushed.

| Actual TestGame project profile | Cold miss | Warm median (4) | Warm range | Edited miss | Restored hit | `--no-cache` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Release / IR O2 | 1.016 s | 0.576 s | 0.571–0.610 s | 1.065 s | 0.578 s | 0.951 s |
| Debug / no IR O2 | 0.976 s | 0.606 s | 0.599–0.608 s | 1.006 s | 0.587 s | 0.911 s |

Both exact status sequences were `miss, hit, hit, hit, hit, miss, hit, disabled`.
The fresh baseline Debug run was **0.945 s**, with caching unavailable and explicit
per-invocation native consent. The installed compiler reproduced the same bypass
at approximately **0.98 s**. Cold persistence adds hashing, writing and flushing;
it is not expected to beat an uncached run. The measured improvement is reuse.

Representative inclusive Release scopes, in milliseconds, from the cold report
and unchanged warm run 2:

| Scope | Cold | Warm |
| --- | ---: | ---: |
| Full frontend | 494.289 | 501.814 |
| Dependency verification (inside frontend) | 65.342 | 55.341 |
| Semantic analysis (inside frontend) | 206.942 | 217.315 |
| Analysis assembly (inside frontend) | 166.673 | 174.142 |
| Cache lookup / identity | 11.366 | 24.055 |
| Typed IR | 64.427 | skipped |
| LLVM IR | 43.714 | skipped |
| LLVM verification | 7.592 | skipped |
| LLVM IR optimization | 96.321 | skipped |
| JIT materialization, including cold machine generation | 206.950 | 4.336 |
| Cold object publication | 40.275 | not applicable |

The frontend accounts for most remaining warm time. Warm Debug lookup is larger
for its approximately 5.19 MB object; Release objects are approximately 2.60 MB.
The final TestGame store contains six private entries, including two from the
initial candidate, all below the history limit. Exact names/sizes/modes are in
the external state receipt. No persistent compiler artifacts were placed outside
the project's `.void/cache/v1/` by these real-game runs.

A separate **warm-hit real terminal approval** run took **1.379 s**, including
**0.774 s** in the authorization scope. Its driver deliberately waited 0.75 s at
the prompt; this is not a prediction of human reaction time. Two unchanged runs
using exact saved consent and fresh private native stages still hit, at
**0.613 s** and **0.616 s**; their authorization scopes were **34.652 ms** and
**28.751 ms**. Redirected invocation and CI saved-consent reuse both refused with
status 2. Consent was saved only inside a disposable isolated user-data fixture.
The real user approval store was not changed.

All final game runs loaded `assets/logo.png`, unloaded textures, closed the window
and returned zero. Final measured application durations were **0.676–0.980 s**
for Release and **0.698–0.775 s** for Debug; these include controlled closure delay.
Exact original application bytes were restored. `.void/run` remained empty and
retained its original inode **1476585553**.

The maintained real-game driver is `tools/bench/cache_project.py`. It runs the
explicit compiler against actual TestGame, records schema-1 JSON timings and
stdout/stderr, finds only that child's X11 window, sends an ordinary Escape event,
and checks asset loading, resource unloading and zero exit. Its edit cycle appends
a comment and restores exact source bytes. Behavioral body/import invalidation is
also checked by the focused source-cache tests. It never purges project caches.
Application durations include initialization, rendering and the driver's deliberate
window-close delay; they are not game-loop throughput measurements.

Timings are inclusive monotonic wall scopes; phase sums are not totals. Cache hits
still parse, analyze and assemble validated source, verify native dependencies,
hash/copy the object and materialize native code/Runtime metadata. Dynamic loader,
JIT setup and recency maintenance are not all separately timed by the existing
schema. Approval timing includes the authorization operation and its storage work;
saved-consent reuse may have a small nonzero authorization scope. No 36 ms pass
threshold or sub-frontend warm-run promise is introduced.

## Original qualification and replay

The maintained coverage includes automatic O2/O0 policy separation, diagnostics
before reuse, fresh globals, source/import/build invalidation, corruption fallback,
same-key publication races, ten-entry eviction with twenty competing distinct-key
writers, restored-version hits, open readers and fourteen still-live native
generations after disk eviction. Failed file-limit/open/write/sync/publication
operations preserve prior history; test faults affect only the fixture cache's
temporary files. The native suites cover dynamic resource/callback authority and
the existing `.void/run` failure, replacement, partial marker and killed-writer
cleanup protections. Production staging/approval/Runtime source is unchanged.
The separate G5 callback harness also exercises cold/warm cached normal, implicit
shutdown, retired-resource and checked-failure modes in both optimization modes.
It is invoked directly without its optional compiler-install arguments.

| Final host build | Focused CTests | Driver wall | Separate G5 cached lifecycle harness |
| --- | ---: | ---: | --- |
| Release | 9/9 | 24.85 s | passed; 16 cache-enabled cases |
| Debug | 9/9 | 118.14 s | passed; 16 cache-enabled cases |
| RelWithDebInfo + fatal ASan/UBSan/leak detection | 9/9 | 105.51 s | passed; 16 cache-enabled cases |

All 27 focused cases passed with hard/soft 1 MiB main-stack limits inherited by
their children. The separate G5 harnesses used the same limits and also exercised
their existing uncached and independently linked/relocated AOT cases. The selected
cases are the nine names in the replay command below; no whole-compiler or hosted
suite is claimed. `git diff --check` passed.
The three `voidls` executables were then relinked against the current frontend,
and the LSP-using `project_managed_native` case passed again in all three profiles
(12.32 s / 57.78 s / 64.77 s). This targeted follow-up verifies the refreshed
server rather than relying on the older executable used by the first selection.

Preserved unsuccessful investigation attempts are identified explicitly: editing
a test while Clang was reading it caused a build bus error; an added G4 assertion
initially checked the unchanged input-7 branch instead of the edited input-91
branch; and the first callback warm assertion exposed the reserved-directory
self-invalidation repaired above. The original diagnostics remain in the evidence
archive. No Runtime guard, stack limit, failure assertion or timeout was relaxed.

Use a task-owned temporary parent outside source/docs and `/tmp` for this replay:

```sh
export TMPDIR="$HOME/Documents/Void.Language-archives/g95-replay-scratch"
mkdir -m 700 -p "$TMPDIR"
cmake --build build/llvm23-release --target voidc voidls void_cache_tests void_g4_tests void_project_selection -j3
(ulimit -S -s 1024; ulimit -H -s 1024
 ctest --test-dir build/llvm23-release --output-on-failure \
   -R '^(debug_cache|debug_cache_processes|automatic_cache_processes|cache_retention_processes|ffi_g4_jit_ir|project_mixed_native|project_managed_native|cli_presentation|project_selection)$')
python3 -B tools/bench/cache_project.py build/llvm23-release/voidc \
  "$HOME/Documents/TestGame" "$HOME/Documents/Void.Language-archives/g95-replay" \
  --label release --runs 5 --edit-cycle
python3 -B tests/ffi/callback_process.py "$PWD/build/llvm23-release/voidc" \
  "$PWD/.toolchains/llvm-23.1.4/bin/clang" OFF
```

Use fresh report labels/destinations; reports never overwrite an existing file.
Repeat focused qualification with the retained Debug and strict build directories.
Strict qualification uses fatal ASan/UBSan and leak detection. Actual execution is
Linux x86-64 PikaOS with X11/Mesa and raylib 6.0. Native Windows/macOS execution is
not qualified. Windows retains the existing safe unavailable persistent-cache
path; no unverified ACL implementation is added. Benchmark application processes
and the focused compiler/native tests use 1 MiB main stacks where recorded; build
orchestrators are not described as 1 MiB executions.

## Exact changed-file inventory

| Path | Change |
| --- | --- |
| `CMakeLists.txt` | Register the bounded-cache process test and configured C compiler for its syscall shim. |
| `include/void/cache.hpp` | Clarify verified dynamic-plan eligibility. |
| `src/backend/llvm/backend.cpp` | Native hit admission, dynamic-plan identity, existing loader helper sharing and compatible reload artifact reuse. |
| `src/backend/llvm/native_cache.hpp` | Successful-materialization recency notification. |
| `src/backend/llvm/native_cache.cpp` | Directory locking, private single-link validation, envelope helper, recency and bounded eviction. |
| `src/frontend/project_selection.inc` | Stabilize the previously absent reserved `.void` name in selection identity without changing real directory observations. |
| `tests/cache/automatic.py` | Require a genuine immediate second-run hit in a newly created manifest project. |
| `tests/cache/native_identity.py` | Provider/adapter-only rebuilds, unmanaged bypass, managed warm/restored hits, incomplete identities and integrity/consent rejection. |
| `tests/cache/tests.cpp` | Verify live generated code survives eviction of its disk artifacts. |
| `tests/cache/retention.py` | Ten-entry history, recency, restoration, races, active readers, hostile contents and failed-write assertions. |
| `tests/cache/fault.c` | Test-only Linux partial-write/quota, file-sync and publication faults. |
| `tests/ffi/dynamic.cmake` | Link dynamic-cache tests with the real compiler build identity. |
| `tests/ffi/dynamic.cpp` | Native cold/warm isolation, reload persistence/sharing, fresh-stage reuse and tamper refusal. |
| `tests/ffi/callback_process.py` | Cached O0/O2 native callback lifecycle and checked-failure process cases. |
| `tests/project/selection.cpp` | Selection key remains stable when compiler-owned storage is first created. |
| `tools/bench/cache_project.py` | Reproducible real TestGame timing/asset/exit/source-restoration driver. |
| `docs/compiler-health/stabilization/CACHE.md` | Current cache policy, native eligibility, bounded retention and watcher limitations. |
| `docs/projects/G95_VOIDC_PERSISTENT_CACHE.md` | This closeout and independent-review handoff. |

There are **18 maintained changed files** after the focused correction. The original
17-file inventory is preserved as `pre-correction-changed-files.json` in the
correction evidence directory; the current exact inventory remains at
`g95-2026-11-21/changed-files.json`. Generated benchmark receipts, source
snapshots and the exact SHA-256 inventory live outside the checkout. Build outputs
remain with the reused build configurations. The final closeout itself remains in
this checkout to avoid a self-referential archive hash.

## Original verified archive and storage handoff

Archive:
`/home/shmellyorc/Documents/Void.Language-archives/g95-review-evidence-2026-11-21.zip`

SHA-256:
`fe1266552f5e0a0a4aebe23781892243caceae99a8fd7592fd299ccd42bdf8de`

Ordinary ZIP/DEFLATE level 9, **228,102 bytes**. All **84 payload members**
(16 original maintained source inputs and 68 evidence records) passed CRC and extracted
SHA-256 checks against `_ARCHIVE_MANIFEST.json`. The external `archive-index.json`
contains the matching receipt. This unchanged archive preserves the pre-correction
candidate; corrected source hashes are in the current loose inventory.
No executables, objects, providers, toolchain
copies, keys or installations are archived. Current exact inventory and final
storage metadata are `g95-2026-11-21/changed-files.json` and
`g95-2026-11-21/storage-receipt.json` beside the loose receipts.

Reused Release/Debug/strict configurations and active SDK/provider inputs are
retained. Disposable harness fixtures clean themselves; the task-owned retained
callback diagnosis fixture and compiler crash scratch are removed after their
selected diagnostics are verified in the archive. Initial available space was
approximately **209 GiB**; final availability is approximately **208 GiB**, with
exact final cleanup observations in the storage receipt. No older build tree,
historical archive or arbitrary project storage is removed.

No staging, commit, branch creation, push, installation, hosted action or Wiki
publication is authorized by or performed for G95. The installed `voidc` remains
unchanged; use the candidate build paths for review.
