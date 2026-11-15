# G38 — Automatic SIMD and efficient VOID loops

**Hold default performance acceptance of the correctness-qualified runtime-free AOT candidate.** Two real new SIMD paths are profitable, but repeated managed JIT host-context regressions remain unresolved. The first all-module candidate is rejected for native regressions and remains recoverable. This is a real compiler change in an isolated detached G38 worktree from accepted G36 `1e606f8e5067a3113b1d4d4f850fd3a11e2e57a0`. The original exact nine-file G37-F worktree remains untouched and performance-held. Nothing is staged, committed or pushed; no hosted CI was used.

## Implementation and ownership

`src/backend/llvm/backend.cpp` registers LLVM23.1.4's existing nontrivial `SimpleLoopUnswitchPass` at the existing vectorizer-start extension point, using the existing loop adaptor and MemorySSA. Both AOT callers pass the existing validated `!input.coordinated` module fact; `finish` defaults the extra policy argument to false for existing JIT callers. The pass is registered only for optimized native AOT with that fact. LLVM owns loop legality and profitability. This is one bounded addition to the existing O2 pipeline, with no second optimizer, new metadata authority, allocator, collector, Runtime state or public ABI.

SLP remains disabled; generic deployment CPU/features, strict FP flags, normal AOT and normal/O2 JIT policy stay accepted. Ordinary invariant branches can choose a loop version before repeated execution, exposing integer arithmetic to LLVM's existing vectorizer. No annotation, unchecked access or extra instruction-set requirement is introduced. Managed/mutable/reference arrays, strings/dictionaries, native/callback/worker/async/cleanup-bearing modules retain the accepted coordinated pipeline. This deliberately does not claim general checked-array SIMD or new range/alias proof.

The existing frontend coordination fact covers managed types, collections, text, native operations, workers/async and cleanup; IR validation rejects inconsistent Runtime-dependent constructs. Arithmetic failure remains an observable call/control path. Unswitching does not authorize speculative division or reordered failure. The existing loop vectorizer must still prove legality. The new driver checks late division failure, integer wrap and inclusive/reverse/break/continue endpoints. Strict dependent f32/f64 operations retain scalar order, with no fast/reassoc/contract/nnan/ninf/nsz flags.

The opportunity review selected invariant source control because it hides otherwise legal counted integer reductions from the existing vectorizer. Scalar temporaries already flow through LLVM's accepted promotion/simplification pipeline; no additional VOID-side rewrite was justified. General bidirectional ranges still hide a useful trip-count fact in some shapes. Checked arrays retain Runtime geometry, bounds/failure paths, mutation/alias visibility and coordinated polls; this change supplies no new proof for removing those operations or lifting metadata across mutable calls. The broad trial's extra versioning did not improve those workloads. Integer reductions use existing wrapping semantics; loop-carried nonlinear and strict FP dependencies remain LLVM's responsibility and stay scalar in the controls. These observations bound the implemented opportunity without claiming array/range specialization is finished.

## Activation and negative controls

All eleven source functions are captured, including unused exported functions, rather than only the selected entry. Real production CLI objects match diagnostic emission byte for byte. Two previously scalar G36 functions (`variable_divisor`, `invariant_division`) now contain vector integer IR and packed SSE2 instructions. Their native object is identical to the first trial's proven runtime-free object. Normal AOT objects remain identical and all eleven normal/O2 JIT IR functions remain byte-identical across variants. Source disassembly contains no added AVX/YMM/ZMM/FMA requirement.

Unguarded bidirectional ranges, varying loop conditions, nonlinear dependencies, unsafe division branches and strict floating recurrences provide negative/fallback controls. The independent C++ oracle uses explicit unsigned arithmetic for wrap and bit-based strict FP comparisons, including NaN, signed zero, infinity and subnormals: 240 f32/f64 bit checks per AOT run, eight integer shapes over seventeen inputs, endpoint and failure controls. CMake maintains new normal/O2 independently linked AOT tests and numeric JIT source tests. Existing collection/index, alias, root, callback, cancellation, fault/recovery, reload and GC tests remain in the qualification selection.

