# G60 — Faster warm project startup

G60 replaces the eager construction of a name-visibility set in the
frontend with point lookups in the existing declaration indexes. The
accepted parent is G59 `1c3cfc4e9bd0d8ca169d08fc470c9c214ece1649`.
G55–G58 remain held. G60 leaves the G59 package workflow and bundled
raylib inputs intact. See the [accepted G59 report](../projects/G59_FIRST_CLASS_PACKAGE_EXPERIENCE.md).

## Where startup time went

`analyze_manifest_project` in `src/frontend/project_selection.inc`
dispatched one `validate_graph` for the executable source graph.
`validate_graph` populated the unit indexes and constructed a
`Semantics` checker repeatedly to validate declarations, expressions
and bodies. The old constructor traversed every unit's structures and
globals, populated a red-black tree of visible names from the source
root, current unit and imports, and freed it when that checker finished.
Three call sites needed a membership answer for one spelling at a time.
A GDB entry/return count on the accepted compiler recorded 3,737
constructions in the exact raylib process. G60 removes all 3,737 of
those per-checker sets and their graph-wide walks. G43's packed
definite-initialization solver is a separate validation step and is
unchanged. [G43 proof and measurements](G43_SOURCE_IR_VALIDATION_SCALING.md)

The accepted compiler's user-space CPU profile sampled 1,669 events.
Red-black-tree unique insertion-position search alone took 13.64% of
samples and tree insert/rebalance took another 5.20%. G60 sampled 671
events, with 1.79% in the insertion-position routine for remaining
compiler work. These symbols have other uses and the sampler lacked
stack unwinding and hardware counters on this host. Source inspection,
the constructor count and the paired phase timings isolate the
frontend change without assigning every tree sample to it.

