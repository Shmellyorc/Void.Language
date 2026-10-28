# OS module: one complete engineering sprint

27 October 2026. Baseline **`0d014e851851203e30d3b376ca99606a67d36a7a`**.
**Research roadmap only.** Golden Wrench reviews architecture; Boss approves the
[contract](DESIGN.md) before Keyboard implements it. [Evidence/source audit](RESEARCH.md).

**Bounded final amendment:** the Windows placement/resource policies below are
already Boss-approved. The initial eleven queries remain; state_dir and
process_id extend the complete proposed delivery to **13 functions**. This is
the same sprint, with cwd reused and adjacent responsibilities explicitly owned.
After final Wrench review and Boss approval of this amended complete scope, the
steps below form one substantial implementation mission. No engineering is
authorized before that gate, and no further research stage is proposed.

## Complete initial scope

Deliver all thirteen proposed public queries, the error type, platform adapters,
compiler/IR/Runtime integration, docs, examples and focused tests together.
Paths-only data/config/cache/state/home/temp; process executable and PID;
explicit resource layout and macOS bundle detection; individual readonly environment lookup;
platform/process architecture. No prototype accepted with one platform silently
guessing or one execution mode missing. Unsupported packaging/security models
are documented limits, not implied native passes.

Do not add FS operations, process execution, cwd mutation, environment mutation,
directory creation, a global singleton, arbitrary FFI, a new async provider or
an editor semantic engine. Root README and Git/hosted CI remain Boss-controlled.

## Ordered implementation

1. **Accept the amended complete contract.** Preserve Boss's settled Windows
   data/local, config/roaming, cache/local and bundle/executable-parent resource
   policies. Wrench reviews the complete inventory/ownership matrix, including
   state and PID; Boss approves engineering scope. Do not reopen settled choices
   or rewrite original research evidence. Confirm Windows
   scope is desktop Win32; packaged/AppContainer-specific stores are not silently
   promised. Keep GetTempPathW's documented discovery contract; no new version
   floor or dynamic optional-API subsystem is needed.
2. **Define one small native kernel contract.** Use an owned request/result and
   closed operation enum/table for native roots, env, image, bundle and identity.
   A reply needs portable status, original native code/domain, string output and
   a boolean output and a distinct i64 scalar payload. No file handles, arrays
   of directory entries, task layouts or resource-control operations. Put native-specific logic behind a small
   internal interface; avoid a general OS abstraction framework. PID must not
   reuse os_code or allocate managed strings to carry its integer result.
3. **Implement Linux native queries and focused kernel fixtures.** HOME/XDG/temp
   policy including XDG_STATE_HOME/.local/state, passwd ERANGE/no-record/error
   handling, bounded growing readlink, strict bytes, no proc fallback. Query only
   the root needed by the caller.
   Use child-process environment fixtures; never mutate the test runner's global
   environment concurrently. Missing/empty are separate native states. Use
   getpid for the process scalar; retain owner-side operation validation.
4. **Implement Windows wide queries.** Known Folders, scoped COM ownership,
   GetTempPathW, GetEnvironmentVariableW and GetModuleFileNameW. Preserve unsigned
   HRESULT bits/domain, empty versus missing, size changes and strict conversion.
   Use native allocation guards. No MAX_PATH truncation, alias canonicalization or
   changes to compiler staging. Existing STA/MTA initialization must survive.
   State reuses LocalAppData and appends id/state. GetCurrentProcessId widens
   unsigned DWORD directly to i64, including its high bit, with no fabricated
   last-error path or additional library dependency.
5. **Implement the small Apple adapter.** Enable OBJCXX only on Apple and keep
   Foundation objects private to one `.mm` adapter/autorelease scope. Native
   results cross as owned C++ data. User-domain discovery uses create:NO;
   bundle classification/resourceURL and `_NSGetExecutablePath` follow DESIGN.
   No AppKit, GUI runloop, hand-built sandbox roots or Objective-C public ABI.
   Contain Objective-C exceptions before the noexcept ABI: allocation/invariant
   failures use RuntimeFailure, while reported NSError lookup failures remain
   recoverable results. No native exception may escape into generated code.
   State reuses Application Support/id/state and create:NO. PID uses getpid;
   neither addition introduces a new framework or persistent native resource.
