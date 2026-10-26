# Local accumulation measurements

Linux x86-64, pinned LLVM/Clang 19.1.7. Three samples per size/mode; reported median
milliseconds. The source workload performs repeat, split (one field per unit), join,
replace, hex encoding and Base64 encoding, checking independent size/content
invariants. It measures invocation work after compilation; global/runtime first-call
cost can affect the first sample. Runs shared the host with acceptance work and
are observations, not portable latency promises or an isolated benchmark lab.
These final measurements use the Boss-confirmed plain encode API; earlier result-
returning API measurements are retained in pre-api-acceptance/. They are not a
controlled before/after comparison.

| Input UTF-8 bytes | Release normal | Release O2 | Debug normal | Debug O2 |
| --- | ---: | ---: | ---: | ---: |
| 1,024 | 53.104 | 49.857 | 449.556 | 504.945 |
| 4,096 | 210.169 | 257.106 | 2,440.110 | 1,911.210 |
| 16,384 | 972.939 | 954.110 | 9,133.640 | 9,861.320 |
| 65,536 | 5,396.330 | 6,687.960 | 47,301.400 | 45,408.300 |

[Release samples](performance-release.csv), [Debug samples](performance-debug.csv).
The workload deliberately creates thousands of independent split-field strings.
Managed allocation, precise roots, collection, checked list operations and copies
at the existing string/byte conversion boundary contribute to measured cost.
A fourfold input increase is sometimes more than fourfold elapsed time. No strict
linear end-to-end scaling or performance victory is claimed.

Algorithm review: KMP prefix/search are linear in input plus needle size; repeated
nonoverlapping searches advance rather than rescan earlier input. Split constructs
each selected field once. Join preflights aggregate size, then appends each byte
once to geometrically grown source lists. Replacement advances one cursor and
appends unmatched/replacement chunks; repeat and codecs use the same geometric
list growth. None repeatedly concatenates an accumulated immutable string or
copies the entire accumulated byte buffer for each append. Existing collector
and per-element Runtime overhead remain practical costs; no speculative GC or
buffer framework change was made to improve these numbers.

A separate 256 KiB source fixture checks text, 512 KiB hex, Base64, independent
owned decoding and bounds through JIT/O2 and linked AOT in each build. Its exact
commands/times are in each profile's text_encoding_large log. There was no prior
text/codec implementation to benchmark against; these are scaling measurements,
not a before/after speedup claim.

Exact benchmark commands, run from the repository root:

```sh
python3 tests/text_encoding/process.py build/ci-official-debug/void_text_performance tests/text_encoding/performance.void
python3 tests/text_encoding/process.py build/ci-official-release/void_text_performance tests/text_encoding/performance.void
```

Both commands exited zero; stderr receipts are empty. The runner compiles each
mode once, invokes each size three times and validates workload invariants on every
sample. These CSVs record medians, not all individual samples. Executable hashes
are in final-builds.json and source hashes in source-freeze.json.