| Function | G36 bytes | G38 bytes | G36 vector integer IR ops | G38 vector integer IR ops |
| --- | ---: | ---: | ---: | ---: |
| variable_divisor | 93 | 585 | 0 | 13 |
| invariant_division | 222 | 530 | 0 | 7 |
| dependent | 86 | 86 | 0 | 0 |
| fp32 | 162 | 317 | 0 | 0 |
| fp64 | 162 | 317 | 0 | 0 |
| bidirectional | 69 | 69 | 0 | 0 |

## Qualification

| Platform / mode | Final corrected source result |
| --- | --- |
| Linux Release | 77/77 selected CTests |
| Linux fatal ASan/UBSan/LeakSanitizer | 77/77, leak detection and halt/abort enabled |
| Fatal TSan | Runtime, allocator, races, dictionary-policy publication/forced GC, checked collections, 26-pattern normal/O2 independently linked AOT |
| Windows10 build19045 native Release | 63/63 selected CTests plus source/native oracles |
| Windows10 build19045 native Debug | 63/63 selected CTests plus source/native oracles |
| Stack / tools | Linux 1 MiB runtime stack; fourteen PE 1 MiB reserves in each Windows profile; existing LLVM/Clang/LLD23.1.4 |

Normal/O2 JIT and independently linked normal/optimized AOT execute the maintained source applications and result oracles. Native Windows uses Visual Studio x64 tools, SDK10.0.19041.0 and the existing verified providers; the VM's misleading label is “Windows11”, its actual OS is Windows10. Every affected compiler/Runtime consumer is rebuilt with matching headers. Linux provider recipes and verification remain unchanged: a read-only bubblewrap source projection preserves the SDK owner's canonical compiler paths without copying or rebuilding providers. Protected SDKs and previous archives are rehashed.

Cached stale managed payload access is deliberately required to fail with ASan `use-after-poison`; this expected failure is retained separately from passing tests. All remaining failures, retries and intentionally stopped first-trial builds are retained as such. No partial first-trial Debug/fatal result is counted as final qualification.

## Matched loop performance

Milliseconds per measured region; six alternating balanced baseline/candidate blocks, seven in-process samples each, independent oracles and CPU affinity. All four execution modes and small/large/negative/tail shapes are retained in raw JSON/TSV. This table includes both new vector paths and unfavorable/unchanged controls.

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| aot-optimized/bidirectional/1000000 | 1.27967 | 1.2775 | -0.17% |
| aot-optimized/dependent/1000000 | 4.43794 | 4.45289 | +0.34% |
| aot-optimized/invariant_division/1000000 | 0.984079 | 0.398347 | -59.52% |
| aot-optimized/invariant_division/1000001 | 4.06903 | 4.07305 | +0.10% |
| aot-optimized/variable_divisor/-7 | 6.27282 | 6.26243 | -0.17% |
| aot-optimized/variable_divisor/1 | 0.971945 | 0.384423 | -60.45% |
| aot-optimized/varying_condition/1000000 | 0.973305 | 0.969635 | -0.38% |
| jit-O2/bidirectional/1000000 | 1.29585 | 1.30955 | +1.06% |
| jit-O2/dependent/1000000 | 4.40036 | 4.39171 | -0.20% |
| jit-O2/invariant_division/1000000 | 0.93451 | 0.937971 | +0.37% |
| jit-O2/invariant_division/1000001 | 4.04059 | 4.04148 | +0.02% |
| jit-O2/variable_divisor/-7 | 6.23996 | 6.24038 | +0.01% |
| jit-O2/variable_divisor/1 | 0.98476 | 0.98374 | -0.10% |
| jit-O2/varying_condition/1000000 | 1.50366 | 1.50036 | -0.22% |

## Whole-VOID representative results

The corrected full G36 optimized suite object **and linked executable are byte-identical**. Normal AOT suite, Runtime/Core archives, exact-object numeric relinks and memory-pressure executables are identical as well. Consequently timing movements on those images are retained as unchanged-workload controls, not claimed instruction-level improvements. The original coordinated numeric/array regressions are resolved at the emitted-code boundary; identical code does not establish that historical G34/G35/GC-tail problems are solved.