6. **Wire both native link paths.** Add new Runtime objects/dependencies in
   CMakeLists.txt; keep C++20/OBJCXX ABI, pinned SDK/arch/deployment/RTTI/sanitizer
   policy coherent. Link documented Windows shell/COM import libraries and
   Foundation for CMake consumers **and** generated/installed native link flags
   in cmake/NativeBuild.cmake. Validate actual import-library selection on Windows.
   Reuse existing Runtime archive cache digest and native-build publication.
7. **Add the guarded compiler bridge once.** Canonical `/@stdlib/os.void` private
   selection; one shared operation contract for signatures and validation; one
   narrow OS query IR operation rather than a public FFI or many unrelated
   primitives. Register its C ABI in JIT and linked Runtime, coordinate Runtime
   execution, validate result geometry with LLVM DataLayout, and attach exact
   source spans. Reject forged op/module/result/arity shapes independently.
   Add owner-only transitive effect rejection to frontend capabilities and IR
   worker-graph checks. Do not accidentally treat lookup as a pure constant.
   Do not mark the host-query ABI readnone/memory(none) or speculatable; these
   calls observe execution-time host state, even though they expose no setter.
8. **Implement rooted native-to-managed publication.** Copy managed inputs into
   owned native bytes while managed, enter NativeSafe only around native work,
   reenter before managed allocations. Root every string/aggregate reference
   through domain/text/error construction and publish a complete result. Never
   retain native env pointers. Translate only known OS conditions into portable
   errors; bad_alloc/extent/ABI failures retain RuntimeFailure. No resource ledger
   or scheduler admission is required because queries own no persistent resource.
9. **Write ordinary source wrappers.** Add stdlib/os.void with the thirteen public
   functions, ID validation and result/error construction. Use byte_length and
   to_utf8 for bounded ASCII ID checks; no unsupported string ordering, nullable
   strings or overload syntax. Reuse `path.join`/`parent` for composition and
   explicitly translate their io.error into os.error in one private helper:
   `?` cannot silently turn unrelated io.error into os.error. Pass native failures
   through the public operation name. No duplicate path algorithm. is_bundle
   returns result<bool,error> so classification failure cannot masquerade as a
   plain executable.
   Include the ordinary scalar PID wrapper through the same canonical bridge;
   do not format/parse its number or broaden worker effects.
   Include state ID/placement/role docs. Do not add os.cwd: document the complete
   existing path.absolute("", "") query and preserve its io.error/snapshot
   behavior. Do not add a native cwd operation or portable runtime_dir alias.
10. **Finish documentation/tooling/install.** Plain `##`/`@param`/`@return` on
    every public symbol, with JIT-host and paths-only caveats visible in hover.
    Add startup/save/resource examples and public API inventory. Existing stdlib
    install recursion and compiler-owned import completion should discover os;
    confirm compiler and tooling install components and relocated distributions.
    Do not hardcode APIs or host locations in VS Code, Neovim or Vim.
11. **Run focused acceptance and review.** Linux Debug, Release, strict sanitizers
    first; fresh-process environment vectors, actual JIT/O2, independent linked
    AOT, installed build, GC/failure, cache/reload, LSP and hard 1 MiB tests below.
    Obtain actual Windows/macOS evidence for their adapters. Keep native limits
    explicit if those machines are unavailable. No routine full seven-job hosted
    run, permanent instrumentation framework or speculative optimization. Stop
    after the completeness/evidence handoff for Wrench source review.

## Focused acceptance matrix

