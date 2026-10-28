# OS module: Golden Wrench source-review handoff

27 October 2026. Boss baseline: `0d014e851851203e30d3b376ca99606a67d36a7a`.

The complete approved **13-function module is implemented** with Linux, Windows
and macOS source adapters, compiler/IR/LLVM/Runtime integration, both native link
routes, documentation, examples and focused tests. Linux local engineering
verification is complete. **Native Windows/macOS compilation and execution are
unverified**, and independent Golden Wrench acceptance/release approval is not
claimed. Stop here for independent source review and Boss's platform decisions.

[Public API and boundaries](API.md) · [engineering/failure record](IMPLEMENTATION.md)
· [measurements](BENCHMARKS.md) · [file inventory](FILES.md)
· [approved contract](DESIGN.md) · [acceptance scope](ROADMAP.md).

## Delivered implementation

`stdlib/os.void` exports data_dir, config_dir, cache_dir, state_dir, home_dir,
temp_dir, executable_path, resource_dir, env, platform, architecture, process_id
and is_bundle. All public functions, error fields and enum cases have source docs.
The managed error derives from the common base and retains operation, stable
one-based kind/code, native code/domain and an owned value-free explanation.
Path-helper io.error adaptation is explicit; error types remain separate.

One closed operation table selects canonical stdlib-only intrinsics. Frontend
capabilities and independent typed IR reject transitive worker access. A single
LLVM lowering serves normal/O2 JIT, cache loading and linked AOT. The native ABI
validates exact reply geometry against compiler DataLayout. PID has a genuine
integer output, never the native error slot or formatted text, with zero managed
string allocation. Empty strings retain Runtime's normal null representation.
No query executes during checking, editor analysis, lowering or object population.

Native inputs are copied while managed. NativeSafe contains only owned native
work; publication reenters managed execution and roots every output. Scoped
buffers, COM ownership and Foundation autorelease scopes unwind before the
noexcept boundary. Native allocation/invariant failures retain Runtime handling.
There is no hidden environment/location cache, native scheduler or persistent
resource provider.

Windows uses redirected Known Folders, strict wide APIs, balanced COM including
S_FALSE and RPC_E_CHANGED_MODE, and unsigned PID/HRESULT conversion. Apple uses a
private Objective-C++ Foundation adapter, create:NO, native home/temp, genuine
APPL .app classification and actual file resource URLs. Both CMake consumers and
installed native-build flags include their shell/COM or Foundation/objc dependencies.
The Objective-C++ target explicitly uses C++20 and the configured compiler/RTTI/
SDK/architecture/sanitizer policy. Compiler staging code is unchanged.

## Actual local results

| Configuration | Consolidated selection | Final OS recheck | Effective unique suites |
|---|---:|---:|---:|
| Linux Debug | 25/25 | 14/14 | **25/25** |
| Linux Release | 24/25 initially, one fixture failure retained | 14/14, including correction | **25/25** |
| Linux strict ASan/UBSan/leaks | 25/25 | 14/14 | **25/25** |

This is **75/75 effective configuration/suite outcomes**. The repeated OS checks
are not added to the unique total. Actual CTest attempts were 117: 116 passed,
one initial fixture failure. The 11 adjacent suites per configuration cover
filesystem kernel/source/async errors, GC schemas/buffers, Runtime globals,
compiler-owned globals/documentation LSP, documentation execution and object cache.
No failure was suppressed or expectation weakened.

Consolidated receipts:

- [Debug 25/25](evidence/acceptance-debug-v57uat2a/receipt.json), [final OS 14/14](evidence/acceptance-debug-os-final-rsocaqz_/receipt.json).
- [Release initial 24/25](evidence/acceptance-release-s555eqwd/receipt.json), [corrected final OS 14/14](evidence/acceptance-release-os-final-m4kaijj9/receipt.json).
- [Strict 25/25](evidence/acceptance-sanitizers-p06njp6m/receipt.json), [final OS 14/14](evidence/acceptance-sanitizers-os-final-lis01l6r/receipt.json).

Each directory includes JUnit per-suite output, timings, complete configuration,
commands, strict environment, source hashes and tool hashes. Strict flags were
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. These instrument
C++ compiler/Runtime code; they do not establish sanitizer instrumentation of
every generated LLVM application instruction.

Subsequent focused additions also passed in all three configurations:

- **127 host assertions** after adding independently constructed reply/field/PID
  geometry mutants, preserving exact declaring-owner metadata.
- **92 process checks per configuration on actual hard 1 MiB stacks**, including
  explicitly simulated Flatpak/Snap/AppImage roots, JIT/O2 and executed linked AOT.
- **8 owner-async process checks per configuration**, through the existing async
  host and independently emitted/linked/executed AOT, including safe transfer of
  an owner-discovered string snapshot to a worker.
- Concurrent cached process checks passed: **10 process checks** in Debug/strict
  final suites and the Release supplemental check. Two live child PID markers
  gate execution, then the same code observes each process's environment/PID.

[Supplemental commands, results and hashes](evidence/supplemental.json) identify
these later source/test variants separately from the CTest receipts above.

## Acceptance matrix and limits