Milliseconds per measured region. All unfavorable values are retained here and in full raw data. Remaining normal AOT and JIT modes, additional sizes and 26 escape patterns are in `G38-results.tsv`/JSON with ranges and per-block comparisons.

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| g31/aot/array/1024 | 1.04495 | 1.07142 | +2.53% |
| g31/aot/array/4096 | 4.15155 | 4.22555 | +1.78% |
| g31/aot/dependent/1000000 | 24.5058 | 24.4604 | -0.19% |
| g31/aot/dict/1024 | 4.23789 | 4.22609 | -0.28% |
| g31/aot/float/1000000 | 24.2725 | 24.3392 | +0.27% |
| g31/aot/integer/1000000 | 23.7915 | 23.7731 | -0.08% |
| g31/aot/integer/4000000 | 95.1016 | 95.0696 | -0.03% |
| g31/aot/io/200 | 6.77911 | 6.81988 | +0.60% |
| g31/aot/json/128 | 36.0214 | 35.3962 | -1.74% |
| g31/aot/json/32 | 5.73283 | 5.68888 | -0.77% |
| g31/aot/sort/1024 | 26.8739 | 27.2337 | +1.34% |
| g31/aot/sort/256 | 5.32698 | 5.35334 | +0.49% |
| g31/aot/text/1024 | 51.0229 | 50.4776 | -1.07% |
| g31/aot/text/256 | 12.6425 | 12.6792 | +0.29% |
| managed/aot/calls | 0.665871 | 0.663892 | -0.30% |
| managed/aot/managed-escape | 8.18371 | 8.16615 | -0.21% |
| managed/aot/managed-local | 0.509238 | 0.509942 | +0.14% |
| managed/aot/managed-survives-allocation | 42.3674 | 42.6141 | +0.58% |
| managed/jit-O2/async-tasks | 1.64279 | 1.6853 | +2.59% |
| managed/jit-O2/calls | 0.827766 | 0.802175 | -3.09% |
| managed/jit-O2/closures | 1.42377 | 1.36206 | -4.33% |
| managed/jit-O2/managed-across-edge | 1.09565 | 1.07139 | -2.21% |
| managed/jit-O2/managed-escape | 5.39564 | 5.74514 | +6.48% |
| managed/jit-O2/managed-local | 0.641032 | 0.627371 | -2.13% |
| managed/jit-O2/managed-reference | 0.648956 | 0.631037 | -2.76% |
| managed/jit-O2/managed-survives-allocation | 38.0741 | 38.1962 | +0.32% |
| managed/jit-O2/workers | 0.722227 | 0.673196 | -6.79% |
| managed/jit-normal/async-tasks | 1.62865 | 1.68047 | +3.18% |
| managed/jit-normal/calls | 0.828736 | 0.80471 | -2.90% |
| managed/jit-normal/closures | 1.59545 | 1.3592 | -14.81% |
| managed/jit-normal/managed-across-edge | 1.12372 | 1.08173 | -3.74% |
| managed/jit-normal/managed-escape | 5.52601 | 5.88532 | +6.50% |
| managed/jit-normal/managed-local | 0.643957 | 0.618562 | -3.94% |
| managed/jit-normal/managed-reference | 0.645647 | 0.623741 | -3.39% |
| managed/jit-normal/managed-survives-allocation | 38.2969 | 38.5933 | +0.77% |
| managed/jit-normal/workers | 0.704366 | 0.674782 | -4.20% |

Whole-suite remaining mode deltas (same six balanced blocks):

| Workload | Normal AOT | Normal JIT | O2 JIT |
| --- | ---: | ---: | ---: |
| array/1024 | -1.96% | +0.48% | +1.05% |
| array/4096 | -1.98% | +0.72% | +1.78% |
| dependent/1000000 | -0.01% | -7.89% | -3.53% |
| dict/1024 | -0.98% | -0.55% | -0.25% |
| float/1000000 | -0.14% | -9.66% | -8.08% |
| integer/1000000 | -0.05% | -8.07% | -7.97% |
| integer/4000000 | -0.05% | -7.64% | -7.81% |
| io/200 | -0.90% | -0.06% | -1.52% |
| json/128 | +0.32% | +0.85% | -0.30% |
| json/32 | -1.43% | -1.13% | +3.19% |
| sort/1024 | -0.32% | +2.19% | +2.96% |
| sort/256 | +0.61% | +0.73% | +0.39% |
| text/1024 | +0.08% | -1.46% | +1.61% |
| text/256 | -0.02% | -1.53% | +1.92% |

Exact-object numeric controls (milliseconds):

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| dependent | 26.6217 | 26.6139 | -0.03% |
| float | 26.6598 | 26.6171 | -0.16% |
| integer | 26.2155 | 26.1527 | -0.24% |

