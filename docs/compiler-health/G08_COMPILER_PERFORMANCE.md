# G08 compiler and editor performance

> 12 November 2026 follow-up: [G19 attribution and isolated experiment](G19_COMPLETION_ATTRIBUTION.md)
> uses the integrated G18 baseline `3c81b7eb75c44533a5326da589a6fe47144c3c0f`
> and the unchanged frozen project. Its new timings are separate from the G08
> results below. The experiment's project-root disappearance behavior differs
> from production; the exact-equivalence gate and production optimization HOLD
> remain active. G19 is IN PROGRESS pending Golden Wrench review.

Baseline `561bd020d9904dcfa2dc792297f9e87688e89f5e`. Local performance
qualification on PikaOS 4 x64 and the Windows 10 Home x64 VM. Production
source and test registrations remain at the baseline during measurement.
[Milestone register](../roadmap/MILESTONE_REGISTER.md)

## Frozen workload and procedure

Inventory Report is a manifest application with six local sources and a
locked local library. It computes stock quantities and values from a
managed inventory, value-structure items, generic i32 and string
specializations, named multiple returns and standard-library text
processing. The normal entry prints `inventory 7 7 58`. The alternate
entry, referenced module and unreferenced draft exercise separate editor
contexts. One LSP assertion verifies the exact UTF-16 position of a
function token after both a BMP character and a surrogate pair. The
fixture checks deterministic emitted object bytes and verifies each
JIT and independently linked AOT result.
[Fixture](../../tools/bench/compiler_project) · [Benchmark driver](../../tools/bench/compiler_project.py)

Use the exact baseline compiler and server built with LLVM, Clang and LLD
23.1.4 at LLVM revision `7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`.
Use the profile-matched verified providers, Linux AWS-LC and Windows
Schannel. Keep `void-native.txt` beside the compiler. The independent
link uses that manifest and the exact LLD. On Windows the benchmark
checks the actual Clang driver launch plan before linking.

| Host | Execution and build conditions |
| --- | --- |
| PikaOS 4 x64 | Linux `7.2.11-pikaos`, Ryzen 7 1700X with 16 logical CPUs and 31 GiB reported RAM, performance governor. The measured process tree was pinned to CPUs 4–7 on separate physical cores. Project scratch used local XFS. CMake 4.3.4 and Python 3.14.8. Debug uses `-g`. Release uses `-O3 -DNDEBUG`. |
| Windows 10 Home x64 | Registry product name and NT version `10.0.19045.6466`, VirtualBox guest with four vCPUs and 8,000 MiB configured memory. Project scratch used private local NTFS. CMake 4.4.3, Python 3.14.8 and Visual Studio 2022 x64 environment 17.14.42. Clang-CL Debug uses `/Zi /Ob0 /Od /RTC1`, Release uses `/O2 /Ob2 /DNDEBUG`, and both use the existing static CRT. |

Compiler build profile and application optimization are independent.
Normal JIT, emit and build pass `--no-opt`. O2 uses the default Release
optimization for this manifest. The uncached JIT lane passes
`--no-cache`. Cache-status controls use `--cache` in the Debug application
mode and check automatic persistence separately with `--profile release`.

Run from a native developer environment. On Linux point `TMPDIR` at a
private local scratch directory. On Windows set both `TMP` and `TEMP` to
the verified private NTFS scratch directory and initialize the x64 Visual
Studio environment. Use a new output path outside the checkout.

```sh
TMPDIR=SCRATCH taskset -c 4-7 python3 -B tools/bench/compiler_project.py BUILD --repetitions 20 --queries 10 --output SCRATCH/baseline.json
```

Use the CPU set appropriate to the replay host. On Windows use `python`
with the same benchmark arguments. Each repetition creates
separate disposable CLI and editor projects from the same byte-identical
inputs and resolved lock. A fresh process and absence of previous
project cache artifacts define the first-run cases. Repeated CLI commands
use fresh processes against the same project. Repeated LSP requests use
one running server at one accepted revision. OS pages are left under
normal host policy. The local hosts run serial cohorts. No hosted run
or timing acceptance threshold is part of G08.

The driver reports wall-clock milliseconds from a monotonic timer.
It excludes project copying, oracle validation and executable hashing
from the action intervals. LSP change timings include notification,
compiler work, diagnostics and a response barrier. Query timings
include the actual request and response. They exclude client rendering.
Median, nearest-rank p95 and worst sample are reported alongside sample
counts. Each edit and each first query has 20 observations per measured
profile. Repeated query methods each have 200 observations.

### In-process phase replay on Linux

