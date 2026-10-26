# File I/O and host paths — Golden Wrench handoff

26 October 2026, based on `084fe1ebf26e3f6376ed3e569e1b6c1a74499b17`.
Implementation and the local completeness audit are finished; stopped for Golden
Wrench review. This document is not an acceptance lock or permission to commit,
push or run hosted CI. The earlier unexplained Release fault remains open.

## Implemented scope

`stdlib/io.void` provides the managed error hierarchy, seven opening modes, shared
file/cursor identity, up-to/exact/line reads, bytes and strict UTF-8, all ten numeric
types through `encoding/binary`, selected endian u32 Pascal framing, checked seeks,
length/truncation, flush and one-shot close. Whole-file functions support owned
bounded reads, write and append. File and directory copy/move/removal, metadata,
symbolic links, filename-pattern lookup and recursive enumeration are included.
Every applicable blocking operation has a real native `_async` partner.

`stdlib/path.void` provides joining, normalization, filename/stem/extension/parent/
root extraction, extension replacement, qualification predicates and explicit-base
absolute/relative conversion. Runtime host paths retain their distinction from
compiler VFS/import paths. No `os` or application-data API was added.

The [contract](CONTRACT.md) and [API audit](API.md) define the full behavior.
[API.tsv](evidence/API.tsv) lists the 149 actual public declarations and source
locations. [API-AUDIT.tsv](evidence/API-AUDIT.tsv) maps every declaration to tests,
documentation and ownership behavior. The
[executable example](../../examples/io/filesystem.void) covers Unicode settings,
game save/load, binary assets and memory/file wire interoperability.

## Necessary compiler and Runtime support

* A narrow canonical-stdlib operation table, frontend lowering, independent typed
  IR checks and LLVM DataLayout-derived reply geometry. No public FFI, reflection
  or source-language feature was introduced.
* One existing native-resource ledger shared by files and prior native providers.
  File identity is a rooted managed wrapper; native handles and cursor data remain
  private native state. Close invalidates aliases and retires native identity once.
* Owned native jobs on the existing finite worker pool. Inputs are copied before
  waiting, workers run NativeSafe, and the owner publishes precise rooted results.
  The inherited task queue uses checked allocation, not a new arbitrary task cap;
  worker count remains the existing host limit of 1–64, default four.
* Source operation reservations prevent interleaving unrelated aliases. Nested
  canonical wire helpers share their reservation through their awaits. A racing
  source close reports `busy`; cancellation drains entered work before disposal.
* Acquisition rollback reuses the existing job record on a worker. A Runtime
  admission counter covers both entered work and unpublished acquisitions through
  commit/rollback, including whole-file jobs. Runtime shutdown rejects new native
  admissions and drains these intervals before retiring resources.
* A focused `with acquire()?` lowering repair registers/activates cleanup before
  creating the resource child group, so failed acquisition cannot bypass a join.

The [engineering record](IMPLEMENTATION.md) preserves the discovered defects,
repairs and intermediate receipts. [Source freeze](evidence/source-freeze.json)
records all 53 changed source/build/test hashes and the seven protected hashes.
Root README, console, binary, memory, convert, time and base-error sources match
the baseline. No Git mutation, remote action or hosted CI was performed.

## Local verification

The final run selects 63 tests in Debug and Release and 56 in the strict sanitizer
configuration. It includes I/O, installed/relocated stdlib execution, real JIT/O2
and linked AOT, allocation/GC sweeps, native failure/progress injection, actual
owner responsiveness, pending-close/cancellation/shutdown races, compiler-owned
LSP documentation and malformed-constant recovery, stack cases and the existing
codec/memory/console/convert/time/globals/async/native regressions.

Final consolidated results: **Debug 63/63 (1,020.90 s), Release 63/63 (688.54 s),
strict sanitizers 56/56 (866.81 s)**. There were no skipped, disabled or failing
cases in these final runs: **182/182 selected tests passed**. The
[concise receipt](evidence/RECEIPT.md) links full logs, summaries and JUnit results.

Strict runs use `detect_leaks=1`, `halt_on_error=1`, ASan exit 87 and UBSan exit 86,
outside the sandbox because its ptrace restriction prevents LeakSanitizer. No
sanitizer is disabled. The pinned LLVM SDK has RTTI off, so UBSan vptr checking is
unavailable under the existing matching ABI. New I/O small-stack tests run actual
compiler/JIT/AOT/LSP processes with a 1 MiB limit in Debug/Release; the existing
registered global-inference guard also runs in the sanitizer selection.

## Open findings and practical limits

An earlier Release async wire run exited with SIGSEGV. Two debugger attempts
passed, including the original fixture pathname with normal ASLR. No failing stack
was obtained, so the cause remains **unclassified and unresolved**. It is not
claimed to be the historical LLVM DSE failure, nor claimed repaired by the shutdown
fix. The final passing Release run is a retry after intervening documented repairs,
not a clean first attempt. Original failure and debugger receipts are retained in
`evidence/release-initial-io*` and `evidence/release-gdb-attempt-*`.

Native Windows/macOS execution remains **unverified**. Registered platform branches
cover Windows drive/UNC/ordinal rules and native noninheritability; POSIX branches
cover permissions, hard links, invalid native UTF-8 names and real FIFO short reads.
Windows symlink tests require Developer Mode or privileges. Cross-volume tests
need distinct volumes (`VOID_IO_SECOND_VOLUME` can select the second); this Linux
host used its distinct checkout and temporary filesystems successfully.

Directory traversal/mutation uses bounded path snapshots, not an atomic namespace
snapshot or a containment boundary against malicious concurrent ancestor changes.
Known links are not descended into. Native exclusive creation/rename and opened
inode checks protect collision/self-copy decisions; ordinary recursive operations
still require external synchronization when namespace isolation is needed.

Entered syscalls may not be interruptible. Cancellation does not undo committed
filesystem changes. Sync scope disposal can block; await `close_async()` first
when disposal must stay off the owner. GC does not invoke source destructors;
forgotten handles remain Runtime-owned until close/shutdown. Flush requests host
flushing without an unconditional hardware/power-loss guarantee.

Golden Wrench should review source, the public contract, lifecycle/ABI proof and
the unexplained Release fault before recommending Boss-controlled acceptance.