Prepared native boundary and Unicode controls (native ns/op; Unicode ns/region as recorded):

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| boundary/native/dictionary_count_existing_token/128 | 683.489 | 676.018 | -1.09% |
| boundary/native/dictionary_count_existing_token/3 | 684.059 | 676.881 | -1.05% |
| boundary/native/dictionary_existing_token/128 | 764.167 | 762.811 | -0.18% |
| boundary/native/dictionary_existing_token/3 | 763.865 | 763.358 | -0.07% |
| boundary/native/dictionary_source_abi/128 | 882.934 | 886.722 | +0.43% |
| boundary/native/dictionary_source_abi/3 | 816.84 | 822.747 | +0.72% |
| boundary/native/fixed_existing_token_layout/128 | 78.0709 | 78.7023 | +0.81% |
| boundary/native/fixed_existing_token_layout/3 | 78.0056 | 79.1164 | +1.42% |
| boundary/native/fixed_source_abi/128 | 319.474 | 320.603 | +0.35% |
| boundary/native/fixed_source_abi/3 | 254.845 | 256.756 | +0.75% |
| boundary/native/list_existing_token/128 | 250.431 | 251.06 | +0.25% |
| boundary/native/list_existing_token/3 | 251.084 | 252.178 | +0.44% |
| boundary/native/list_source_abi/128 | 346.984 | 342.07 | -1.42% |
| boundary/native/list_source_abi/3 | 282.165 | 283.654 | +0.53% |
| boundary/native/native_entry_metadata/128 | 160.191 | 160.733 | +0.34% |
| boundary/native/native_entry_metadata/3 | 159.512 | 160.409 | +0.56% |
| boundary/native/native_key_metadata/128 | 170.71 | 173.636 | +1.71% |
| boundary/native/native_key_metadata/3 | 172.386 | 175.892 | +2.03% |
| boundary/native/raw_scalar_reference_only/128 | 0.491333 | 0.491943 | +0.12% |
| boundary/native/raw_scalar_reference_only/3 | 0.491333 | 0.491333 | +0.00% |
| boundary/native/root_admission/128 | 84.4786 | 87.4889 | +3.56% |
| boundary/native/root_admission/3 | 31.5629 | 28.4334 | -9.92% |
| boundary/native/source_prepared_array_4096/0 | 1.46765e+06 | 1.49901e+06 | +2.14% |
| boundary/native/source_prepared_dictionary_1024/0 | 2.49138e+06 | 2.50403e+06 | +0.51% |
| unicode/aot-normal/unicode-count | 1.50409e+07 | 1.50662e+07 | +0.17% |
| unicode/aot-normal/unicode-prefix | 1.32588e+07 | 1.32217e+07 | -0.28% |
| unicode/aot-optimized/unicode-count | 1.49579e+07 | 1.49443e+07 | -0.09% |
| unicode/aot-optimized/unicode-prefix | 1.32489e+07 | 1.32268e+07 | -0.17% |
| unicode/jit-O2/unicode-count | 1.492e+07 | 1.51911e+07 | +1.82% |
| unicode/jit-O2/unicode-prefix | 1.34841e+07 | 1.37456e+07 | +1.94% |
| unicode/jit-normal/unicode-count | 1.49761e+07 | 1.51649e+07 | +1.26% |
| unicode/jit-normal/unicode-prefix | 1.34173e+07 | 1.37416e+07 | +2.42% |

Go1.27.4 is an explicitly historical accepted G36 comparator, reused from the verified G36 archive with source-entry SHA-256. It was not remeasured and is not mixed into this matched cohort. The accepted G31/G36 reports describe semantic differences; these SIMD kernels do not acquire a new fabricated Go equivalence. No additional Go SDK was installed.

## Allocation, roots, GC, workers and memory

G38 changes no Runtime or headers. Exact operation-count assertions compare native allocations/requested bytes/live/peak retention, managed construction, collections, roots, descriptor/layout work, frame entries and dictionary/string policy counts. G36's existing zero redundant layout/policy-node work and G35 allocation-elimination eligibility remain unchanged. The 26 managed positive/negative patterns retain their expected construction/root outcomes. Actual application GC polls and worker checkpoints remain 4,000,005 each for integer/1M and 4,000,003 each for dependent/float. Required checks are not removed to enable SIMD.