The optional phase driver uses the same compiler libraries and application
bytes. It times validated project analysis, typed IR lowering and each
native object emission after lowering. The emission order alternates
between normal and O2. Analysis includes filesystem capture and dependency
checks. LLVM IR construction, optimizer passes, target code generation and
object writing remain combined inside the emission interval. The independent
link measurements in the Python driver time the link driver and linker
after object emission. [Phase probe](../../tools/bench/compiler_phases.cpp)

For the configured Linux Unix Makefiles builds, build the existing
`void_cache_tests` target first. Use its compile and link recipes with
the standalone phase driver in place of the test objects. From the
checkout, pass an existing private scratch directory and build directory
as `SCRATCH` and `BUILD`.

```sh
python3 -B - BUILD SCRATCH <<'PY'
import json, shlex, subprocess, sys
from pathlib import Path

build, scratch = (Path(p).resolve() for p in sys.argv[1:])
source = Path("tools/bench/compiler_phases.cpp").resolve()
recipe = next(x for x in json.loads((build / "compile_commands.json").read_text())
              if x["file"].endswith("/tests/cache/tests.cpp"))
compile_args = shlex.split(recipe["command"])
obj = scratch / "phase-driver.o"
compile_args[compile_args.index("-o") + 1] = str(obj)
compile_args[compile_args.index("-c") + 1] = str(source)
subprocess.run(compile_args, cwd=build, check=True)
link = shlex.split((build / "CMakeFiles/void_cache_tests.dir/link.txt").read_text())
first = next(i for i, arg in enumerate(link) if arg.endswith("/tests/cache/tests.cpp.o"))
link[first] = str(obj)
link = [arg for arg in link if not (arg.startswith("CMakeFiles/") and arg.endswith(".o"))]
link[link.index("-o") + 1] = str(scratch / "compiler_phases")
for i, arg in enumerate(link):
    if arg.startswith("--dependency-file="):
        link[i] = "--dependency-file=" + str(scratch / "link.d")
    if arg.startswith("-Wl,--thinlto-cache-dir="):
        link[i] = "-Wl,--thinlto-cache-dir=" + str(scratch / "thinlto")
subprocess.run(link, cwd=build, check=True)
PY
cp -a tools/bench/compiler_project SCRATCH/project
mkdir -m 700 SCRATCH/objects
BUILD/voidc resolve --project SCRATCH/project
SCRATCH/compiler_phases SCRATCH/project SCRATCH/objects 20
```

## Work reuse and authority

| Path | Current work and reuse |
| --- | --- |
| Project open, edits and errors | `ToolingSession` creates a new snapshot identity and strictly increasing revision for changes. `analyze_manifest_project_impl` captures authorized sources, binds and validates the graph, and rechecks membership and bytes before accepting a valid analysis. The project document checks admit the open overlays. An unreferenced source gets its own analysis and index. G14/G16 can run a bounded recovery analysis. The last successful result is used for invalidation planning. Semantic publication requires the current snapshot. |
| Hover and semantic tokens | Queries use the current immutable compiler-owned index. Hover resolves declarations and references. Semantic tokens check exact open bytes, compute ranges in UTF-16, and encode the response. Neither requires a new execution graph on the same revision. |
| Completion and signature help | `editor_query` builds exact overlays and calls compiler assistance. The compiler performs an initial project capture, computes the cursor probe, and for member completion enters a second capture and compiler binder with the repaired text. Candidate collection may return from inside the probe before the ordinary successful analysis path finishes. The probe can observe a changed package or lock at the same document revision before a client event. |
| CLI check | Every process analyzes and validates source before command dispatch. Project capture includes resolver checks. `check` stops before lowering or execution. Resolver capture and recheck protect filesystem and lock authority. |
| JIT and cache | The key is derived from validated source, manifest/project selection, compatibility, target, profile, build/ABI and optimization identities. A hit reads and validates the envelope and native object, establishes the expected Runtime/application context and loads the object into ORC. It bypasses lowering and code generation. A miss follows the ordinary compile path. Windows takes the cold fallback when the store is unavailable. |
| Native emit and build | Both perform fresh source analysis. Emit lowers typed IR, builds LLVM IR, runs the selected pipeline, writes the object and publishes it. Build also verifies the driver/resources, links and publishes the executable. Neither uses the JIT object store. |

[Tooling session](../../src/frontend/tooling.cpp) · [LSP dispatch](../../src/lsp/main.cpp) · [Compiler cursor probe](../../src/frontend/compiler.cpp) · [Project capture](../../src/frontend/project_selection.inc) · [Resolver](../../src/frontend/project_resolution.cpp) · [LLVM and ORC](../../src/backend/llvm/backend.cpp) · [Native build](../../src/native_build.cpp)

## Native baseline measurements