| Area | Required evidence |
|---|---|
| Public completeness | All thirteen signatures, error fields, approved policies and source docs appear in an API inventory. Verify DESIGN's included/existing/future/platform-specific ownership matrix; excluded APIs must not appear as stubbed os features. No private query/reply leaks into completion. Source examples check without new syntax. |
| App IDs | Valid simple/reverse-DNS tokens; 1/255 bytes; zero/256; uppercase, Unicode, whitespace, NUL, slash/backslash/colon, dot traversal, doubled dots, trailing punctuation and Windows reserved stems including `con.game`. Identical validation on all three adapters before discovery. |
| Linux roots | Each of the four XDG home overrides independently, including STATE_HOME unset/empty/relative and `.local/state` fallback; valid override with missing/broken HOME, defaults, absent/empty HOME passwd fallback, relative HOME/TMPDIR failures, absent/empty TMPDIR `/tmp`, absolute invalid bytes, missing user record, ERANGE and native error. Launcher-style Flatpak/Snap/AppImage environment fixtures preserve the supplied roots. |
| Windows roots | Redirected/offline Profile/Local/Roaming; native failures and allocation cleanup; COM initially absent, STA, MTA and preexisting host initialization. UTF-16 non-BMP names, malformed surrogates, long paths, extended drive/UNC namespaces and `%`/`#`/space paths round-trip through path/io. No truncation or profile reconstruction. |
| macOS roots | Foundation user domain, nonexistent roots, create:NO, container home/temp, distinct config/state children and regenerable cache mapping; nil/error outputs and original NSError domain/code. Do not touch Library/Preferences. No standardizingPath/realpath or synthesized container spelling. |
| State roles/placement | Windows LocalAppData/id/state; macOS Application Support/id/state; Linux selected state root/id. All share the existing ID/Unicode/error rules and create nothing. State is not aliased to cache/config/temp, cleared on query/exit, or treated as login-session storage. Logs use this path only by explicit application/log policy; no rotation or retention service is added. |
| Cwd reuse | `path.absolute("", "")` and the omitted-default spelling return one existing cwd snapshot with io.error preserved. Compare against an independent native query, not PWD or project root; include Unicode/long cwd, supported deleted/inaccessible-cwd failures and explicit-base resolution after cwd changes. Use separate child processes for cwd mutation fixtures. No os wrapper/native query or process mutation API is added. |
| Process ID | Native getpid/GetCurrentProcessId comparison, repeated calls in one process, multiple Runtimes sharing that process, simultaneously live distinct child processes, and JIT/O2 host vs independently linked AOT. Synthetic DWORD values above INT32_MAX and through UINT32_MAX widen losslessly. Same cached code run in fresh children observes each executing PID; no compiler PID, string payload or fake native error. The PID kernel needs no managed-string allocation. |
| Runtime/session exclusion | No public portable runtime_dir/session_dir, temp/cache/state fallback, automatic path creation or warning. `os.env("XDG_RUNTIME_DIR")` remains ordinary missing/empty/invalid-encoding-aware env access, with no owner/mode/lifetime certification. |
| Env | Absent differs from empty, exact Unicode/newline/equals values, invalid names, POSIX case-sensitive vs Windows case-insensitive, invalid native encoding and oversized/growing values. Stale Win32 last error, sizing changes, eight-attempt exhaustion. No value in diagnostics. Native concurrent environment mutation is outside the contract. |
| Image/resources | JIT/O2 host image vs independent AOT app image; unrelated cwd, launch symlinks, long Unicode paths, missing procfs/readlink truncation, deleted executable and a real filename ending ` (deleted)`. macOS relative native result yields unavailable. No argv/PATH/source/cwd fallback. |
| Bundle packaging | Plain macOS process false, real APPL `.app` true, mainBundle non-nil without standard app false; moved bundle, alternate cwd, renamed/nonstandard bundle per predicate, nullable/non-file resourceURL, actual Contents/Resources. Windows/Linux false. Bundle errors must not turn into a successful path to Contents/MacOS. |
| No FS mutation | Configure nonexistent roots, call getters, then independently inspect the test-owned tree: no children created, no access probing requirement. A returned unusable location can succeed; subsequent io.create_directories/read/write must return its own error. Temp query reserves no object. |
| Paths/security | No case/Unicode rewriting, shell expansion, link resolution or cwd changes. Absolute roots with `..` through symlinks retain meaning; malformed/native relative roots fail per contract. ID validation blocks child replacement, but does not claim ACL, containment or symlink safety for user-supplied asset names. |
| ABI/IR | Closed operation enum, canonical module only, wrong arity/types/flag/text/i64 geometry and forged worker-reachable queries reject; missing Runtime coordination rejects. Independently emitted native object calls the same ABI as JIT. PID uses a scalar payload, never native error code or managed text. No editor-time host query. |
| GC/faults | Force collection during native query and between output allocations; inputs copied before NativeSafe, outputs rooted, no UAF/leaked native memory, balanced COM/autorelease cleanup. Fault native and managed allocations through existing test seams/budgets; retain primary failure without half-success. No large automatic buffers. |
| Owner/async/globals | Sync entry and owner async calls work; direct/transitive worker calls reject. Global query initializes once, failure respects ordinary startup handling, compatible reload retains stored string; fresh calls can observe changed host state only in quiescent host fixtures. Runtime close/repeated invocations leave no OS-owned resources behind. |
| Cache | Actual object-cache miss/hit and --no-cache paths produce runtime values, not compile-host snapshots. Same cached code in separate child processes observes different configured roots/env. Changed Runtime/bridge code follows existing build identity. Globals do not serialize. Preserve Windows cache-unavailable policy. |
| Tooling | use completion, os member/error completion, alias calls, hover, contextual parameter/return documentation, definition/references, guarded installed-stdlib rename; incomplete calls/unsaved overlays/UTF-16 and stale results. Same server for all editors; no UI-specific discovery logic. |
| Install/AOT | Compiler and tooling install components include source; relocate install then `voidc build` and execute result. Exercise CMake-linked native host and directly linked AOT with Foundation/shell/COM dependencies; output/staging/atomic replacement/security regressions remain passing. |
| Stack/profile | Actual hard 1 MiB child stacks for source analysis and normal/O2 JIT plus linked AOT; long heap-buffer outputs and many bounded queries. Linux focused Debug/Release/ASan/UBSan/leak runs. Preserve native stack policies; do not enlarge stacks to pass. |

