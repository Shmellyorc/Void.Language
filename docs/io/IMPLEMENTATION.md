# File I/O and paths — engineering record

26 October 2026. Implemented and locally audited from
`084fe1ebf26e3f6376ed3e569e1b6c1a74499b17`; stopped for Golden Wrench review.
Final consolidated tests passed **182/182**: Debug 63/63, Release 63/63 and strict
sanitizers 56/56, with no skipped cases. This is not a final acceptance lock.
The [handoff](HANDOFF.md), [API audit](API.md), [contract](CONTRACT.md) and
[concise receipt](evidence/RECEIPT.md) define the delivered scope and evidence.
No hosted CI or Git mutation has been performed. One earlier unexplained Release
SIGSEGV remains an open reliability finding; native Windows/macOS execution is
unverified.

## Implemented dependencies

The existing Runtime native-resource ledger now admits statically typed file
resources alongside the existing fixture/sink providers. Native file identity,
shared cursor, operation reservations, close and shutdown use that one ledger.
Expected native shutdown-close errors use the existing Runtime close status;
all resources are still attempted before an unexpected provider exception is
re-raised. Failed global startup preserves its primary diagnostic and appends
native cleanup failure when present.

The existing bounded async worker queue accepts owned native jobs. Workers enter
NativeSafe state, perform native operations and never run a generated source
callback for file work. Owner completion publishes precise rooted results.
Queued cancellation releases admission counts; entered calls drain before
resource release. Acquisition rollback reuses its existing worker record without
allocating or running blocking disposal on the owner. The opening activation
cannot finish until rollback is quiescent. No source-worker sharing rule or
public task syntax was relaxed.

A necessary frontend repair creates an async resource's child group after
acquisition and cleanup registration. Previously, `with acquire()?` could emit an
Err edge bypassing a group join. The permanent acquisition fixture proves both
Err propagation and successful one-shot disposal in JIT/O2.

The shared native filesystem kernel implements buffered file operations, checked
seeking/growth, bounded whole-file reads, native exclusive renames, metadata,
filename-pattern filtering and iterative bounded directory operations. Source
`io` owns public managed errors, numeric adaptation through `encoding/binary`,
strict UTF-8 decoding and u32 Pascal framing. Source `path` calls a standard-library
internal facade over native host lexical paths. Public direct file construction
is private: factories establish acquisition and publication cleanup correctly.
Synchronous opening uses a source acquisition guard; async opening retains its
native acquisition until the opening activation successfully finishes.

## Evidence recorded during implementation

* Shared-ledger Debug regressions: native_source, runtime_callbacks and
  stage6_integrated passed, 3/3 (18.54 s).
* Existing async_source, async_ir, async_parallel and async_lifecycle passed,
  4/4 (56.77 s) after the initial native-job integration.
* Native job result, blocked-owner responsiveness, queued/pending cancellation,
  publication failure, acquisition rollback and cleanup-diagnostic tests passed.
* Native kernel paths, modes, cursor/read-ahead, EOF, lines, growth, metadata,
  filtered recursion, copy/move/delete, symlinks, budgets and 3 MiB streaming
  passed. A FIFO verifies looping over real short reads through actual EOF.
* First real VOID async file fixture passed JIT/O2 (28.70 s): shared aliases,
  number/Pascal/raw UTF-8 wire data, EOF, close/double close, whole-file bytes,
  metadata, filtered lookup and explicit recursive removal.
* Public-source wire test passed 18 checks with 42 numeric vectors, both byte
  orders, JIT/O2 and linked native AOT. Independent Python byte/file oracles
  include signed boundaries, IEEE signed zero, subnormal, infinities and NaNs.
* Ownership audit documented 149 public io/path declarations with zero warnings.
  Each JIT/O2 sync run recovered from 437 checked allocation failures; each async
  run recovered from 602. All precise roots balanced and unreachable objects
  collected: 7,093 checks. Later acquisition-shape changes require the final
  consolidated rerun; these numbers identify the intermediate receipt exactly.
* Actual VOIDLS pipe test passed 41 documentation, completion, parameter-help and
  repeated malformed-constant recovery checks with one persistent process.

Those bullets are intermediate evidence. Final receipts are in [evidence/](evidence/).
The later consolidated runs include Release, strict sanitizers, actual 1 MiB
stack cases, the complete 149-declaration inventory, controlled real-file native
jobs and the existing regression families. Native Windows/macOS execution remains
unverified; the handoff records the earlier unexplained Release fault separately
from final passing executions.

## Development corrections retained for review

The first real-file responsiveness harness confused the construction-count and
payload-size arguments of the existing managed probe. Its marker never released
the controlled read; this was a harness defect, not evidence of an owner-blocking
implementation. The probe now uses the documented payload-size argument and a
bounded worker gate. Only the corrected run can certify responsiveness.

Function headers remain on one line because the existing VOID parser does not
accept multiline formal lists. Function bodies, declarations and logical sections
retain readable four-space formatting; no syntax extension was introduced.

The first consolidated development run found a real allocation regression at
async allocation budget 580. Close had acquired its operation reservation before
entering `with`; allocation while registering scope cleanup could strand that
reservation and prevent the enclosing file cleanup. Close now acquires inside the
`with` expression, using an inactive lease for an already closed resource. This
keeps idempotent close and immediate `busy` replies without an unguarded ownership
gap. The permanent sweep passed 437 sync and 622 async injected failures in each
of JIT/O2, with 7,213 checks. The failed development receipt is retained separately;
some later tests in that run read a transient source type error during the repair.
Only a new frozen-source consolidated run can certify final acceptance.

The initial shutdown test incorrectly tried to enter Managed state after closing
the Runtime. The test now checks that repeated shutdown has no additional close
error, and does GC collection before shutdown. Ordinary manual close still proves
unreachable roots are collectible. Runtime shutdown intentionally rejects new
Managed entries; the test does not relax that policy.

The final shutdown audit added an entered-open reproducer. It demonstrated that
resource-ledger shutdown alone could return while opening still owned an
unpublished native acquisition. A narrow Runtime admission counter now covers
native jobs, including whole-file work. Ordinary work retires its admission after
native execution; an acquired open retains admission through owner publication
and source factory completion, or through drained rollback. Runtime close rejects
new admissions and waits without holding the GC mutex before retiring resources.
It does not force-terminate or cancel source bodies. Draining one's own pending
native work on its executor is rejected instead of deadlocking. Permanent tests
cover both resource opening and whole-file opening held behind a native gate.

The path audit removed an unnecessary cwd query when callers supply an explicit
absolute base. A POSIX deleted-cwd test proves explicit-base absolute/relative
operations work while implicit-base resolution returns an ordinary native error.