The timed cohorts ran on 12 November 2026, Windows Debug then Release
followed by Linux Debug then Release. The Linux desktop's one-minute
load average ranged from 3.47 to 4.41 at cohort boundaries. The guest
build and CTests were outside the timed cohorts. These are local
measurements on a shared desktop and its guest.

All five runs verified the same resolved lock SHA-256
`f4d776e773bcd7d3fef845b90e92bb78279d6830fd477e0c5e49c017edb1332f`
and fixture aggregate SHA-256
`063a086a45584eed8d7740d9641efd164ce8125593c09d0c4a74ac31d3646ab3`.
The aggregate hashes the sorted compact JSON map of relative file names
to file digests. Benchmark driver SHA-256 was
`ecbfe058a2d7bed74a3e3cf12cc3013daa2aa73d94a357fede209c4f723eef28`.

| Profile | Embedded compiler build identity | `voidls` SHA-256 |
| --- | --- | --- |
| PikaOS Debug | `9ebfc400abbd52d1b951eeb5f0af33ed57fb5619564a5bb7a49a49c52726cfa5` | `590cdba02f0e2476faeb91794d153c381446489e046e2092099ccc0ceb639477` |
| PikaOS Release | `f98b790280f7876a35988a3cbe7905a6529b7b6a0acf8a2791169b904ec24fd4` | `114865c9d2340abf4e9a5f2863f56d6b22018f64487e7359bf901873a350fc7f` |
| PikaOS strict | `b12a00f43c24bde109dbc01a31167ae24e461f356d01b06050f7c36616397478` | `a06ac242f6a944c148e78547fa91725c7a697f169abfc306b2fac0abb2299493` |
| Windows Debug | `e2d74d576393374b5614d18f82a47ea421901b83b0311515ce0377894c5d2b65` | `a19db6e74d07c5ee60c5f96fc8a4acb0b50f7f7f75366387ea084febdc85ca76` |
| Windows Release | `220eda6783d1574319d1b80366bbbc248112803d1a53dbbfe70c6ee93e2368fc` | `9beef58055c62add47120d8a5955f8a0b98f227413979a20bcd2b4dd5abad10a` |

Cells show **median / p95 / worst** in milliseconds. All observations
are included. Repeated queries have ten samples from each of 20 server
sessions. Strict Linux was a correctness cohort.

### Editor timings