Clang supports point lookup by name through existing declaration
contexts. Rust resolves names using the appropriate scope and namespace.
G60 applies that technique to the current VOID unit indexes, with the
existing VOID import and visibility rules. [Clang declaration contexts](https://clang.llvm.org/docs/InternalsManual.html#declaration-contexts) · [Rust name resolution](https://rustc-dev-guide.rust-lang.org/name-resolution.html)

## Source change and validity

The only production change is in `src/frontend/compiler.cpp`. The new
`Semantics::visible_name` handles declaration shadowing, the
`tasks.group()` special form's shadowing guard and assignment to
readonly declarations. The old set and all of its population code are
gone. The lookup returns the same union of admissible names.

| Source of a name | Lookup rule |
| --- | --- |
| Current unit | Types, globals, constants, functions and import qualifiers |
| Implicit project root in the same library | Visible types and globals, and constants that are not file-private. The current-unit lookup includes root-private names when applicable. |
| Direct imports without an explicit alias | Types, globals, constants and functions admitted by the original `source_visible` predicate |
| Explicitly aliased imports | The alias itself is reserved. The target's declarations require qualification. |

The graph's declaration and qualifier indexing runs before semantic
body checks. Generic specialization can later append structure keys
containing angle brackets. Generated display, collection and JSON
adapters can append reserved, dollar-prefixed names. Those spellings
cannot collide with a source identifier queried by these three sites.
Generated checker locals use a separate set of names, such as
`$value` and `$pattern`. The diagnostic code, related declaration search,
original source spans and emission order stay in the existing callers.
The stdlib random seed parameter retains its original exception.
The point query can visit each direct unaliased import on a miss, so
an unusually wide unaliased import list still has a per-lookup cost.
This milestone measured the exact raylib graph and a dependency-free app.

Project capture still verifies manifest, lock and native identities.
Discovery and execution retain their source snapshots and freshness
comparison. The observer saw the same two captures, two lock checks,
two native selections, two discovery inventories and seven parser
calls on both binaries. Program construction, LLVM lowering and
native load still execute. No source or semantic cache, new allocation
authority, Runtime change or ABI change is introduced. The early checks
on invalid input remain in place. The removed frontend allocations
were ordinary C++ container work. Runtime allocation and recovery use
the pre-existing code and were exercised in the selected controls.

## Launch measurements

The primary comparison is the accepted G59 integration executable and
the G60 Release executable on the G59 raylib project with its exact
application, wrapper, lock, descriptor, shim and upstream shared
library. Every measured success returned the independent stdout
oracle `G59 raylib pixel: 241`. Linux ran on an AMD Ryzen 7 1700X,
with the timed process pinned to logical CPU 7. The primary observations
alternate G59/G60 and G60/G59 and use fresh processes, warm filesystem
state and the existing prepared catalog. The stopwatch starts before
process creation. It stops at the first complete stdout line and again
at process exit, and records the process's own peak RSS using
`wait4`. Phase measurements are separate.

| Warm JIT workload | G59 first output ms, median (range) | G60 first output ms, median (range) | G59 → G60 total wall ms, medians | Paired median first-output change |
| --- | ---: | ---: | ---: | ---: |
| Raylib normal, 10 pairs | 1,690.91 (1,677.89–1,704.22) | 633.68 (624.98–645.75) | 1,722.47 → 664.20 | −62.70% |
| Raylib O2, 10 pairs | 1,674.04 (1,669.36–1,762.78) | 625.15 (617.94–786.87) | 1,703.72 → 655.09 | −62.57% |
| Ordinary normal, 16 pairs | 7.72 (7.61–8.07) | 7.83 (7.58–8.14) | 9.80 → 9.85 | +1.49% |
| Ordinary O2, 16 pairs | 7.49 (7.37–8.16) | 7.61 (7.41–8.46) | 9.48 → 9.52 | +0.86% |

The raylib latency improved in every pair. Its median peak RSS rose
from 132.93 to 135.12 MiB in normal JIT and from 132.89 to 134.86 MiB
in O2 JIT. The ordinary project's first output was slightly slower
within overlapping ranges. Its median peak RSS changed from 26.41 to
26.59 MiB in normal JIT and from 26.42 to 26.54 MiB in O2. The
small-project memory observations were replayed in a fresh harness
using streamed executable checksums after the initial harness
inflated its child's inherited peak RSS. Both raw logs are retained.
The Release compiler executable was 48 bytes smaller than G59.

Three balanced debugger pairs, measured separately, locate the change
within the work before LLVM. Intervals overlap and must be read
individually.

| Raylib phase | Calls per process | G59 median ms | G60 median ms |
| --- | ---: | ---: | ---: |
| Frontend analysis | 1 | 1,497.18 | 459.49 |
| Project capture | 2 | 60.65 | 61.68 |
| Locked dependency verification | 2 | 58.10 | 59.07 |
| Native catalog selection | 2 | 36.20 | 36.69 |
| Source graph discovery inventories | 2 | 0.70 | 0.69 |
| Parser | 7 | 26.13 | 25.58 |
| Source graph validation | 1 | 1,186.99 | 142.70 |
| Frontend program construction | 1 | 204.91 | 210.06 |
| IR lowering and JIT | 1 | 215.69 | 217.72 |
| Native dynamic loads | 2 | 35.83 | 35.84 |

The first-launch control copied a fresh project for each observation
with the original manifest, lock and already prepared native catalog.
It leaves the filesystem page cache warm. Fresh acquisition and native
build were exercised separately in the qualification workflow.
The unpaired download and auto-preparation observation was 7.797 s
during G60 qualification and 5.431 s in the retained G59 integration
run. The tests ran under different background load and download
conditions. No part of native preparation changed in G60.

| Additional control | G59 median wall ms | G60 median wall ms | Pairs |
| --- | ---: | ---: | ---: |
| First launch in a fresh project copy | 1,925 | 814 | 4 |
| Source-only edit | 1,763 | 717 | 4 |
| Reviewed package source edit | 1,728 | 677 | 4 |
| Reviewed package produces a different pixel | 1,761 | 688 | 2 |
| Valid manifest edit | 1,763 | 684 | 4 |
| Full project check | 2,040 | 644 | 4 |
| Locked dependency resolution | 64.71 | 65.22 | 4 |
| Compiler `--version` process | 4.94 | 4.99 | 16 |

These controls come from the broader paired run, which overlapped
background qualification jobs. Its warm raylib first-output medians
were 2,806 → 1,142 ms in normal JIT and 2,877 → 1,163 ms in O2 JIT.
The quieter dedicated replay above supplies the launch estimate.
In the broad run raylib warm peak RSS increased approximately
0.5–0.6 MiB and locked resolution and the version proxy were slightly
slower. The one uncached small O2 observation increased from 40.1 to
43.6 ms. The repeated observations are more informative than that
single pair. No native build-time or AOT runtime speedup is claimed.

The source-only edit changed the printed text, and after a reviewed
package edit the changed pixel caused the application's expected
exit 19. Editing the package without reviewing a new lock, modifying
the recorded dependency tree hash, selecting an invalid manifest root,
or altering the native descriptor or the installed library all
produced byte-identical rejection diagnostics against G59. Both
compilers built and ran independent normal and optimized AOT
executables with the exact output oracle. The archive retains their
different executable identities and makes no code-identity claim.

Debug object caching still gave a G59 miss followed by a hit. The
same project missed after switching to G60, then hit. Invalid edited
source failed before running. Valid edited source missed, returned
the edited text, and returned to a hit when restored. The replay
recorded three existing-format objects totaling 21,275 bytes. Raylib created no Debug
object cache in this native-provider configuration. G60 added no
on-disk cache. Normal execution without explicit cache flags is the
raylib acceptance control.

## Qualification

- Linux Release passed 24 distinct selected tests, plus the full
  headless raylib lifecycle with G59's normal/O2 JIT and independent
  AOT checks, download/preparation, unchanged catalog reuse, explicit
  trust, tamper rejection and restoration. The initial package test
  found a symlink in the task-owned build view's notices. A replay
  using exact physical notice directories passed that test and the
  two related packaging tests. Product source and security policies
  were untouched. Both receipts are retained.
- Fatal Linux ASan/UBSan/LeakSanitizer passed 23 distinct selected
  tests and the separate headless raylib workflow. The controls
  included frontend and module checks, LSP recovery, async and
  Runtime callbacks and GC buffers. The 25-case G59/G60 visibility
  matrix compared exit status, complete stdout and stderr. It covered
  aliases, private and internal declarations, implicit-root visibility,
  readonly assignments, diagnostic order and the task-group guard.
  The source change is a synchronous read of the existing per-analysis
  indexes and adds no shared state or changed concurrent path. No new
  TSan run was required under that scope.
- Windows 10 x64 build 19045 used LLVM/Clang/LLD 23.1.4. Both Debug and
  Release passed the headless raylib lifecycle, normal/O2 JIT and
  independent normal/optimized AOT, including native ownership,
  explicit trust, catalog reuse, tamper rejection and manifest/lock
  restoration. The selected suites each passed 9/9. This Release
  run built `void_project_creation.exe` and exercised it in
  `project_maintenance`, including interruption recovery. G59's
  historical Release result remains 7/8. PE headers confirmed a
  1,048,576-byte stack reserve for the compiler, language server,
  covered controls and the independently linked applications.
- Real VS Code 1.141.0 with the G60 Release `voidls` passed 14
  recorded events. The open project recovered after add, displayed
  semantic function coloring, hover and signatures, and found
  `rl.` completions and both physical source definitions. It reported
  bad calls and recovered from unsaved application edits, saw a saved
  and re-locked package edit, rejected a physical native declaration
  edit at lock verification, recovered on restoration and detected
  removal and re-addition of the dependency. The native declaration
  test requested a real project refresh. Installed locked package
  buffers retain their existing read-only overlay semantics.

The pinned LLVM/Clang/LLD sources identify
`7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`. The production
postimage for `src/frontend/compiler.cpp` has SHA-256
`3ce39cd1093d6d8a9efeee67dd79d8a957985a416f1411dddf920ea89b56f79d`.
The Windows packet's source hash matches it. The archive records the
remaining source and binary hashes, raw balanced samples, checked
fixture bytes, diagnostic oracles, replay, and qualification receipts.
Its patch restores from G59 and covers the compiler file, this report
and the permanent ledger.

**Recommendation: INTEGRATE.** Review the isolated three-file change.
The measured gain is frontend work removed from each new process.
Any future startup work can re-profile current program construction
and JIT costs with this change in the baseline. No cache or additional
frontend change is part of G60.