Memory-pressure region milliseconds:

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| large | 3.76892 | 3.77667 | +0.21% |
| mixed | 3.41182 | 3.37886 | -0.97% |
| retained | 6.12975 | 6.14502 | +0.25% |
| small | 2.80997 | 2.80897 | -0.04% |
| threads | 14.2116 | 15.5971 | +9.75% |

Snapshots at the end of each measured region (medians; bytes). The native harness then requires a final collection with zero live objects. Thread snapshots can differ with scheduling; complete per-run values and checksums are retained.

| Shape | Variant | Live objects | Live payload B | Cached payload B |
| --- | --- | ---: | ---: | ---: |
| large | base | 2 | 131072 | 0 |
| large | candidate | 2 | 131072 | 0 |
| mixed | base | 88 | 23056 | 15008 |
| mixed | candidate | 88 | 23056 | 15008 |
| retained | base | 544 | 8704 | 3584 |
| retained | candidate | 544 | 8704 | 3584 |
| small | base | 33 | 528 | 3584 |
| small | candidate | 33 | 528 | 3584 |
| threads | base | 245.5 | 3928 | 184 |
| threads | candidate | 242 | 3872 | 256 |

Memory-pressure process peak RSS (KiB):

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| native/native | 14186 | 14102 | -0.59% |

| Workload | Variant | Stopped median us | p95 us | p99 us | Maximum us | Samples |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| text-1024 | base | 823.231 | 1199.423 | 1306.064 | 1459.932 | 660 |
| text-1024 | candidate | 860.380 | 1279.134 | 1386.352 | 1904.149 | 660 |
| json-128 | base | 797.841 | 1082.204 | 1223.303 | 1269.613 | 288 |
| json-128 | candidate | 837.031 | 1068.774 | 1172.303 | 1215.023 | 288 |

These are full stopped-world samples, not a mutator-only proxy; end-to-end latency samples, pressure live/cached bytes, epochs, checksums and RSS are retained. P99/max variability is reported on the unchanged collector and linked pressure controls. There is no new GC-tail optimization claim. Historical GC-tail, worker, AOT-array and numeric costs stay active in the ledger.

## Compilation and startup cost

Milliseconds, process wall time including the fixed namespace-launch overhead; code-emission comparisons replay exact object identities. Full-corpus compile/link and cold starts are separate from warmed execution. The new pass has a real small-kernel compilation/code-size cost that is part of the acceptance tradeoff.

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| normal | 52.1096 | 52.4107 | +0.58% |
| optimized | 68.2104 | 80.3545 | +17.80% |

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| cold/aot | 9.14259 | 8.80713 | -3.67% |
| cold/aot-normal | 8.89893 | 9.00051 | +1.14% |
| cold/jit-O2 | 3772.65 | 3794.6 | +0.58% |
| cold/jit-normal | 3149.45 | 3130.13 | -0.61% |
| compile/compile-link | 3662.98 | 3645.95 | -0.46% |

| Case | G36 median | G38 median | Change |
| --- | ---: | ---: | ---: |
| independent-AOT-process | 8.73809 | 8.65802 | -0.92% |

## Residual native/JIT investigation and acceptance hold

The main cohort measured managed-escape JIT about +6.5%, threaded pressure +9.75%, text stopped p99 1306.064→1386.352us and maximum 1459.932→1904.149us. All are retained. A separate six-balanced-block rerun paused only task-owned strict-build descendants and the Windows build VM for about20seconds, then resumed them; no qualification test was paused. Threaded pressure changed sign to −4.81% on identical executable images, demonstrating substantial same-image scheduling variability. Managed cases remained slower across actual compiler hosts. Driver object files, Runtime/Core archives and captured managed-local/escape/calls/extra normal/O2 JIT IR are identical; compiler-host link context and JIT placement remain different. Identical candidate-host label controls also vary, including −5.01% on O2 managed escape. Selected native Runtime symbols retain their sizes but shift3168bytes in the compiler host, changing their cache-line offsets. This is a concrete link-context difference, not proof of a particular instruction-cache/ASLR/allocator cause. No alignment, padding or link-order workaround was introduced.

