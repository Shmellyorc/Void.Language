# Logging local acceptance

**Pre-repair acceptance record.** Counts in this original report do not validate the subsequent performance changes. Fresh expanded profile results are being recorded under [evidence/performance-repair](evidence/performance-repair/). Release remains on hold for independent review.

28 October 2026. Baseline `f9ac49f1558f326129948bc06c829263448b7714`.
The preserved [DESIGN](DESIGN.md) defines behavior and [ROADMAP](ROADMAP.md)
defines acceptance. Boss's implementation authorization supersedes their earlier
research-only labels. Independent review and native Windows/macOS acceptance are
not claimed.

**Final local result: 93/93 selected CTests passed**—33 logging outcomes and
60 adjacent regression outcomes. The source stayed at the recorded
[validation hashes](evidence/validation-source-freeze.json) throughout these runs.

| Profile | Passed | CTest wall time | Receipt |
| --- | ---: | ---: | --- |
| Debug | 31/31 | 1469.14 s | [summary](evidence/acceptance-debug-verified.log) |
| Release | 31/31 | 526.61 s | [summary](evidence/acceptance-release-verified.log) |
| Strict ASan/UBSan/leaks | 31/31 | 1500.09 s | [summary](evidence/acceptance-strict-verified.log) |

## Reproducible selection

From the repository root:

```sh
python3 tests/log/profiles.py build/ci-official-debug
python3 tests/log/profiles.py build/ci-official-release
python3 tests/log/profiles.py build/ci-official-sanitizers --strict
python3 tests/log/examples.py build/ci-official-release/voidc
python3 tests/log/benchmarks.py build/ci-official-release/void_log_host build/ci-official-release/void_log_probe_normal build/ci-official-release/void_log_probe_optimized
python3 tests/log/benchmarks_default.py build/ci-official-release
python3 tests/log/evidence.py
```

The existing pinned LLVM/Clang 19.1.7 toolchain and configured profiles are used.
The selected 31 CTests comprise 11 logging tests and 20 adjacent tests:
advanced source, Runtime globals, console source, I/O ownership, seven native I/O
lifecycle cases, native jobs, filesystem kernel, three async I/O source cases,
and async source/IR/parallel/lifecycle. This is focused local acceptance, not a
claim that every repository test or hosted platform ran.

