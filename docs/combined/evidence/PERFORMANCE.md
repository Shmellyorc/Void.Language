# Memory-write measurements

Actual local Linux x86-64 linked Release/O2 executables, pinned LLVM/Clang/LLD
19.1.7. Three fresh process wall-clock samples per row; median seconds below.
Compilation is excluded. Timings include process startup, stream construction,
and (for sequential overwrites) construction of the existing zero-filled data.
Each write uses the same one-byte input; final position and length are checked.
Append starts empty; overwrite starts with exactly count bytes and writes from zero.

| Operation | Writes | Before seconds | After seconds | Speedup |
| --- | ---: | ---: | ---: | ---: |
| append | 20,000 | 0.292263 | 0.056350 | 5.19× |
| append | 40,000 | 0.713790 | 0.097206 | 7.34× |
| append | 80,000 | 2.306722 | 0.193547 | 11.92× |
| overwrite | 20,000 | 0.268250 | 0.062432 | 4.30× |
| overwrite | 40,000 | 0.616756 | 0.120597 | 5.11× |
| overwrite | 80,000 | 1.838239 | 0.235401 | 7.81× |

[Before raw samples](memory-before-large.json), [after raw samples](memory-after.json),
[after execution log](memory-after.log), [original source identity](baseline-source.json).
The additional [small baseline](memory-before.json) records the initial
1,000/2,000/4,000-write probe before choosing the more discriminating larger sizes.

Reproduce the same generated workload with:

```sh
python3 tests/memory/benchmark.py /absolute/path/to/Release/voidc /absolute/results.json
```

Both measurements use the same byte workload and normal optimized linked AOT.
The baseline script used stable /tmp filenames; the checked-in reusable script
uses an isolated temporary directory with the same source/work and timing method.
No clock or allocator instrumentation is added to the language. Measurements ran
under ordinary local host load (other engineering processes were active during
the after measurement); this is a local observation, not a statistical or native
cross-platform performance guarantee. In both after series, doubling the larger
workload takes approximately twice the time. The amortized linear copy guarantee
also follows directly from checked geometric capacity growth; correctness and
allocation-failure recovery have separate focused receipts.