| Action | n | Linux Debug | Linux Release | Windows Debug | Windows Release |
| --- | ---: | ---: | ---: | ---: | ---: |
| Server spawn | 20 | 0.709 / 1.805 / 2.341 | 0.688 / 1.422 / 1.515 | 8.117 / 9.156 / 9.179 | 8.088 / 14.976 / 57.195 |
| Initialize response | 20 | 2.098 / 3.012 / 3.015 | 1.785 / 2.084 / 2.576 | 7.227 / 8.368 / 8.610 | 7.165 / 9.214 / 10.325 |
| Project open and diagnostics | 20 | 194.703 / 197.491 / 201.651 | 28.765 / 30.785 / 32.154 | 319.719 / 349.693 / 350.288 | 104.519 / 110.904 / 136.891 |
| Entry document open | 20 | 199.778 / 206.701 / 218.263 | 28.916 / 30.301 / 31.842 | 320.314 / 350.325 / 364.617 | 100.228 / 113.376 / 118.570 |
| Edit local body | 20 | 199.735 / 210.775 / 226.294 | 28.043 / 30.615 / 32.516 | 323.039 / 335.424 / 337.895 | 96.780 / 102.636 / 105.583 |
| Edit referenced module | 20 | 199.007 / 206.996 / 218.843 | 28.779 / 31.742 / 33.780 | 324.528 / 348.428 / 361.361 | 98.693 / 103.163 / 104.814 |
| Edit unreferenced draft | 20 | 210.128 / 216.527 / 260.475 | 32.565 / 37.943 / 39.113 | 359.057 / 387.977 / 388.498 | 120.469 / 124.969 / 132.144 |
| Completion, first | 20 | 52.281 / 58.220 / 59.128 | 9.978 / 10.817 / 16.204 | 102.228 / 110.858 / 115.222 | 53.509 / 56.635 / 57.269 |
| Completion, repeated | 200 | 51.840 / 56.361 / 84.787 | 9.658 / 10.991 / 16.869 | 101.697 / 109.677 / 122.057 | 54.048 / 59.126 / 80.605 |
| Hover, first | 20 | 0.315 / 0.382 / 0.410 | 0.155 / 0.301 / 1.025 | 8.095 / 13.080 / 14.087 | 8.350 / 11.288 / 12.854 |
| Hover, repeated | 200 | 0.315 / 0.448 / 0.550 | 0.159 / 0.220 / 0.361 | 7.888 / 14.963 / 16.194 | 8.036 / 11.530 / 16.702 |
| Semantic tokens, first | 20 | 0.322 / 0.542 / 0.612 | 0.124 / 0.196 / 0.245 | 15.802 / 16.173 / 16.286 | 15.347 / 15.891 / 16.356 |
| Semantic tokens, repeated | 200 | 0.334 / 0.488 / 1.581 | 0.122 / 0.193 / 1.157 | 15.568 / 16.067 / 16.534 | 15.571 / 15.990 / 16.154 |
| Introduce syntax error | 20 | 68.671 / 75.113 / 85.187 | 15.287 / 18.512 / 25.620 | 147.861 / 163.180 / 180.414 | 88.464 / 103.872 / 112.457 |
| Repair syntax error | 20 | 209.482 / 221.634 / 221.927 | 31.850 / 38.290 / 52.249 | 354.793 / 364.706 / 378.644 | 120.395 / 128.907 / 132.105 |
| Valid to invalid dirty text | 20 | 60.789 / 63.650 / 70.267 | 13.259 / 14.746 / 18.339 | 136.504 / 140.883 / 142.541 | 76.978 / 84.754 / 93.371 |
| Invalid to valid dirty text | 20 | 208.025 / 219.421 / 219.554 | 32.158 / 34.941 / 36.833 | 359.664 / 385.552 / 401.908 | 122.649 / 128.419 / 128.633 |
| Switch to audit entry | 20 | 223.561 / 233.032 / 252.814 | 36.521 / 38.393 / 39.088 | 404.729 / 435.174 / 470.395 | 151.101 / 159.611 / 161.170 |
| Switch back to main | 20 | 206.942 / 222.683 / 226.552 | 32.667 / 35.825 / 40.715 | 355.346 / 376.715 / 386.130 | 121.903 / 131.847 / 137.659 |
| Rename referenced source | 20 | 40.553 / 42.855 / 44.504 | 10.365 / 11.033 / 13.925 | 95.727 / 106.642 / 107.769 | 59.237 / 69.728 / 71.317 |
| Repair renamed import | 20 | 206.084 / 221.408 / 233.727 | 32.353 / 35.738 / 37.152 | 363.848 / 379.894 / 386.670 | 122.194 / 134.440 / 136.125 |
| Remove referenced source | 20 | 39.866 / 41.594 / 41.930 | 10.132 / 11.219 / 14.332 | 105.053 / 112.150 / 113.325 | 68.562 / 73.670 / 74.867 |
| Restore referenced source | 20 | 206.890 / 220.034 / 223.171 | 32.550 / 38.239 / 42.661 | 358.033 / 394.701 / 402.631 | 121.310 / 134.617 / 137.012 |

### CLI and cache timings

