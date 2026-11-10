# Test output retention repair

The test infrastructure was qualified locally from `1132e0c9116d35a15034edc93ecc5ae3aff785fb` with the existing LLVM, Clang and LLD **23.1.4** toolchain. Compiler and Runtime sources, standard library fixtures, vendor inputs, licenses and existing assertions were preserved.

## Output ownership

- The shared Python harnesses for channel, collections, logging, JSON, Math, Random, OS and host capabilities write receipts to per-invocation temporary directories. Text/encoding, generics, HTTP and URL crash diagnostics use the same default. Normal exits and assertion failures clean the directory. CTest keeps its own log and prints a bounded excerpt from captured diagnostics on failure.
- CTest sends generated HTTP and VOIDUP receipts, project and numeric reports, retention measurements and native test sandboxes to private directories. Frontend and relative-path IO fixtures also get a private working directory. CTest suppresses Python bytecode, and the IO and logging fixture generators use `python -B` during builds.
- Checked-in inputs used by ordinary JIT tests run with `--no-cache`. Dedicated cache tests still exercise the real cache on private fixtures. Build-generated objects, link and install manifests, and prelinked regression programs remain available in the configured build. Historical reporting and dependency provisioning are separate, explicitly invoked workflows.
- Set `VOID_TEST_DIAGNOSTICS_DIR` to a private directory outside the checkout for a diagnostic rerun. This keeps the selected invocation's complete shared-harness output. For routine runs, use the system temp location or a private `TMPDIR` and let the test remove its own files. [Python temporary directory behavior](https://docs.python.org/3/library/tempfile.html#tempfile.TemporaryDirectory) · [CTest test environment](https://cmake.org/cmake/help/latest/prop_test/ENVIRONMENT.html)

## Local qualification

The build used Python 3.14.8 and CMake 4.3.4 on Linux, and Python 3.14.8 and CMake 4.4.3 on Windows 10 build 19045. Counts are distinct selected tests after the recorded reruns.

| Profile | Registered inventory | Selected passing |
| --- | ---: | ---: |
| Linux Release | 443 | **278/278** |
| Linux Debug | 456 | **279/279** |
| Linux fatal ASan/UBSan with leak detection | 433 | **272/273** |
| Windows 10 Release | 381 | **61/63** |
| Windows 10 Debug | 381 | **61/63** |

The selected suites cover the changed harnesses, the private IO recipes, installed and relocated execution, project tooling, HTTP/URL, cache behavior, G1–G4 and retained G5 callbacks. The Linux registration audit checked test names, arguments and properties against the accepted inventory and accounted for output paths, private working directories and bytecode settings. Windows kept the same configured names and timeouts. The supplemental Windows IO run applied the private-working-directory launcher to the existing registered commands.

Two independent CTest runs passed the same project and native IO tests using a shared temp parent containing spaces and Unicode. The launcher probe ran eight simultaneous same-name invocations on Linux and Windows and checked default failure cleanup, a visible diagnostic excerpt and explicit retention. The IO generator checks produced byte-identical fixtures with no source bytecode.

### Initial failures and controls

- Linux Release and Debug initially rejected the qualification runner's group-writable temp parent in `voidup_smoke`. Both passed with mode `0700`. Windows Release `tooling_demo` passed after building its indirect executable prerequisite. The separate Windows installed-memory checks passed after setting the scratch ACL to the test user and removing the explicit `OWNER RIGHTS` grant.
- Windows Debug `http_reload` reached its 180-second subprocess limit twice. An original-recipe control passed in 163.30 seconds. The isolated wrapped replay passed in 159.96 seconds using the same rebuilt binary.
- Windows `io_wire_normal` and `io_wire_optimized` returned `91` in both profiles with both working-directory recipes. The guest's direct symlink probe returned WinError `1314`. The other 16 IO cases and the Schannel store case passed in both profiles.
- The first sanitizer `http_tls_memory` run reported curl code `28` during a verified handshake. The retry and an unwrapped original-recipe control reached the same 1,200-second subprocess limit. The registered 1,300-second CTest limit was preserved. Release and Debug passed this fixture.

