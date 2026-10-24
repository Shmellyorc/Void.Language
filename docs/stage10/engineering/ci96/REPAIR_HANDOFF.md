# CI #96 native failure repairs — Step42 remains INCOMPLETE

Source under test: `ef6017613ecf92de31380476ed5d9c3fc8cf3dbb`.
[Original run](https://github.com/Shmellyorc/Void.Language/actions/runs/40042618839).
All seven original files being repaired match that committed Git blob identity;
see committed-source-blobs.json and before/. No commit or hosted rerun was made.

## Original native evidence

Windows Debug job120716958536 and Release job120716958457 built successfully.
Each executed194 suites:190 passed,4 failed. The identical failures are
stage10_operator_reload, format_temporal, stage10_console_aot and
lsp_stage10_source_formatting. macOS ARM64 Debug120716958543 and
Release120716958546 configured but failed compilation at console.hpp stop_token;
no macOS suites executed. Native editor steps after failed builds/tests did not run.
Original decoded job logs are losslessly preserved in job-ID.json (including
CRLF and BOM in the JSON string). Artifacts.json preserves upstream ZIP digests;
ZIP bytes were not independently downloaded/verified here. The initial run/jobs
metadata is a dated snapshot, not a claim about subsequently finishing Linux jobs.

## Demonstrated causes and bounded changes

1. **Windows cache test:** native_cache.cpp intentionally declines persistence.
   The test now requires unavailable for cold/repeated/body/dependency/spec/reload
   cases on Windows, preserving all executable behavior and old-generation leases.
   It also rejects creation of persistent storage. POSIX miss/hit/corruption checks
   remain. No production cache implementation changed. Console cache checks also
   tolerate the genuinely absent directory and still execute hook/spec edits via
   unavailable fallback; corruption is exercised only where real artifacts exist.
2. **Temporal arithmetic:** MSVC STL represents hours/minutes with32-bit int while
   seconds uses64 bits. The old sys_days + hours + minutes chain narrows the large
   intermediate minute duration before conversion to seconds. Promoting sys_days
   to sys_seconds before adding time avoids that loss and retains years1–9999.
   The exact32-bit-representation model of9999-01-01T23:59:59 produced
   seconds=-4327186561 /1832-11-16; the promoted expression gives253370851199 /
   9999-01-01. This is a Linux ABI-width model, not observed native component output:
   original CI logged only temporal check1195. Native rerun must confirm the
   component repair. Existing round-trip checks now include exact expected/actual
   diagnostics, and explicit year1/year9999 epoch-second endpoints were added.
   Standard duration_cast can narrow without a UBSan report; the original model
   exits1 from its mismatch assertion. No unsupported signed-overflow claim is made.
3. **Legacy trailer:** CLI std::cout and emitted legacy printf retain native CRT
   text newlines: CRLF on Windows, LF on POSIX. Source print still writes exact raw
   UTF-8/LF. Only the known legacy entry-result trailer has a platform-specific
   expected byte sequence, separately asserted for JIT and independent AOT; no
   output normalization or production output-mode change. The explicit legacy
   control now requires that one platform-appropriate sequence rather than either.
4. **LSP fixture encoding:** source and repair writes now explicitly use UTF-8;
   related console fixture reads/writes are explicit as well. With a controlled
   cp1252 default, the original fixture reproduces UnicodeEncodeError; the repair
   passes all144 LSP/UTF-16/formatting/revision checks against the accepted Linux
   server. This controlled locale test is not native Windows acceptance.
5. **Pinned libc++19:** the SDK supplies stop_token behind its experimental gate.
   Its header's presence does not imply the types are exposed. An actual Linux
   compile against the pinned libc++19 headers reproduces missing stop_source/token/
   callback; the supported _LIBCPP_ENABLE_EXPERIMENTAL opt-in compiles, links and
   runs the cross-thread/immediate-callback probe, without libc++experimental.a.
   Native Apple configuration now first compile/links the ordinary probe, then,
   only if necessary, compile/links the pinned190107 opt-in and propagates that
   definition through existing void_options. Failure remains fatal. Existing
   std::stop_source/token/callback machinery, buffers and Runtime shutdown remain
   unchanged. No new cancellation library, replacement stdlib or LLVM upgrade.
   This local probe is Linux, not an Apple SDK execution. Native SDK availability
   and deployment-target compatibility still require the actual configure/link/run.

Primary evidence: [MSVC17.14 chrono aliases](https://github.com/microsoft/STL/blob/vs-2022-17.14/stl/inc/__msvc_chrono.hpp),
[libc++ C++20 implementation status](https://libcxx.llvm.org/Status/Cxx20.html),
[pinned SDK stop_token header](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/libcxx/include/stop_token).
SDK header/macro/link probes and original CI commands are retained locally.

## Local verification scope

linux-commands.json and its raw configure/build/CTest/JUnit logs are authoritative.
Fresh step42-ci96-debug / step42-ci96-san directories preserve accepted Step41
binaries and archives. Focused suites retain their existing timeouts and assertions.
Normal/O2 JIT, independent AOT, native production builds, real POSIX PTYs, GC,
queued/entered cancellation and restoration are exercised by the existing tests.
The unchanged original full frontend and semantic-depth probes also run under
soft/hard1048576 bytes. No larger stack or guard/optimization change was made.

The small strict width probe initially hit the known sandbox LSan/ptrace failure;
its exact failed log is retained. The established untraced profile passes with
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 unchanged.
No debugger, security policy, suppression or Daybreak action was used.

## Exact native verification still required — Boss controls execution

Use a fresh approved source snapshot including these repairs. Preserve Step41's
original freeze and results; use ci96/final-source-freeze.json for the new767-input
snapshot. Do not use the original Step42 source-receipt command unchanged: it
correctly rejects repairs relative to its historical Step41 freeze.

From repository root, before and after native build/test:

```text
python -c 'import hashlib,json,pathlib; p=pathlib.Path; m=json.loads(p("docs/stage10/engineering/ci96/final-source-freeze.json").read_text()); bad=[f for f,s in m.items() if not p(f).is_file() or hashlib.sha256(p(f).read_bytes()).hexdigest()!=s]; print(json.dumps({"checked":len(m),"mismatches":bad})); assert not bad'
```

Use the existing Step42 native command inventory for SDK setup and fresh native
Debug/Release builds, preserving pinned19.1.7, MSVC/DIA/CRT and architecture guards.
Native compilation/verification has NOT been dispatched by this increment.

On macOS ARM64, capture the actual clang++ --version, -dumpmachine,
xcrun --show-sdk-path, deployment target and include search. Using that same SDK:

```bash
cxx="$PWD/.toolchains/llvm-19.1.7/bin/clang++"
mkdir -p build/step42-macos-probes
"$cxx" -std=c++20 -dM -E -x c++ -include version /dev/null > build/step42-macos-probes/macros-default.txt
"$cxx" -std=c++20 -D_LIBCPP_ENABLE_EXPERIMENTAL -dM -E -x c++ -include version /dev/null > build/step42-macos-probes/macros-opt-in.txt
"$cxx" -std=c++20 -v -E -x c++ -include stop_token /dev/null > build/step42-macos-probes/headers.txt 2> build/step42-macos-probes/search.txt
"$cxx" -std=c++20 -pthread cmake/probes/StopToken.cpp -o build/step42-macos-probes/standard > build/step42-macos-probes/standard.log 2>&1
# Record this exit: failure is expected only when libc++19 gates the types.
"$cxx" -std=c++20 -pthread -D_LIBCPP_ENABLE_EXPERIMENTAL cmake/probes/StopToken.cpp -o build/step42-macos-probes/enabled
build/step42-macos-probes/enabled
```

Record _LIBCPP_VERSION, __cpp_lib_jthread, experimental/thread macros and actual
header paths. Retain the original failed compile log. The opt-in must link and run;
a compile-only success is insufficient. Native CMake checks both compile AND link;
its logs must show the standard result and, if needed, the pinned opt-in result.
External C++ embedding builds using pinned libc++19 must use the same public
void_options definition. Independently emitted VOID AOT continues to link the
existing Runtime archive; no experimental archive or runtime sidecar was added.

On Windows in the same native MSVC SDK environment, also run this direct
representation/old-expression/new-expression probe and retain its exact output:

```powershell
& .toolchains/llvm-19.1.7/bin/clang++.exe --driver-mode=g++ -std=c++20 -fms-runtime-lib=dll -Ithird_party/date/include docs/stage10/engineering/ci96/probes/temporal-native.cpp -o build/step42-temporal-probe.exe
& build/step42-temporal-probe.exe
```

The local Linux native standard-library control reports8-byte hour/minute reps and
both expressions correct. The forced32-bit width reproduction fails before and
passes after. Only the native Windows command can record the actual host aliases
and old/new result; do not relabel the Linux width model as that observation.

Then, in EACH native Debug/Release build directory, run the four reproduced suites:

```text
ctest --test-dir NATIVE_BUILD --output-on-failure --output-junit native-repair-junit.xml -R "^(stage10_operator_reload|format_temporal|stage10_console_aot|lsp_stage10_source_formatting)$"
```

After focused repair verification, complete unfiltered native suites and the
already prepared stack, packaging, editor and actual console-host gates. Projected
counts remain194 Windows/199 macOS; save actual inventory and outcomes. Require
unavailable/no storage for Windows cache while preserving behavior and reload;
confirm years1–9999 and exact round-trip components; compare JIT/AOT console bytes
independently; retain all Unicode/UTF-16 dirty-editor assertions. Traditional console/
Windows Terminal/ConPTY real wide-I/O/state gates remain additional, not certified
by injected adapters. macOS must exercise actual termios/SIGPIPE/PTY behavior.
The existing seven-job workflow still has no native-only dispatch input.

No native repaired-source pass or native binary identity exists yet. Original
native failures remain failures. Step41 Linux597/597 and its original limits/logs
remain historical acceptance. Preserve Step37, transient Clang and Release LLD
GVN observations, deep compilation costs and deferred objectives. Steps43–44 remain
separate; return this repair handoff to Wrench before Boss-controlled reruns.


## Final focused Linux results

Debug18/18 and strict ASan/UBSan/leaks18/18 pass, zero final failures/skips.
Both original genuine1MiB probes pass. Each configuration retains1311 temporal
core,248 operator/cache/reload and144 source-formatting LSP checks; full console
normal/O2 JIT, linked AOT and production native comparisons pass. The evidence
recorder PosixPath error after the original Debug pass is preserved and repaired;
both standalone probes were rerun with complete exit records. No traced strict
build/test was started. Step41 originals remain unchanged; final hashes and exact
commands are in ../step42-results.json. Repaired native platforms UNVERIFIED.