| Action | n | Linux Debug | Linux Release | Windows Debug | Windows Release |
| --- | ---: | ---: | ---: | ---: | ---: |
| CLI launch and version | 20 | 5.236 / 6.681 / 7.118 | 5.495 / 5.993 / 6.197 | 21.440 / 24.284 / 25.854 | 18.694 / 20.417 / 22.074 |
| Check, first | 20 | 195.863 / 203.014 / 205.851 | 34.967 / 38.437 / 39.233 | 342.369 / 363.246 / 365.208 | 115.806 / 127.312 / 153.335 |
| Check, repeated | 20 | 195.990 / 215.302 / 221.876 | 32.933 / 35.444 / 35.544 | 314.487 / 330.618 / 334.653 | 93.105 / 100.230 / 107.274 |
| Resolve locked graph | 20 | 10.365 / 11.829 / 14.275 | 7.802 / 8.581 / 9.795 | 47.294 / 53.372 / 59.270 | 39.727 / 41.606 / 42.854 |
| Normal JIT, bypass | 20 | 640.646 / 695.663 / 698.555 | 159.016 / 170.714 / 178.681 | 1096.571 / 1154.121 / 1158.358 | 309.695 / 336.257 / 337.847 |
| O2 JIT, bypass | 20 | 661.485 / 710.313 / 722.335 | 174.670 / 190.194 / 197.420 | 1134.512 / 1166.906 / 1174.185 | 346.913 / 368.668 / 370.107 |
| Emit normal object | 20 | 1090.835 / 1172.290 / 1223.780 | 408.410 / 434.827 / 437.973 | 1810.841 / 1918.591 / 1922.122 | 679.283 / 709.318 / 781.441 |
| Emit O2 object | 20 | 1191.439 / 1247.521 / 1272.022 | 501.377 / 531.979 / 533.127 | 1944.839 / 1985.715 / 2138.325 | 824.608 / 878.482 / 878.534 |
| Independent normal link | 20 | 70.517 / 77.744 / 81.521 | 41.095 / 45.389 / 47.104 | 252.374 / 272.721 / 297.807 | 177.317 / 189.070 / 197.886 |
| Independent O2 link | 20 | 73.127 / 79.771 / 80.164 | 38.394 / 43.470 / 46.483 | 231.662 / 243.076 / 246.918 | 168.598 / 179.939 / 188.498 |
| Independent normal AOT execution | 20 | 3.251 / 4.154 / 4.684 | 2.625 / 3.036 / 3.631 | 1074.016 / 1131.907 / 1188.183 | 430.880 / 483.714 / 508.431 |
| Independent O2 AOT execution | 20 | 3.426 / 4.580 / 4.809 | 2.641 / 3.053 / 3.298 | 533.191 / 571.472 / 578.005 | 203.440 / 215.184 / 231.696 |
| Build normal, first | 20 | 1051.937 / 1092.002 / 1125.100 | 310.664 / 327.080 / 339.834 | 2049.900 / 2146.913 / 2262.434 | 712.168 / 742.534 / 784.749 |
| Build normal, repeated | 20 | 1048.959 / 1122.236 / 1140.257 | 312.076 / 346.585 / 357.172 | 2005.368 / 2111.596 / 2532.162 | 705.153 / 747.266 / 752.625 |
| Build O2, first | 20 | 1071.351 / 1101.055 / 1150.884 | 333.484 / 347.922 / 363.834 | 2067.384 / 2446.646 / 2572.380 | 739.925 / 776.339 / 821.120 |
| Build O2, repeated | 20 | 1075.795 / 1094.016 / 1097.373 | 330.446 / 361.150 / 367.176 | 2056.463 / 2167.621 / 2246.437 | 726.489 / 779.916 / 837.833 |
| Debug cache requested, first | 20 | 659.035 / 685.684 / 691.273 | 171.595 / 190.456 / 209.425 | 1103.420 / 1134.213 / 1141.850 | 315.818 / 331.275 / 331.870 |
| Debug cache requested, repeated | 20 | 201.522 / 209.167 / 228.783 | 36.456 / 41.861 / 42.580 | 1094.359 / 1134.230 / 1148.131 | 305.835 / 324.034 / 324.886 |
| O2 automatic cache, first | 20 | 673.020 / 710.414 / 738.129 | 192.652 / 206.401 / 250.224 | 1121.882 / 1171.156 / 1200.291 | 343.034 / 369.332 / 383.239 |
| O2 automatic cache, repeated | 20 | 199.687 / 221.907 / 225.766 | 36.972 / 40.589 / 47.118 | 1119.406 / 1157.696 / 1177.744 | 336.046 / 362.370 / 370.489 |
| Debug cache after source edit | 20 | 651.613 / 687.611 / 732.320 | 174.103 / 192.886 / 240.049 | 1085.487 / 1139.284 / 1184.661 | 310.765 / 340.392 / 382.265 |
| Debug cache after manifest edit | 20 | 657.528 / 677.577 / 683.901 | 173.850 / 181.625 / 222.504 | 1095.849 / 1133.253 / 1133.324 | 312.192 / 339.602 / 345.103 |
| Refuse stale dependency lock | 20 | 8.389 / 11.334 / 12.079 | 6.886 / 7.250 / 11.351 | 35.455 / 37.947 / 39.316 | 30.050 / 32.572 / 34.330 |
| Debug cache after lock refresh | 20 | 661.821 / 687.383 / 692.893 | 173.831 / 196.904 / 276.693 | 1096.182 / 1122.323 / 1135.325 | 307.524 / 331.546 / 333.097 |

### Repeatability and observed costs

| Profile | Repeated completion median by five-session block (ms) | O2 object emission median by five-run block (ms) |
| --- | --- | --- |
| PikaOS Debug | 52.233 / 51.693 / 52.107 / 51.195 | 1,229.340 / 1,185.163 / 1,197.715 / 1,181.935 |
| PikaOS Release | 9.706 / 9.525 / 9.746 / 9.688 | 506.678 / 501.694 / 493.840 / 525.426 |
| Windows Debug | 100.941 / 101.633 / 100.739 / 102.335 | 1,950.358 / 1,924.942 / 1,966.591 / 1,955.331 |
| Windows Release | 54.112 / 53.893 / 54.882 / 53.066 | 866.967 / 825.478 / 852.416 / 809.299 |

Project reselection and valid edits drive the highest editor latencies.
Repeated completion remains substantially slower than indexed hover and
semantic tokens on each profile, including in all four five-session
blocks. Opening and editing an independent draft still involves project
analysis. Its selection key stayed unchanged in every unreferenced-edit
control. That result isolates editor state from the application graph.