## Reproduce

Configure and build with the pinned profile inputs in the [Slice 7 reproduction instructions](SLICE7_FINAL_CLOSEOUT.md#reproduce-from-source). Run CTest from the configured build. Use `RelWithDebInfo` with `VOID_SANITIZERS=ON` for the Linux fatal profile.

```sh
ctest --test-dir "$profile_build" -j2 --output-on-failure
ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir "$strict_build" -j2 --output-on-failure
```

For a focused replay, use `-R '^(io_native_close_failure|project_creation_process|text_encoding_crash_preservation|json_execution_.*|ffi_g5_callbacks)$'`. See the [test development policy](VOID_ARCHIVE_AND_RETENTION_POLICY.md#test-development) for direct Python runs and opt-in diagnostics.

## File accumulation and cleanup

The closing scan found **zero new persistent generated files or directories** in the Linux source tree, including ignored output paths. The scan covered 105,796 files and 22,238 directories outside Git metadata, build and toolchain trees. All 594 maintained VOID fixtures matched their opening SHA-256 values. Earlier runs exposed 18 task-created JIT cache entries and three Windows bytecode files from build-time imports. The identified Linux entries and the disposable Windows source copies were removed. The corrected runs left zero files in the per-profile test temp roots.

At two-second sampling, Linux test output peaked at approximately **241 MiB** in Release, **474 MiB** in Debug and **850 MiB** in the sanitizer profile. Peak combined use was **1.02 GiB**. These figures cover per-test temp storage. The disposable Linux build and qualification tree separately measured **45.71 GiB** by `du`, including reflinked inputs.

Git recorded `main` and local `origin/main` at `1132e0c9116d35a15034edc93ecc5ae3aff785fb`, an empty staged diff and a passing `git diff --check`. The 2,753 preserved untracked paths remain alongside the new launcher and this report. The three external ZIPs matched the retained SHA-256 receipts, and the S1–S7 backup and pinned environments are present.

Verified removal of the three host-side scratch roots and the Windows guest scratch, including the retry executable after its file lock cleared. The checkout filesystem had 327.58 GiB available at the start, 294.32 GiB immediately before cleanup and 339.98 GiB afterward. The measured increase at cleanup was **45.65 GiB**. Free space on `/tmp` moved from 15.07 GiB to 15.07 GiB.

## Final qualification follow-up

### Linux sanitizer TLS corpus

The delay was in **AWS-LC trust-store insertion during the TLS connection phase**. The fixture originally varied serial numbers while giving all 983 certificates the same subject. With a fresh CA blob on each connection, the pinned AWS-LC store scans same-subject entries to detect duplicate certificates. A scratch-only instrumented replay timed the provider calls inside the real loopback handshake. Across nine full-size store loads per variant, mean PEM parsing was 0.336 seconds for the old corpus and 0.332 seconds for the corrected corpus. Mean insertion fell from **2.527 to 1.603 seconds per store**. The one-round probe also completed the negative-peer checks. libcurl's connection timeout includes the TLS handshake, which accounts for the earlier curl code 28. [Pinned AWS-LC store implementation](https://github.com/aws/aws-lc/blob/cd481c207e38f06aeb5766f4a1cdfce10a4c0af8/crypto/x509/x509_lu.c#L473) · [libcurl connection deadline](https://curl.se/libcurl/c/CURLOPT_CONNECTTIMEOUT_MS.html)

In `tests/net/http/tls_memory.cpp`, the synthetic certificates now get distinct, equally sized subject and issuer names before signing. A controlled pair generated from the same key and original certificate contained **983 signed certificates and 1,047,902 PEM bytes** in each case. The original corpus had one distinct subject and the new corpus had 983. The real trust anchor, 256 verified handshakes, negative peers, memory thresholds, fatal ASan/UBSan and leak detection all remain part of the full test. The connect, subprocess and CTest limits are 30, 1,200 and 1,300 seconds respectively. Historical strict passes at 757.63 and 1,052.33 seconds, followed by the earlier local timeouts, document the old fixture's timing variability. [HTTP qualification history](../net/http/CI_HANDOFF.md#coverage-and-diagnostics)

### Windows symlink capability

Both the original working-directory recipes and the disposable launcher still report **two failures per profile** on the standard Windows 10 token. Each wire program exited normally and printed `91`, the fixture's symlink-creation failure sentinel. Direct file-symlink probes in the same private temp parent returned WinError `1314` with flags `0` and `SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE`. Microsoft documents that the latter needs Developer Mode and that `1314` means the token lacks a required privilege. The wire fixture requires creating a real file symlink, reading its target, verifying its type and removing it. A junction or hard link cannot establish that contract. The project already documents this prerequisite. The tests retain their assertions and report genuine failures on this guest. [Win32 symbolic link API](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createsymboliclinkw) · [Win32 error 1314](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--1300-1699-) · [VOID IO contract](../io/API.md)

### Final local results

| Profile | Selected tests passed | Focused result |
| --- | ---: | --- |
| Linux Release | **14/14** | Corrected `http_tls_memory` passed in 32.23 s |
| Linux Debug | **14/14** | Corrected `http_tls_memory` passed in 247.70 s |
| Linux fatal ASan/UBSan | **14/14** | Corrected `http_tls_memory` passed in 551.40 s |
| Windows 10 Release | **6/8** | Both wire cases failed in both launch modes with the same privilege limitation |
| Windows 10 Debug | **6/8** | Both wire cases failed in both launch modes with the same privilege limitation |

The original sanitizer binary passed on this replay with both recipes. The native child took **797.31 s unwrapped** and **802.24 s through the launcher**. The historical timeouts remain in the record above. The selected Linux runs also covered HTTP framing, DNS, timeouts, URLs, IO wire and probe modes, crash preservation and Runtime callbacks. For the sanitizer IO close control, a retained binary failed to resolve current stdlib declarations and a partial relink exposed a UBSan alignment error with mixed compiler and Runtime inputs. A coherent scratch build from current sources passed the original registered case. The Windows replays used retained Release and Debug binaries whose normalized generated wire source matched the current fixture. Six adjacent IO cases passed in each profile.

### Reproduce and verify cleanup

Build `void_http_tls_memory` in the pinned Linux profile and run `ctest --test-dir "$profile_build" -R '^http_tls_memory$' --output-on-failure` with the fatal sanitizer options above for the strict profile. To compare launch paths, compile the original `tests/net/http/tls_memory.cpp` from the baseline into a private directory using the profile's generated target flags. Run `python -B tests/net/http/tls_memory.py "$scratch/void_http_tls_memory" --evidence "$scratch/direct"`. For the wrapped replay, prefix the Python command with `python -B tools/test_output.py http_tls_memory` and change its evidence argument to `@VOID_TEST_OUTPUT@/http-tls-memory-evidence`. On Windows, run `ctest --test-dir "$windows_build" -R '^io_wire_(normal|optimized)$' --output-on-failure` under the token being qualified.

All **594** maintained VOID fixtures matched their opening SHA-256 hashes. Source scans found zero persistent generated artifacts. Two simultaneous native IO invocations passed from a temp parent containing spaces and Unicode and left it empty. The original Windows recipes wrote 22 entries per profile into their disposable case directories. The wrapped and adjacent Windows runs left zero entries in their test temp roots.

This follow-up modified `tests/net/http/tls_memory.cpp` and this report. Git is on `main` at `1132e0c9116d35a15034edc93ecc5ae3aff785fb`, with 67 tracked modifications including the earlier retention changes. The opening untracked path list still matches. The staged diff is empty and `git diff --check` passes.

The Linux qualification builds and applications, Windows guest workspace and host exchange directory were removed. The external archives and S1–S7 backup were left outside this work.