Use existing tests/io, tests/random, tests/stage11 and LSP Client helpers rather
than a new verification framework. A small kernel result seam can supply native
failures/long outputs that are unsafe to create on a real host; it must not
replace actual native API calls in platform acceptance. Keep fault injection
private/test-owned, with no global mutable production hook.

## Native verification and scope of claims

- **Linux:** deterministic child environment and kernel failure fixtures plus
  real current-user home/temp/image queries. Real Flatpak/Snap/AppImage tests are
  required before claiming those packaged launchers specifically verified;
  environment fixtures establish convention handling only.
- **Windows:** actual desktop Debug/Release executions, not a cross-compile.
  Include Unicode/long/UNC conversion, empty env, apartment preservation,
  redirected Known Folders and installed native linkage. Use controlled native
  error fixtures if a corporate/offline profile cannot be provisioned, and state
  that coverage limit. AppContainer/UWP/SYSTEM service behavior is not certified.
- **macOS:** actual arm64 run, real `.app`, unrelated Finder/shell launch cwd,
  Foundation linkage through both build paths. A signed App Sandbox fixture is
  required to claim container behavior verified. Test Rosetta only if x86_64
  execution support is claimed; architecture means process target in both cases.

These are focused native checks, not an instruction to dispatch hosted CI.
Boss controls access and any hosted run. Linux success is insufficient to mark
the cross-platform module fully verified; record pending evidence honestly.

## Measurements, crash evidence and stopping point

Record representative first/repeated startup-query latency and allocation
behavior using existing bounded timing/memory helpers, with no thresholds
invented from an absent implementation. Distinguish cold OS/NSS/native service
cost from bridge/string allocation. If measurements show a problem, first remove
duplicate queries/checks; applications can explicitly retain startup snapshots.
Do not add global caching, env watchers, background tasks or optimization before
evidence warrants them.

All compiler/runner child executions use the existing preservation wrapper in
tests/text_encoding/crash.py (or the current shared import used by io/random).
Every abnormal exit retains exact `_crash.void` bytes, imported source set,
generator/provenance hashes, command, profile and stderr before temp cleanup.
A passing retry does not erase or explain a crash. Stop before ptrace unless
Boss has authorized that investigation; do not mix historical incident repair
into this module sprint.

Final Keyboard handoff must list changed files, the complete public API/behavior
matrix, exact local results by mode/profile, native-platform evidence/limits,
original failures and unresolved issues. No module-complete claim while an
approved API, failure path or required platform implementation is missing.
No commits, branches, pushes, CI dispatches or root README changes are part of
this roadmap. **Stop for Golden Wrench independent review and Boss acceptance.**