Object emission and native builds are the largest one-shot compiler
operations measured. `emit` uses the all-exports mode. `build` emits
the application entry closure and also links and publishes. Their wall
times cannot be subtracted to estimate the link. The independent linker
lane provides the separate link measurement. `resolve --locked` times
the resolver with its own capture and lock checks. It is a filesystem
and dependency control with a different path from `check`.

PikaOS language-server end-of-session RSS and peak RSS reported the
same median / p95 / worst in each profile. Debug measured
23,686 / 23,872 / 23,880 KiB. Release measured
22,852 / 23,036 / 23,320 KiB. These are one-workload process observations.

An additional transport control called `Client.initialize()` on the same
resolved project, then timed 40 sequential `Client.sync()` calls in each of
five fresh sessions. No document mutation or diagnostic notification
occurred during the timed barriers.

| Profile | n | Idle barrier median / p95 / worst (ms) |
| --- | ---: | ---: |
| PikaOS Debug | 200 | 0.083 / 0.133 / 0.216 |
| PikaOS Release | 200 | 0.054 / 0.106 / 0.693 |
| Windows Debug | 200 | 15.798 / 16.034 / 16.476 |
| Windows Release | 200 | 15.798 / 16.075 / 16.812 |

The Windows server polls pipe readiness with `PeekNamedPipe` and
`Sleep(1)`. The idle result shows a substantial round-trip cost even
without an editor query. Its scheduling and request order differ from
the main cohort, so it supplies a transport control without attributing
all of that cost to one call or subtracting it from query medians.
[Server transport](../../src/lsp/main.cpp) · [Replay client](../../tools/lsp_client.py) · [Windows wait behavior](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep)

### Isolated Linux compiler phases

| Phase | n | PikaOS Debug | PikaOS Release |
| --- | ---: | ---: | ---: |
| Validated project analysis | 20 | 180.938 / 197.029 / 197.129 | 21.222 / 23.076 / 23.652 |
| Typed IR lowering | 20 | 263.008 / 274.416 / 297.686 | 24.391 / 27.401 / 27.650 |
| LLVM and object emission, normal | 20 | 664.142 / 701.747 / 705.655 | 350.106 / 377.509 / 388.531 |
| LLVM and object emission, O2 | 20 | 780.774 / 836.824 / 861.081 | 443.454 / 488.366 / 497.494 |

The phase probe's peak RSS was 58,600 KiB in Debug and 57,092 KiB in
Release, measured with Linux child-process resource usage. It isolates
LLVM plus object emission as the largest measured Linux phase. The O2
interval includes differences in LLVM optimization and code generation.
It does not isolate optimizer pass time. Windows has CLI and independent
link boundaries in this record.

### Cache behavior

In each Linux profile, the explicit Debug cache sequence returned 20
cold misses and 20 warm hits. The median fell from 659.035 to 201.522 ms
in Debug and from 171.595 to 36.456 ms in Release, reductions of 69.4%
and 78.8%. These are existing cache benefits. Every changed source,
valid manifest edit and refreshed
dependency generated a distinct key. The stale-lock request failed before
cache status or execution, and refresh restored valid execution with
the new result. The automatic O2 path published a distinct key and
preserved its bytes and timestamp on repeat. The maintained
`debug_cache` fixture separately asserts native O2 hit status and
fresh Runtime ownership.

Both Windows profiles returned `Debug cache: unavailable` on all
explicit cache requests and created zero persistent objects. Automatic
O2 created no cache directory. Stale-lock refusal, checked refresh
and the changed output still worked. In all profiles, normal and O2 JIT
and independently linked AOT produced the expected output. The
maintained cache suites check malformed envelope and object rejection
on Linux, source authority, contention, fallback and key identity.

## Optimization disposition

**HOLD production optimization.** An exact-position completion memo tied
to the accepted revision could avoid repeated cursor analysis. The
same-revision control changed the locked package source on disk without a
watcher event. Completion refused the request while the selection revision
stayed unchanged. Restoring the package bytes restored the candidates.
The revision and cursor alone therefore cannot authorize a cached
completion. The current probe also handles incomplete tokens, scope,
visibility and package authority.

A future experiment can attribute the two project captures and the
cursor-repair binding pass separately. It would have to preserve all
filesystem and lock observations, keep any reuse bounded by the admitted
request or immutable analysis owner, and prove identical diagnostics,
spans and behavior on source, manifest, package, entry and close events.
Replay the same-revision lock control and the maintained selection before
comparing candidate latency and retained memory on both hosts. No
candidate source, before/after speedup or new timing requirement is claimed.

