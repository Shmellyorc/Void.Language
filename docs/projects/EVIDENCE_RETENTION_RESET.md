# VOID evidence retention reset

**Proposal for Golden Wrench and Boss.** The first tracked cleanup would remove **16,010 generated files** and reduce the unpacked checkout from **1.94 GiB to 628 MiB**, a **68.3%** reduction. The audited commit is `a642ce746a62a7646d06d42d9f87931813aaa812`. Complete the documentation corrections below before approving the exact manifest.

## Tracked files

The external machine-readable manifest `Void.Language-docs-cleanup-manifest.json.gz` lists every proposed path, Git blob, file mode, byte count, group and reason. It also lists each newly broken relative link with its source line and target. The manifest SHA-256 is `fe0ea6c5d74a6edc300b2a924c618abeebeaf082879eb61422d2fd376fba0042`. All 16,010 candidate files matched their recorded blobs on disk.

| Exact manifest group | Files | Unpacked savings |
| --- | ---: | ---: |
| `docs/log/evidence/performance-repair/` generated benchmark ELF executables | 42 | 1,252.26 MiB |
| `docs/log/evidence/run-*/` successful run transcripts and receipts | 6,957 | 42.62 MiB |
| `docs/channel/evidence/implementation/run-*/` successful run transcripts and receipts | 2,496 | 0.76 MiB |
| `docs/collections/evidence/run-*/` successful run transcripts and receipts | 6,510 | 14.99 MiB |
| `docs/net/http/evidence/ci41307437931/original/run-logs.zip` | 1 | 1.63 MiB |
| `docs/voidup/evidence/ci-{42377496702,42427327049,42446206797}/original-ci.tar.gz` | 3 | 28.36 MiB |
| `docs/compiler-health/stabilization/evidence/llvm-ir-native.tar.xz` | 1 | 13.87 MiB |
| **Total** | **16,010** | **1,354.48 MiB** |

The globs summarize the manifest. It selects three output files per successful routine run, verifies expected stdout where recorded and leaves input sidecars and nonzero runs in place. The HTTP ZIP's 14 members match tracked extracted blobs. Eleven other HTTP ZIPs lack a complete byte-identical tracked extraction and stay outside the set. The compiler-health archive contains generated IR, objects and measurement outputs. Its source and measurement archives remain available.

| Largest tracked documentation groups | Current size | Keep or review |
| --- | ---: | --- |
| `docs/log/evidence/` | 1,480.22 MiB | Keep the three frozen baseline executables, source/patch material, accepted measurements and unresolved incident inputs. |
| `docs/net/http/evidence/` | 96.68 MiB | Keep provider provenance and both curl patches consumed at configure time. Review remaining old CI and dependency captures separately. |
| `docs/channel/evidence/` | 82.19 MiB | Keep archived investigation inputs, generated source sidecars, typed IR anchors and failure evidence pending reproduction review. |
| `docs/voidup/evidence/` | 76.27 MiB | Keep source patches, indexes, provider notices, native verification and package/provenance inputs. |
| `docs/collections/evidence/` | 39.01 MiB | Keep failures, input oracles and maintained test data. |
| `docs/stage10/` | 36.45 MiB | Separate archived console observations and source snapshots from current portability and reproduction inputs before another pass. |
| `docs/compiler-health/stabilization/evidence/` | 31.45 MiB | Keep source snapshots and unresolved H1–H7 inputs. |
| `docs/json/evidence/` and `docs/math/evidence/` | 49.77 MiB | Keep oracle inputs and failure provenance until their readers and unique cases are mapped. |

Preserve all Markdown, implementation source, maintained tests and examples, build configuration, vendor source, patches and licenses. `docs/text-encoding/evidence/unicode.json` and the crash recorder's synthetic fixture are examples of required inputs under evidence paths. Local Slice 7 screenshots and transcripts are outside this tracked set. The proposed checkout would contain **53,102 files**, including about **590 MiB under `docs/`**. These are checkout savings. Older blobs remain in Git history.

## Reproduction and link corrections before cleanup

1. **VOIDUP.** Three relative links in `docs/voidup/INTEGRATION_REPORT.md` would break at lines **347**, **1460** and **1905**. Point each to its adjacent retained restoration index, and state that the original archive is available from the audited commit with `git show <audited-commit>:<archive-path>`. Keep the in-page job outcomes and native replay commands. They already record the later Windows full 351/351 results per profile and distinguish them from the original hosted failures. [VOIDUP qualification](../voidup/INTEGRATION_REPORT.md)
2. **HTTP.** The triage's inline reference to `original/run-logs.zip` should point to the tracked `original/run-logs/` directory. Keep the job IDs, original ZIP hash, Linux failure/skip counts and the local scope of the Windows filter and macOS configure controls. [HTTP triage](../net/http/CI41307437931_TRIAGE.md)
3. **Logging, channel, collections and compiler stabilization.** Keep the acceptance tables and checkout-relative test commands. Logging records 88 Release, 88 Debug and 87 strict selected passes for the repair cohort. Channel records 20, 20 and 19, collections records 10 per profile, and stabilization records 94, 94 and 93 for its selected suites. Clarify that logging's measured ELF copies are historical outputs and keep the frozen baseline and rebuild scripts. For current performance disposition, the FILE200 10 ms aim remains unmet and Boss treats it as an aim. Keep the measured cohort and native-platform limits. [Logging acceptance](../log/ACCEPTANCE.md) · [Performance decision](../log/LOG_PERF_DECISION.md) · [Channel handoff](../channel/HANDOFF.md) · [Collections validation](../collections/VALIDATION.md) · [Stabilization verification](../compiler-health/stabilization/VERIFICATION.md)
4. **Slice 7.** The link scan found 293 distinct evidence targets already absent from the committed tree. This manifest creates none of those gaps. Keep the final Linux **443/443 Release, 456/456 Debug and 433/433 fatal sanitizer** results and Windows 10 **150 selected cases per profile**. Move the exact historical Windows selector and the raylib Win32 software-build recipe into the retained closeouts before retiring the local runners. Keep the distinction between headless leak-enabled tests and the visible GUI's documented driver leak. [Slice 7 closeout](SLICE7_FINAL_CLOSEOUT.md) · [Raylib preparation and limitations](SLICE7_RAYLIB_INTEGRATION.md)