| Cohort | Mode | Case | G36-label median ms | G38-label median ms | Change |
| --- | --- | --- | ---: | ---: | ---: |
| actual-host | jit-normal | managed-escape | 5.41613 | 5.8855 | +8.67% |
| actual-host | jit-normal | context_control | 5.86666 | 6.13273 | +4.54% |
| actual-host | jit-normal | public_helper | 5.29706 | 5.58442 | +5.42% |
| actual-host | jit-O2 | managed-escape | 5.37742 | 5.59224 | +3.99% |
| actual-host | jit-O2 | context_control | 5.54517 | 6.05723 | +9.23% |
| actual-host | jit-O2 | public_helper | 5.35296 | 5.57855 | +4.21% |
| identical-host | jit-normal | managed-escape | 5.64989 | 5.73709 | +1.54% |
| identical-host | jit-normal | context_control | 6.26245 | 6.21937 | -0.69% |
| identical-host | jit-normal | public_helper | 5.64731 | 5.72184 | +1.32% |
| identical-host | jit-O2 | managed-escape | 5.73566 | 5.44821 | -5.01% |
| identical-host | jit-O2 | context_control | 6.05166 | 5.85289 | -3.28% |
| identical-host | jit-O2 | public_helper | 5.67795 | 5.58859 | -1.57% |

The final independent cohort executes both actual host images through one constant pathname/argv, removing variant executable-path length as a confound. Image bytes and code alignment are untouched. It runs after all qualification, with the idle VM paused and no Linux build active; results supplement every earlier cohort.

| Mode | Case | G36 median ms | G38 median ms | Change |
| --- | --- | ---: | ---: | ---: |
| jit-normal | managed-escape | 5.51776 | 5.70242 | +3.35% |
| jit-normal | context_control | 5.86533 | 6.22373 | +6.11% |
| jit-normal | public_helper | 5.3735 | 5.85355 | +8.93% |
| jit-O2 | managed-escape | 5.23891 | 5.73725 | +9.51% |
| jit-O2 | context_control | 5.58439 | 6.08487 | +8.96% |
| jit-O2 | public_helper | 5.44877 | 5.57608 | +2.34% |

The same constant-path cohort also runs the complete byte-identical AOT suite. Unchanged-image deltas are retained below, without attributing them to the optimization.

| Workload | G36 median ms | G38 median ms | Change |
| --- | ---: | ---: | ---: |
| array/1024 | 1.04289 | 1.03082 | -1.16% |
| array/4096 | 4.10949 | 4.11492 | +0.13% |
| dependent/1000000 | 24.3794 | 24.4222 | +0.18% |
| dict/1024 | 4.15857 | 4.13496 | -0.57% |
| float/1000000 | 24.217 | 24.1637 | -0.22% |
| integer/1000000 | 23.6214 | 23.5963 | -0.11% |
| integer/4000000 | 94.392 | 94.3194 | -0.08% |
| io/200 | 6.89836 | 6.83692 | -0.89% |
| json/32 | 5.62073 | 5.67756 | +1.01% |
| json/128 | 35.2966 | 35.207 | -0.25% |
| sort/256 | 5.24672 | 5.2159 | -0.59% |
| sort/1024 | 26.5282 | 26.3954 | -0.50% |
| text/256 | 12.5055 | 12.4706 | -0.28% |
| text/1024 | 50.5868 | 50.3739 | -0.42% |

Actual-host reruns include managed-escape normal +8.67% / O2 +3.99%, context-control O2 +9.23% and public-helper normal +5.42%. The AOT optimization is disabled on those JIT paths, yet native deployment performance is part of acceptance. **Keep the bounded candidate separate from accepted G36 until the persistent host-context costs have an adequate explanation or a separately qualified correction.** Do not broaden eligibility or erase the profitable SIMD capability because acceptance is held. Broad coordinated versioning remains rejected independently.

## Rejected broad candidate: native regression evidence

The first pass registration covered all optimized AOT modules. It passed Linux Release77 and Windows Release63 and accelerated the two pure kernels, but six balanced blocks regressed coordinated AOT integer/1M +27.76%, integer/4M +25.16%, arrays/1024 +6.62%, arrays/4096 +5.63%, dependent +12.76% and float +8.97%. Its exact source, patch, all raw timings, GC tails and failures remain under `first-candidate/`.