## Maintained correctness selection

The unchanged registrations cover project snapshots and dependency locks,
LSP UTF-16 ranges and guarded recovery, G14/G16, G15 JIT and native
links, Linux cache envelopes and object admission, the Windows cache
fallback, G07 sockets and G06 atomic saves.

```sh
selection='^(project_sessions|project_protocol|lsp_positions|lsp_editor_recovery|lsp_semantic_tokens|lsp_enum_recovery|lsp_multiple_returns|project_dependencies|project_dependency_explain|project_packages|debug_cache|debug_cache_processes|automatic_cache_processes|multiple_returns|net_sockets|io_atomic_save_native|io_atomic_save_source)$'
ctest --test-dir BUILD -j 2 --output-on-failure -R "$selection"
```

On Windows pass the same selection to CTest from the native x64
developer environment. Use the existing `void_windows_native_staging
--create-private-root` control with a fresh absolute path on UTF-8 stdin
to verify the scratch directory before setting `TMP` and `TEMP`.
On Linux keep the task-owned temporary directory owner-only. Strict Linux
uses `RelWithDebInfo`, `VOID_SANITIZERS=ON`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
Strict runs qualify correctness and leak detection.

| Profile | Focused CTests | Additional control |
| --- | --- | --- |
| PikaOS 4 x64 Debug | 17/17 across initial selection and targeted replay | First 15/17 in 104.66 s. Both G06 tests passed in the 33.54 s replay. |
| PikaOS 4 x64 Release | 17/17 across initial selection and targeted replay | First 16/17 in 29.37 s. Both G06 tests passed in the 12.87 s replay. |
| PikaOS 4 x64 strict | 17/17 in 125.20 s | Two frozen-workload correctness repetitions also passed. Zero ASan, UBSan or leak findings. |
| Windows 10 Home x64 Debug | 17/17 in 242.79 s | The existing Windows private-root probe admitted the test directory. |
| Windows 10 Home x64 Release | 17/17 in 77.59 s | Same test selection and verified NTFS scratch. |

CTest JSON and logs verified 17 active commands per profile, matching
timeouts and zero skips. The final selection passed across all five
profiles. Native macOS timing and Windows process memory are outside
this record.

The initial Linux runner used an intermediate `TMPDIR` mode of 0775.
Debug's two G06 tests and Release's native G06 test correctly refused
that ancestor. Source reported exit 6 and `io.create_private_directory
failed; native code 0`. Native reported `private fixture admission failed:
2/0`. The root was corrected to 0700 before the strict run. Both G06
registrations passed the targeted Debug and Release replays at mode 0700.

### Original pilot and setup observations

- The first fixture named its library package `Inventory Rates`. The
  resolver rejected it with `VOID5004`. The frozen fixture uses the
  valid package identifier `rates`. The early completion pilot at a
  module qualifier returned an empty list. The frozen completion query
  exercises checked value-structure fields with an exact oracle.
- Windows blocked `powershell.exe -NoProfile -File
  "\\vboxsvr\VM\void-g08.fmsPr6Ox\probe.ps1"` under script execution
  policy. The first CMD launcher also hit sharing errors on repeated
  redirections to one share file. The working launcher used CMD and
  Python with separate logs. `fsutil fsinfo volumeinfo C:` returned
  access denied. The ordinary `GetVolumeInformationW` query confirmed
  NTFS. These checks used the normal guest session.
  [Native volume query](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationw)