For a replay, use a disposable checkout, LLVM/Clang/LLD **23.1.4**, CMake, Ninja, Python 3.12 or newer, and the attested profile-matched host/HTTP prefixes. Keep the SDK, provider source archives and notices required by the verifiers. Use the build guide's configure command with `BUILD_TESTING=ON`. Use Release, Debug with `VOID_LLVM_RESEARCH=ON`, or RelWithDebInfo with `VOID_SANITIZERS=ON` for the corresponding Linux profile. For strict, set `ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Run on the native host with a private temp directory. Build all targets, then run the complete suite or select an affected group using the registered test names below. [Pinned build and install procedure](../SOURCE_COMPILER.md#building-the-compiler)

```sh
export VOID_TEST_STDLIB="$PWD/stdlib"
cmake --build "$build" --parallel 2
ctest --test-dir "$build" --output-on-failure
ctest --test-dir "$build" --output-on-failure -R '^(log_|lsp_log$|channel_|lsp_channel$|collections_stdlib_|lsp_collections_stdlib$|http_ci_policy$|http_install$|voidup_|ffi_g[1-5]_|runtime_callbacks$|cli_install)'
```

The focused selector was checked against the accepted CTest registry. Retain the maintained benchmark scripts for explicit measurement, including `tests/log/performance_build.py` and `tests/log/performance_run.py`. Exact historic comparison requires the recorded source cohort and benchmark controls, including the hard 1 MiB stack where specified. Before a later deletion, check links and run the affected registered suites from a clean candidate checkout. This proposal did not rerun the qualification suites.

## Local workspace

The checkout measures about **368 GiB** by `du`. The major contributors are **304.91 GiB** in `build/`, **44.97 GiB** in `.toolchains/`, **12.52 GiB** in `experiments/`, **2.36 GiB** in `docs/` and **1.08 GiB** in `.git/`. The Slice 7 repair and integrated parent directories account for **95.22 GiB** and **53.09 GiB**, including build copies, source mirrors, symbol archives and relocated installations. Keep the active `build/llvm23-{release,debug,strict}` trees, totaling **41.31 GiB**, and the pinned SDK and live provider prefixes.

| Separate local proposal | Named scope | Estimated data blocks released |
| --- | --- | ---: |
| First review | Nine Release/Debug/strict build trees in `build/slice7-final-integrated/linux-*` and `build/slice7-final-link-repair/{linux-,native-linux-}*`. This excludes sibling source mirrors, packages and archives. | **43.41–88.66 GiB** |
| Older builds, after checking manual consumers | `build/{ci-official-san,step42-ci96-san,ci-official-debug,step42-ci96-debug,ci-official-sanitizers,ci-sanitizers}` | **50.24 GiB** |
| Obsolete SDK and investigation copies, after checking the retained inputs | `.toolchains/{ci-investigation,http/tls-review,http-io-baseline,wrench-inspection,http2-window-review}` | **20.31–21.00 GiB** |
| Exact tracked manifest | The tracked files above | **1.06–1.35 GiB** |

The nine Slice 7 trees charge **136.08 GiB** by path, but many of their blocks are reflinked. These estimates use XFS extent maps. The low figure counts unshared data blocks and the high figure counts distinct candidate blocks, some of which may have surviving references. The first local set plus the tracked cleanup could release **44–90 GiB**. The older-build and toolchain groups are conditional follow-ups. Keep the active LLVM 23.1.4 SDK, pinned provider prefixes and required corresponding-source downloads. Some old LLVM 19.1.7 and archived SDK files are still named in manual reproduction notes. Check those users before removing any local tree. The accepted Windows recovery was **9.87 GiB**.

## Future default

Put qualification outputs in task-owned temporary directories and retain failures only while they are actionable. Move current writers that target `docs/*/evidence` to scratch in a separate test-infrastructure change. Keep existing assertions, registrations and fixture inputs. Use short-lived CI artifacts for failure diagnosis. Record accepted outcomes, platform scope and replay commands in Markdown, then remove temporary builds and result bundles once the milestone closes.