The integer leaf was byte/relocation-identical at300bytes, dependent grew188→311bytes and float277→396bytes. GC/worker counts remained exact; exact-object relink controls were byte-identical and flat. Unwanted coordinated loop versioning and code/layout exposure had no SIMD benefit. A specific microarchitectural cause for the unchanged integer leaf was not established. No padding, alignment workaround, invented state or favorable link-order manipulation was used. The correction reuses the existing module coordination fact and restores the complete coordinated suite image, while retaining the identical proven pure-loop object. This is a qualified scope boundary, not a benchmark-shape whitelist.

## Remaining objectives and handoff

G37/G37-F source-constant checked operations and native-boundary costs remain active but unaccepted in the original exact nine-file worktree and verified archive. Broader escape analysis beyond G35; checked array/descriptor specialization; dictionary/sorting overhead; text/Unicode/JSON materialization; GC-tail/worker/numeric regressions; unguarded bidirectional and managed-array SIMD remain open. This bounded result does not close those capabilities.

The maintained source change is one existing LLVM pass registration plus two explicit AOT policy arguments; Runtime/public headers are unchanged. Tests add ordinary VOID fixtures and a single independent AOT oracle driver. Exact source/full diffs, identities, original/candidate Git inventory (including G32/G33 research reports), replay instructions, all raw results, first-trial evidence and key IR/disassembly are selected for a DEFLATE-9/SHA-256 archive. Build trees, caches, routine intermediates, binaries and toolchains are excluded. Archive CRC, extraction SHA-256, live source rehash and accepted-preimage patch replay must pass before guarded task cleanup. The isolated candidate source worktree remains reviewable; the original G37 worktree and all active SDK/provider installations stay intact. Adjacent verification/cleanup receipts provide final retained count, ZIP bytes, reclaimed space and final protection checks.

Backend postimage SHA-256: `8cd1793506cb0e5ad562604dbc35a96537fdebe8bfba353325348fc9e16f546f`. Accepted Runtime archive SHA-256: `549b90dc5c56a371d5b9eeffafc18366fdc31d3518019c8fa4bb656cb9fd8754`. Source/full patch hashes and exact file inventory are in `final-git-inventory.json`; the index is empty.

## Verified archive and final cleanup

Retained **533 selected files plus one manifest**, DEFLATE-9 ZIP **6,207,635 bytes**. SHA-256: `9382e15d95d79fc88d62ea28d2e8109929378629ef497d57352405a47cce037e`. CRC, extracted SHA-256, live source rehash and independent accepted-preimage replay of both final and rejected source/test patches pass. [Evidence archive](/home/shmellyorc/Documents/Void.Language-archives/g38-automatic-loop-optimization-2026-11-15.zip), [verification](/home/shmellyorc/Documents/Void.Language-archives/g38-automatic-loop-optimization-2026-11-15-verification.json), [exact compiler/test diff](/home/shmellyorc/Documents/Void.Language-archives/g38-automatic-loop-optimization-2026-11-15.patch), [final full diff](/home/shmellyorc/Documents/Void.Language-archives/g38-automatic-loop-optimization-2026-11-15-final-full.patch), [final Git inventory](/home/shmellyorc/Documents/Void.Language-archives/g38-automatic-loop-optimization-2026-11-15-final-git-inventory.json), [all primary results](/home/shmellyorc/Documents/Void.Language-archives/g38-automatic-loop-optimization-2026-11-15-G38-results.tsv), [cleanup receipt](/home/shmellyorc/Documents/Void.Language-archives/g38-automatic-loop-optimization-2026-11-15-cleanup.json).

Observed reclamation across unique phase retirements and final cleanup: **10,770,558,976 host-filesystem bytes (10.03 GiB)** and separately **11,238,830,080 Windows guest-filesystem bytes (10.47 GiB)**. Guest free space is not a claim that the host VM disk image shrank. Routine build groups were retired after qualification/identity/recipe capture; selected task evidence remained until ZIP verification. Final Linux scratch and both task transports are absent; both Windows private roots are removed, with their known tools junctions detached. The isolated source worktree remains. Active SDKs/providers, 29 previous ZIPs and the original exact nine-file G37-F worktree are verified preserved. Both indexes remain empty.

The immutable ZIP contains the pre-cleanup report/ledger and full-diff snapshot; this storage colophon and final full-diff/inventory are adjacent post-cleanup metadata. Default performance acceptance remains **HELD**, including after the constant-path cohort (managed-escape O2 +9.51%, context-control O2 +8.96%, public-helper normal +8.93%). All correctness gates pass; neither source eligibility nor binary alignment was changed to hide these costs.