- Initial Windows configurations lacked the LLVM zlib lookup prefix and
  the corresponding-source nghttp2 archive. The corrected setup used
  the existing verified LLVM dependency prefix and matching archived
  provider inputs. The first scratch shim preparation failed its literal
  path assertion. The final setup delegated the unchanged host verifier
  to its byte-identical, provider-attested source root using the previous
  Slice 7 CMake include pattern. It compared recipe hashes against the
  frozen source and left the provider receipts unchanged.
  [Attested setup](../projects/SLICE7_FINAL_CLOSEOUT.md#qualification-scope-and-preserved-platform-boundaries)
- The first Windows Debug native build pilot refused the SDK path
  because it traversed a reparse point. Setting `VOID_CLANG` to the
  physical path of the same 23.1.4 driver allowed the original checks,
  native build and independent link to pass.
- Linux `perf record -e cpu-clock:u` and `perf record -e cycles:u`
  were denied with `perf_event_paranoid=2`. Timing used native
  phase boundaries and pipe/CLI observations. The first phase-launch
  script expected `/usr/bin/time`, which is absent on this host. The
  phase-only retry obtained peak RSS from Python's child resource usage.
  The completed main timing cohorts were retained.

## Windows persistent native cache

**HOLD persistence.** The `_WIN32` store remains unavailable. The
POSIX store uses descriptor-relative private directory admission,
no-follow opens, SHA-256 envelope checks and an exclusive stage followed
by same-directory no-replace publication. Existing native tests cover
corrupt/truncated envelopes, independently corrupted object payloads,
unsafe leaves, collisions and safe misses. Windows qualifies its
existing fallback. [Store](../../src/backend/llvm/native_cache.cpp) · [Backend cache validation](../../src/backend/llvm/backend.cpp) · [Cache tests](../../tests/cache)

| Design obligation | Windows evidence or remaining work |
| --- | --- |
| Root and namespace authority | Admit the project root and every cache component by held native identity. Prove ACL owner and write rights, non-reparse directories and leaves, single-link object identity and rename/ancestor exclusion. Inherited checkout or temp-folder ACLs alone are insufficient. Exercise local NTFS, unsafe junctions, hard links and distinct volumes. |
| Key and provenance | Keep the validated compiler graph and project/profile/optimization/build inputs authoritative. Bind object bytes to the exact key and expected entry, target, coordination, source and Runtime contracts. A content digest checks integrity under the private-owner assumption. An arbitrary writer able to replace the object and digest would defeat that assumption. Qualify changes to the actual compiler and ORC object loader identity. Keep the separate AOT driver's Clang and LLD identity checks in the native link path. |
| Bounded artifact admission | Define an object size budget, read exact bytes from the admitted held file, check truncation/trailing bytes and envelope, and qualify object-format rejection before ORC can publish anything. Maintain ordinary fresh compilation on every safe miss. |
| Publication and races | Stage exclusive private data on the same volume, write and flush complete bytes, and use held-handle no-replace publication. Qualify concurrent readers/writers and a concurrent winner. G06 verified that Windows classic replacement can fail even with reader delete sharing. Cache publication must leave any existing winner intact. |
| Recovery | Define a bounded stage policy, stale-stage identity-checked reclamation, corrupt-object refusal and behavior if a publication is visible before a later failure. Preserve the existing compiler result if optional cache I/O fails. Windows file-symlink denial on the guest remains a native fixture limitation. |

G06 supplies useful private-directory and held-file evidence for a
separate design. The application-save API has a different size policy,
caller contract and ownership boundary. Windows cache admission,
object provenance and recovery still need a dedicated native review.
[Private directory implementation](../../src/runtime/host_directory_windows.cpp) · [G06 contract](../io/CONTRACT.md#atomic-application-saves)

The Windows design review uses the documented reparse and sharing behavior,
same-volume held-root rename, and file flush semantics.
[Windows file opens](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew) · [Native rename](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information) · [Native flush](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)

## Historical performance boundary

FILE200 measures a different logging and durability workload with its
original acknowledged writes and native synchronization. Its retained
matched internal-repair cohort went from 16.265 to 14.015 ms median.
The strict 10 ms target remains unmet and advisory by Boss decision.
The earlier editor startup EOF and the recorded editor/HTTP and RSS
regressions retain their original scopes. G08 uses a different workload
and leaves those results intact.
[Integrated performance qualification](INTEGRATED_PERFORMANCE_QUALIFICATION.md) · [FILE200 reproduction and limits](../log/INTEGRATED_LOGGING_REFINEMENT_RESULT.md#matched-complete-workload-performance)

## Evidence retention

Removed the disposable guest source and build trees, host transfer
share, generated applications, raw timing data and diagnostic scratch
after extracting this record. Guest free space rose from
12,166,742,016 to 15,007,084,544 bytes. Linux tmpfs rose from
16,177,553,408 to 16,602,095,616 bytes, and host XFS rose from
367,797,063,680 to 367,798,255,616 bytes. The existing Linux build
profiles and verified provider inputs are retained for replay.
[Retention policy](../projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)

## Changed-file inventory

- Documentation — `docs/compiler-health/G08_COMPILER_PERFORMANCE.md`, `docs/compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `docs/roadmap/MILESTONE_REGISTER.md`.
- Benchmark drivers — `tools/bench/compiler_phases.cpp`, `tools/bench/compiler_project.py`.
- Frozen workload — `tools/bench/compiler_project/void.project.toml`, `tools/bench/compiler_project/deps/rates/void.project.toml`, `tools/bench/compiler_project/deps/rates/src/api.void`, `tools/bench/compiler_project/src/main.void`, `tools/bench/compiler_project/src/model.void`, `tools/bench/compiler_project/src/stock.void`, `tools/bench/compiler_project/src/report.void`, `tools/bench/compiler_project/src/audit.void`, `tools/bench/compiler_project/src/draft.void`.
