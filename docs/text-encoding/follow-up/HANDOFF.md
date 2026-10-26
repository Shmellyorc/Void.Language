# Sprint 3 crash-reliability follow-up — Golden Wrench handoff

26 October 2026. Authorized baseline `108d7639f1dda45df201ea950d9abd751d024ff7`.
Focused follow-up complete. Stop for Golden Wrench review. No production repair
was justified or made; compiler, Runtime, GC, LLVM integration and stdlib are unchanged.

## Crash classification and repair audit

| Evidence | Classification | Result |
| --- | --- | --- |
| Debug semantic-depth SIGSEGV, `incident-splghldg` | Confirmed and repaired | Original source/binary/debugger evidence proves stack exhaustion. Cold helpers genuinely reduce stack usage; original guards, resolver order and diagnostics remain intact. No rewrite needed. |
| Historical Release filesystem SIGSEGV | Unresolved | Original temporary source and executable hash are unavailable. Current-candidate passes do not establish a cause or repair. |
| Historical LLVM 19.1.7 JIT DSE observation | Unresolved | Retained stack establishes an optimization-phase fault, without proving the faulty invariant or a repair. |
| Historical pinned Clang 19.1.7 BDCE/DemandedBits observation | Unresolved | O2/sanitized compilation of conversion.cpp failed; an unchanged retry passed. Preserved preprocessed-source hashes verify. Retry success is not a repair. |
| Repeated Python self-signal preservation artifacts | Intentional test | All 33 historical receipts were independently verified as synthetic before cleanup. No real compiler/runner incident was removed. |

The [full classification and source references](verification/crash-classification.md)
also distinguish the retained LLD GVN trace with overall exit zero and the separate
in-place source-rewrite engineering failure. A common signal number does not connect
these observations. No historical large workload or optimizer experiment was repeated.

The confirmed Debug repair remains exactly as committed. Fresh disassembly confirms
`anchor` frame subtraction 3,840→3,584 bytes and `call_expression` 19,904→16,224 bytes.
The preserved/current compiler diff contains only the cold-helper extractions. The
256-level semantic guard, pure-expression check and global→local→constant order
remain unchanged. Full semantic-depth tests passed all 34 cases/profile with actual
1 MiB compiler stacks, including legal chains and safe excessive-depth rejection.

## Exact changes

- `tests/text_encoding/crash_test.py` now puts every synthetic incident inside a
  TemporaryDirectory, verifies evidence before cleanup, then checks disposal and
  unchanged repository artifact inventory. It covers checked failures, direct Popen
  editor input, unique incidents and colliding non-main filenames.
- `tests/io/execution.py` invokes the shared preservation helper directly. Every
  compiler, runner, JIT and AOT call explicitly supplies its source and generator,
  including linked executables with no `.void` argument. Assertions and fixture
  overwrites/temporary cleanup happen after preservation returns.
- `tests/text_encoding/crash.py` records original/preserved source hashes separately,
  retains hashed readonly generator copies with collision-safe names, and accepts
  explicit generator provenance. Relative executable paths resolve against the
  subprocess cwd. Receipt wording includes explicit source inputs rather than
  assuming every command names a source.
- New `tests/io/preservation.py` proves compiler-check, async-runner and source-free
  AOT abnormal exits retain arbitrary-named input, companion, generator, exact
  command/cwd/status, executable hash and binary stdout/stderr. Ordinary exit-one
  diagnostics do not create incidents. All synthetic output is disposable.
- `CMakeLists.txt` registers `io_crash_preservation` in the existing test system.

There is one shared preservation implementation. Real incidents still default to
persistent `docs/text-encoding/evidence/crashes/`; unique directories and readonly
source copies remain. No ignore rule hides future genuine evidence.

The relative-executable omission was demonstrated by a controlled disposable signal
before repair: [before receipt](relative-executable-before.json). The permanent
self-test now uses a relative interpreter path where drives permit and checks its
exact executable hash. No production crash was invented to justify this harness fix.

