# G30 — standard-library discovery and guidance

13 November 2026. Documentation only, based on
`2a14bb619d761e4c66c84ddee3c857875e0ec0a3` and the unchanged
[research audit](G30_STDLIB_COMPLETENESS_AUDIT.md).

| Changed documentation | Exact improvement |
| --- | --- |
| [docs/README.md](../README.md) | One complete 29-module navigation table with authoritative sources/guides, separate builtin guidance and existing configuration, text, collections, socket/HTTP, package, file and process examples |
| [docs/collections/API.md](../collections/API.md) | Fixed values versus shared growable lists, owned slice-copy cost/authority, comparator specialization, typed callbacks and map inference; existing repeated generic guidance consolidated |
| [docs/text-encoding/API.md](../text-encoding/API.md) | Search sentinel versus typed/string errors, checked Runtime failures, encode/try_encode, unwrap and eager fallback; existing text/codec examples |
| [docs/io/API.md](../io/API.md) | Lexical paths versus held filesystem authority, checked flush/close, cleanup and publication/synchronization outcomes with existing examples |
| [docs/stage10/CONSOLE_IO.md](../stage10/CONSOLE_IO.md) | Convenience read/readline information loss, prompt failure and checked input selection; historical evidence unchanged |
| [docs/channel/API.md](../channel/API.md) | Synchronous owner-thread pub/sub versus async completion/worker boundaries |
| [docs/ASYNC.md](../ASYNC.md) | Explicit adapter requirement for async stdlib calls, synchronous CLI alternatives and existing host/example links |
| [docs/process/API.md](../process/API.md) | Explicit image/argv/environment/cwd authority, start versus capture, output/time budgets and outcome/exit/UTF-8 checks |

This report is the ninth changed documentation file. No API declarations,
examples, compiler, Runtime, stdlib implementation, root README or historical
milestone record was changed. No new API catalog or documentation system was added.

Verification used the existing Linux Release `voidc` and LLVM **23.1.4**:

- Inventory: **29/29** physical `stdlib/**/*.void` modules linked exactly once,
  with correct import names and authoritative source paths.
- Links in the eight edited guides: **222 local path/anchor occurrences checked,
  zero unresolved**; 19 existing external links were unchanged and not fetched.
- Source/API agreement: **13 exact declaration assertions passed**, covering
  collections, console, process, conversion and file/save operations.
- Final checker pass: **45/45 PASS** — 29 disposable canonical import consumers
  and 16 newly linked maintained `.void` examples. Commands were
  `build/llvm23-release/voidc check <source>`. No example execution, full suite,
  native platform matrix or hosted acceptance is claimed.
- Root README and research report hashes were preserved; production/example
  paths have no diff. Final report links were checked separately.

Two verification setup failures are retained: a temporary assertion initially
assumed boolean file flush/close results, while the source correctly returns
`result<i64, error>`; the verifier was corrected. Directly checking `stdlib/time.void`
as an application source omitted canonical stdlib intrinsic admission and failed.
All modules subsequently passed through ordinary `use MODULE` consumers. Neither
probe justified an implementation change or weakened a check.
The final link-only pass briefly overwrote the detailed checker JSON; its output
log was retained and the same 45 checks were repeated to restore full receipts
(90 successful checker invocations overall, plus the initial direct-source refusal).

The existing verified-ZIP/retention procedure preserves the checker receipts,
source hashes, probes, original failure and exact documentation patch outside the
checkout. Task-owned scratch disposition and final Git checks are recorded below.

Archive: `/home/shmellyorc/Documents/Void.Language-archives/g30-stdlib-guidance-2026-11-13.zip`.
SHA-256: `584f148f62b521817c5f71435494fd125ba19e925c3c92469ad41227a8753b22`.
**42 files**, 111,435 uncompressed bytes, **35,306 ZIP bytes**, DEFLATE level 9;
CRC, full extraction, restored hashes and original rehashes passed. The existing
archive index and adjacent cleanup receipt retain verification. The prior G30
research ZIP hash also matched. No earlier evidence was replaced.

Removed only `/tmp/void-g30-docs-d6upss67` and its location pointer: **111,435 logical /
262,144 allocated bytes**. `/tmp` free bytes before/after: **16,509,624,320 /
16,509,890,560**; host filesystem **362,319,765,504 / 362,319,765,504**.
These are immediate cleanup measurements, not exact attribution of concurrent
filesystem activity. No VM, build, SDK, provider or owner editor output was touched.

Final `git diff --check`: **PASS**, no output; staged diff **empty**. HEAD remains
the baseline. Git status is eight modified documentation files listed above plus
this new untracked report and the pre-existing untracked research report. No stage,
commit, push, hosted CI, production/API change or new platform claim was made.
