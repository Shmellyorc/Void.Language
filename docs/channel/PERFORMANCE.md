# Channel performance audit

Linux Release, pinned LLVM 19.1.7. Three serialized execution samples per workload; no cache. Setup and warmup precede the timed invocation. Compile wall/CPU, sample wall/CPU, process high-water RSS, exact checksum, emission and handler execution counts, and generated-source hashes are in the machine-readable receipt. RSS includes analysis/JIT memory; it is not isolated dispatch allocation.

| Workload | Engine | Compile wall ms | Execution min/median/max ms | CPU median ms | Peak RSS KiB | Emissions/sample | Handler calls/sample | Checksum |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| dispatch-0-topics-1 | event | 44.764 | 4.835/4.911/4.914 | 4.847 | 47588 | 1500 | 0 | 0 |
| dispatch-0-topics-1 | channel | 180.606 | 0.852/0.854/0.856 | 0.850 | 72760 | 1500 | 0 | 0 |
| dispatch-0-topics-8 | event | 83.443 | 4.999/5.105/5.161 | 5.085 | 72892 | 1500 | 0 | 0 |
| dispatch-0-topics-8 | channel | 181.527 | 0.873/0.882/0.886 | 0.873 | 72904 | 1500 | 0 | 0 |
| dispatch-1-topics-1 | event | 55.792 | 9.796/9.837/9.975 | 9.796 | 72944 | 1500 | 1500 | 1500 |
| dispatch-1-topics-1 | channel | 190.453 | 16.707/16.783/16.979 | 16.687 | 72952 | 1500 | 1500 | 1500 |
| dispatch-1-topics-8 | event | 135.643 | 10.167/10.204/10.825 | 10.145 | 72952 | 1500 | 1500 | 1500 |
| dispatch-1-topics-8 | channel | 238.396 | 17.093/17.399/17.537 | 17.328 | 72952 | 1500 | 1500 | 1500 |
| dispatch-16-topics-1 | event | 54.432 | 43.339/43.455/43.475 | 43.011 | 72964 | 1500 | 24000 | 24000 |
| dispatch-16-topics-1 | channel | 192.472 | 60.959/61.245/61.377 | 60.979 | 72968 | 1500 | 24000 | 24000 |
| dispatch-16-topics-8 | event | 139.344 | 44.426/44.951/45.107 | 44.746 | 72968 | 1500 | 24000 | 24000 |
| dispatch-16-topics-8 | channel | 239.687 | 62.555/62.914/63.064 | 62.440 | 72976 | 1500 | 24000 | 24000 |
| dispatch-128-topics-1 | event | 58.623 | 57.811/57.994/58.268 | 57.762 | 72984 | 300 | 38400 | 38400 |
| dispatch-128-topics-1 | channel | 192.212 | 74.850/75.525/76.270 | 75.105 | 72984 | 300 | 38400 | 38400 |
| dispatch-128-topics-8 | event | 139.036 | 60.281/60.666/61.077 | 60.426 | 72984 | 300 | 38400 | 38400 |
| dispatch-128-topics-8 | channel | 265.194 | 77.857/78.521/80.408 | 78.163 | 72984 | 300 | 38400 | 38400 |
| unknown | event | 143.158 | 5.252/5.256/5.316 | 5.231 | 72988 | 1500 | 0 | 0 |
| unknown | channel | 239.536 | 5.365/5.522/5.535 | 5.491 | 72992 | 1500 | 0 | 0 |
| duplicates | event | 54.139 | 43.454/43.645/44.227 | 43.274 | 72992 | 1500 | 24000 | 24000 |
| duplicates | channel | 195.021 | 59.672/59.706/59.755 | 59.412 | 73032 | 1500 | 24000 | 24000 |
| churn | event | 71.458 | 1.369/1.375/1.386 | 1.368 | 73032 | 0 | 0 | 0 |
| churn | channel | 196.926 | 10.725/10.741/10.880 | 10.686 | 73032 | 0 | 0 | 0 |
| clear-reuse | event | 61.080 | 3.251/3.261/3.331 | 3.236 | 73036 | 300 | 300 | 300 |
| clear-reuse | channel | 190.319 | 12.497/12.600/12.626 | 12.543 | 73040 | 300 | 300 | 300 |
| nested | event | 58.705 | 17.564/17.651/17.672 | 17.566 | 73048 | 2700 | 2700 | 10800 |
| nested | channel | 194.227 | 30.307/30.786/30.886 | 30.600 | 73060 | 2700 | 2700 | 10800 |

0/1/16/128 counts use identical named callbacks; repeated subscriptions remain duplicates. Eight-topic fixtures populate every topic, then emit only topic zero. Unknown routing is compared with a separate empty event. Clear/reuse replaces the direct event owner because events expose no clear operation. Both churn fixtures construct the iteration topic string; the router additionally maintains its required identity/index/pruning state. Nested payloads descend 8 through 0, giving 9 emissions and 36 accumulated payload units per root.

The original unrolled fixture is retained separately. Source loops reduce its 1024-registration compilation cost without changing the counts or checksums. Both raw runs remain in the engineering archive. No Runtime/event replacement or speculative universal performance target was introduced.

[Implementation handoff](HANDOFF.md).