[In-memory generator comparison](generator-comparison.json) proves sync, async and
error-async generated text matches the authorized committed baseline exactly. No
comparison `.void` was written. This does **not** recover the lost historical input.

## Cleanup and original versus corrected behavior

Previously each self-test wrote another permanent group under
`evidence/synthetic-preservation/`, duplicating canonical stdlib and harness sources.
Eleven test executions had retained 33 intentional incidents. After verifying every
receipt's Python self-signal command, synthetic input marker/hash, and ownership of
all files in that subtree, this follow-up removed **940 tracked working-tree files**
(7,504,348 bytes; 221 directories including the root). The deleted files comprise
440 `.void`, 390 `.py`, 77 `.bin` and 33 `.json` files.

[Compact cleanup evidence](synthetic-cleanup.json) keeps counts, receipt hashes and
classification. The working-tree deletions are unstaged; Boss controls Git. No copy
or archive of the redundant snapshot tree was retained. Synthetic tests now verify
all evidence inside disposable storage and leave concise logs only. The permanent
synthetic subtree remained absent after every completed verification profile.
Historical reports referring to that former subtree remain unchanged as records;
this cleanup receipt explains its intentional removal.

## Focused verification

| Build | Compiler/LSP | IO and preservation | Final preservation branch checks |
| --- | ---: | ---: | ---: |
| Debug | 7/7 passed | 4/4 passed | 2/2 passed |
| Release | 7/7 passed | 4/4 passed | 2/2 passed |
| Strict ASan/UBSan/LeakSanitizer | 6/6 passed | 4/4 passed | 2/2 passed |

These are actual registered CTest executions, with exact commands, environments,
logs and JUnit linked from [RECEIPT.md](RECEIPT.md). Compiler/LSP tests cover full
semantic depth, repeated constants/globals and malformed/unsaved source recovery.
Native const recovery passed 472 assertions normally and with actual 1 MiB server
stacks. Strict registration includes actual 1 MiB compiler depth, without claiming
an unregistered instrumented LSP stack variant ran.

Each IO profile ran one sync/JIT/O2/linked-AOT regression and one async/JIT/O2
regression, with 42 numeric vectors and both byte orders. No worker concurrency or
GC setting was changed. Final relative-executable/receipt corrections affect the
failure-preservation branch; only the two disposable preservation tests were
repeated afterward. Large IO workloads were not repeated for that branch-only fix.

## Integrity and remaining limits

[Completeness evidence](completeness.json) verifies all **2,937** original evidence
and protected-file hashes, all 13 readonly genuine source snapshots, all 14 genuine
provenance hashes, and the original private executable hash. Original crash receipts,
research documents and actual incident `incident-splghldg` are untouched. No new
genuine crash occurred. Root README and every existing stdlib source, including
Boss's time formatting, match the authorized baseline.

Historical filesystem, JIT DSE and Clang BDCE causes remain unresolved; the retained
LLD trace does not prove an abnormal exit. Native Windows/macOS execution remains
for later owner-controlled acceptance. All execution here was focused local Linux
verification. No speculative compiler/GC/LLVM/async redesign, hosted CI, staging,
commits, branches or pushes occurred. Stop for independent Golden Wrench review.

## Final correction after accepted push cc8d476

[The final fatal-exit correction](fatal-exits/RECEIPT.md) is complete. Shared
preservation now recognizes reserved ASan exit 87 and UBSan exit 86, while keeping
ordinary positive diagnostics excluded. The Windows self-test uses ExitProcess,
and signed Windows status simulation verifies receipt creation without Unix-only
signal conversion failures. Focused preservation tests passed 2/2 in all three
available profiles; original evidence is untouched. No production changes or full
IO/compiler matrix were needed. Native Windows execution remains pending. Stop for
Golden Wrench review.