| Area | Evidence/status |
|---|---|
| Complete public surface | All 13 signatures, error fields/cases, parameter/return docs, API inventory and three working examples. No excluded API stubs. |
| Application IDs | 9 valid/61 invalid tokens x four getters in normal/O2 JIT and AOT; validates 1/255 vs 0/256 bytes, device stems/extensions, Unicode, traversal, separators and NUL. Broken native roots prove validation precedes discovery. |
| Linux roots | All four independent overrides, missing/empty/relative/absolute malformed cases, HOME precedence/passwd fallback, TMPDIR rules and real current-user lookup. Linker-owned test seams verify ERANGE/no-record/native errors/ENOMEM. |
| Launcher conventions | Explicit simulated Flatpak, Snap and AppImage environments preserve supplied roots. Actual packaged launcher execution **not verified**. |
| Windows roots/COM/Unicode | Implementation and native-host fixtures present, including stale last error, empty environment, Unicode names, long values, malformed UTF-16, namespace strings and STA/MTA ownership. **No native compile/execution pass**; real redirected/offline folders, UNC IO, long filesystem round trips, Win32 retry/fault cases and import libraries still need native verification. |
| macOS Foundation/bundles | Implementation and real APPL/plain/moved-bundle test script present. **Not compiled/executed here**; arm64, relative native image results, actual nil/error/exception fixtures, Finder launch and signed App Sandbox/container behavior remain native verification work. Rosetta is unclaimed. |
| State placement/roles | Linux selected state/default .local/state exercised; Windows Local/id/state and Apple Application Support/id/state implemented. No rotation, retention or runtime/session alias. |
| Cwd reuse | Both existing path.absolute spellings agree against independent native cwd with existing trailing spelling; Unicode cwd source execution. Selected io_kernel retains deleted-cwd and explicit-base cases. No os cwd operation. |
| PID | Independent pre-exec native PID vs JIT/O2/AOT, repeated and independent Runtimes, simultaneous child identities and cached fresh processes. Shared production DWORD widening helper covers 0/INT32_MAX/high bit/UINT32_MAX; actual Windows API unverified. |
| Runtime/session exclusion | Frontend rejects nonexistent runtime_dir/session_dir; ordinary env lookup remains available without security certification. |
| Environment | Missing vs empty, exact Unicode/newline/equals, Unicode names, POSIX case sensitivity, malformed native bytes and names, owned copy after mutation, 100 KB executed-process values and 2 MiB in-process heap values. Windows-specific behavior pending native runs. |
| Image/resources | Independent executing image/PID, unrelated cwd, launch symlink, long heap readlink seam/truncation, missing procfs/access/unsupported/system errors. Real AOT image named ` (deleted)` unlinks itself and retains both literal/kernel suffixes. |
| No filesystem mutation | Nonexistent root trees remain absent after discovery. Save example creates/writes only through explicit io. Temp discovers only a base. |
| Paths/security | Native path joining/parent reuse, preserved parent components and supplied roots; no case/Unicode/shell/symlink normalization or permission promise. Windows/macOS native path round trips remain pending. |
| ABI/frontend/IR | Canonical ownership, operation bounds, arity/input type, scalar and six-field reply geometry, wrong flags/text/i64, owner metadata, forged worker reachability and missing coordination reject. Actual JIT/AOT call the same ABI. |
| GC/failures | Runtime(1) collections during publication, independent collector while native, precise outputs, native/managed allocation fault budgets, native ENOMEM Runtime failure, original domains/codes and no PID string allocations. Strict leak runs pass. Native COM/autorelease fault verification pending their hosts. |
| Owner/globals/reload | Owner async works, all direct/transitive query workers reject, transferred immutable snapshot works. Eager global observation runs once, fresh calls requery, independent Runtime/cache state remains fresh, compatible body reload retains global value, collection/close retain/release roots. |
| Cache | Real normal/O2 object miss/hit, silent automatic cache and --no-cache bypass; separate children observe fresh env/PID/globals. Windows persistence retains existing unavailable policy. |
| Tooling | 44 source-owned LSP assertions: import/member/error/enum completion, hidden private reply, alias docs, signature help/parameters, definition/references, protected rename, incomplete overlays/repair and UTF-16. Adjacent existing stale-result/documentation/global suites pass. |
| Install/AOT | 14 compiler/tooling install/relocation/direct-link process checks per profile, plus executed build/emit outputs in both optimization modes. Actual Linux CMake-linked host exercises the Runtime. Windows/Apple native link routes pending native execution. |
| Stack/profiles | Real soft+hard RLIMIT_STACK=1,048,576 for frontend, JIT/O2, AOT compilation/execution, long outputs and native host. Initial host stack suite had 115 assertions; later normal host strengthened to 127. No stack enlargement. |
| Performance | Native discovery vs bridge/string publication vs whole source getter measured separately from analysis/compilation. See BENCHMARKS; no hidden caching or speculative optimization. |

## Failures, evidence preservation and review boundary

[IMPLEMENTATION.md](IMPLEMENTATION.md) explains every retained correction. The
existing crash recorder also retained ordinary rejected invocations and output
assertions; the `evidence/crashes` name does **not** mean these were native crashes.
No new signal/ASan/UBSan crash was observed. Historical H1–H7 are separate,
unresolved incidents, with no repair claim and no ptrace use.

Original later failures retain command, status, stderr, exact fixtures, imported
source/provenance and binary hashes. Early source-check and malformed-relative-HOME
assertion logs/fixtures are preserved, but the pre-correction compiler binary was
rebuilt before its hash was recorded; **that original binary hash is unavailable**.
This evidence limitation is explicit; no reconstructed hash is presented as original.

The original approved research documents and protected root README match their
initial hashes; HEAD is unchanged. The preexisting docs index work and separate
stdlib backlog are preserved. No staging, commits, branches, pushes, hosted CI or
root README edit occurred. [Preservation receipt](evidence/preservation-check.json).

Review source, native ABI/GC behavior, platform-specific code and the honest native
verification gaps before Boss acceptance. Native Windows/macOS results must be
obtained on those systems; Linux success and source inspection cannot replace them.