Strict environment:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
LSAN_OPTIONS=exitcode=87
```

C++ compiler, Runtime and host code use the configured ASan/UBSan instrumentation.
JIT/emitted VOID instructions are not themselves passed through Clang's sanitizer
instrumentation. Native AOT objects are emitted, linked and launched separately;
the native logging probes link Runtime libraries without LLVM/JIT libraries.
Async entry uses the established explicit root runner/adapter.

Logging execution hosts, emitted applications and selected compiler/LSP processes
run with both POSIX stack limits set to **1,048,576 bytes**. Native hosts also read
back and assert both kernel limits. This establishes execution within that bound;
it is not a measured instantaneous stack high-water mark. Build/link drivers and
the unrelated adjacent tests do not all inherit that logging-specific limit.

## Counts and evidence units

Each profile runs the same logging selection below. Counts are the harness's
stated checks or commands, not interchangeable units to sum into a fabricated
number of independent scenarios. Detailed stdout and timings live in
`evidence/ctest-{debug,release,strict}-verified.log` and the
[acceptance index](evidence/acceptance-index.json).

| CTest | Coverage/count per profile |
| --- | --- |
| log_native | 1,686 native host checks across normal/O2 JIT |
| log_probe_normal | 826 independently linked native AOT checks |
| log_probe_optimized | 826 independently linked optimized native AOT checks |
| log_execution | 32 check/execution/emission/link observations across four fixtures and both modes |
| log_frontend | 16 visibility, readonly, worker, default-construction and surface checks |
| log_files | 244 native source/filesystem-oracle checks across JIT and AOT modes |
| log_oracles | 15 execution/emission/link checks; 30 severity/threshold pairs, specified Unicode controls, eight byte boundaries, nine invalid configurations and eight fixed UTC instants |
| log_console | 16 real redirected-console/live-UTC/closed-peer/file-independence observations |
| log_cache | Six actual miss/hit/bypass launches, each with fresh Runtime defaults |
| log_install | 15 installed/relocated check/JIT/O2/build/independent-AOT/LSP commands |
| lsp_log | 38 compiler-owned protocol checks |

The seven shipped examples additionally pass **25** source/JIT/O2/independent
async-AOT commands in isolated directories, including an isolated XDG state path.
[Compiler-owned API inventory](evidence/public-api.tsv): **83 declarations**
(seven types, 22 enum cases, 21 own fields and 33 public callables), with all
written parameters documented. The final file/source/tool inventories are under
[evidence](evidence/), with exact command receipts and immutable failures.

## Contract-to-test mapping

| Required area | Implementation and independent observation |
| --- | --- |
| Complete surface/defaults | `log.void`, compiler inventory, `defaults.void`, `semantics.void`, frontend and installed checks; private constructor/helpers remain hidden. |
| Filtering/eager/lazy | All 5×6 pairs; eager custom-display effects, guard, named/captured builders and callback factory counts; recursive same-instance busy and independent-instance success. |
| Bytes/time/budgets | Independent Python UTF-8 oracle for all specified escapes and names; exact LF/no BOM; fixed years 0001–9999, leap/day/year boundaries and truncated fractions; poisoned sampling/formatting overlays; exact 65,536-byte output on hard stacks. |
| Console/two sinks | Actual redirected stdout/stderr and closed OS pipe; native short-transfer adapter retains byte/UTF-16/native diagnostics; file still attempted after recoverable console error, converse file errors retain console success. Borrowed endpoints remain open. |
| Fresh/adopted files | Full exact-set preflight; reject active/archive/hardlink alias by default; known symlink/directory/FIFO failures; adopted regular complete/empty files, gaps, missing active, torn/oversized rejection. Unicode/% paths and changed cwd preserve the acquired absolute snapshot. |
| Rotation/authority | Exact 0/1/3/32 retention after repeated writes, independent byte age oracle, native inode/content observations before/after each mutation, successful rename identity retention and untouched external sentinels. Exact-cap/oversized records do not rotate prematurely. |
| Mutation failures | Actual permission-denied delete, directory obstruction, each no-replace rename collision, exclusive-new-active collision, flush plus close failure; prior mutations remain, no replacement or rollback. |
| Configuration/recovery | Copied settings, mutable aliases/readonly views, retained same-namespace authority, grow/shrink/reduced-bound checks, preserved old history on path change, failed replacement and explicit recovery/adoption, closed-default replacement and concurrent replacement busy. |
| Partial/unwind faults | One-byte native chunks; zero progress; all 18 failing prefixes of a record including UTF-8/escape boundaries; faulted continuation performs no transfer. A real Runtime allocation failure after one transferred byte leaves exactly that byte and rejects replay. |
| Checked lifecycle | Flush and release causes both retained; native close error retires once; repeated close; busy deinit establishes terminal admission while allowing later explicit close retry; acquisition length failure plus cleanup failure does not overwrite the original cause. |
| Async/GC/cancellation | Before-admission cancellation; entered open/read/write/move/close barriers and completion races; forced collection while suspended; balanced roots/commits; exactly one native close. Unpublished default replacement is separately cancelled, remains unpublished and allows later explicit recovery. |
| Allocation/publication | 71 injected acquisition failures before success per mode; write/close budgets; guards release and descriptors return to baseline; builder unwind before transport remains usable. Ordinary Runtime failures are not fabricated as checked log failures. |
| Globals/startup/forgotten ownership | Import aliases share a default; separate Runtime launches remain independent; failed global startup drains provider ownership; forgotten logger survives GC without source deinit and closes at Runtime shutdown without invented fsync. |
| Cache/reload | Real code-cache miss/hit/bypass; compatible code replacement while native write is pending preserves file identity and old/new record order; live generation count retires; initializer-layout incompatibility requests restart. |
| Tooling/installation | Compiler-owned imports/members/enums, signatures/parameter docs, hover, definitions/references, private hiding, protected stdlib rename and unsaved recovery; installed/relocated Unicode/#/% tree, build route and independent linking. |
| Shared repairs | Enum/callable defaults retain missing-default errors; a seven-scenario existing native-job suite includes the deterministic nested cancellation-drain regression; adjacent async tests preserve cycle rejection and structured cleanup. |

The three additional Golden Wrench safeguards are exercised directly: native
acquisition/retirement ownership under failure and cancellation, exact namespace
identity/content observations across failed rotation, and conservative fault
states after partial transfer/Runtime unwind. Metadata checks still do not certify
hostile-namespace, hardlink, alias or competing-writer safety; those remain the
approved caller warranty.

## Preserved failures and pending platforms

[INCIDENTS](INCIDENTS.md) distinguishes source diagnostics, test corrections,
the real acquisition ownership gap, the reproduced shared scheduler defect and
deliberate stops of superseded validation. Original receipts were not replaced by
passing retries. Historical H1–H7 remain separately unresolved.

Native Windows/macOS execution remains pending: Unicode/percent/long paths,
Windows close-before-rename/sharing/case behavior, macOS native move/storage
behavior, and equivalent native resource/identity observations still require
those hosts. No hosted CI, Git staging/commits/branches/pushes, ptrace or protected
root README edit occurred. This document records local engineering acceptance;
Golden Wrench's independent source review is the next gate.

## Mandatory performance repair: fresh final validation

The preceding 93-test record is historical. The repaired source passed **263/263
selected CTests**: 88 Release, 88 Debug and 87 strict ASan/UBSan/leak checks, all
with matching source-file hashes and actual hard 1 MiB stacks. The expanded suite
adds checked scalar reads, buffer aliases and collection, persistent-root removal,
text/encoding, async AOT/lifecycle and long-root retention coverage. Existing
logging GC/OOM, cancellation, ownership, rotation, partial-write, cache/reload,
tooling and installed execution coverage remains. Nested test executions are not
counted as additional CTests. CMake already omits the duplicate
`text_encoding_stack` registration under sanitizers; the strict text execution
still inherits the required hard 1 MiB stack.

See the [final handoff](HANDOFF.md) for exact profile durations, counts and
limitations, and the [fresh acceptance index](evidence/performance-repair/acceptance-index.json)
for all outcomes and executable identities. Sanitizers instrument the compiler,
Runtime and C++ hosts; generated VOID instructions do not independently receive
Clang sanitizer passes. Native Windows/macOS remains pending.

**Performance gate OPEN; release HOLD.** Both investigations and main repairs are
complete, but synchronous logging remains above the single-digit/2–3× direct-I/O
screen. All original failures and benchmarks remain preserved. Independent Golden
Wrench review must assess the implementation and remaining measured costs.
