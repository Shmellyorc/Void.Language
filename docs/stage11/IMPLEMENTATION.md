# Stage 11 runtime globals — engineering handoff

25 October 2026. **Implementation complete; final Linux verification complete; review pending.**
Boss authorized runtime globals only. The Stage 11 research/roadmap's proposed
file/packet/text libraries and package-manager work remain outside this mission.
The approved root-global visibility and eager startup rules supersede conflicting
research recommendations. [Language/runtime contract](../RUNTIME_GLOBALS.md).

Baseline: Stage 10 source `d507240`, lock record `909c0c3`. Pre-existing
`stdlib/console.void` edits and the untracked research/roadmap were preserved.
Root README and Git history are unchanged; no hosted action has been dispatched.

## Implementation

- Parser extracts outermost vars into declarations with ordinary typed initializer
  functions; script extraction, local scope and fields retain their shared checker.
- Canonical lookup covers own-file, project-root, imported and aliased globals.
  Own-module bindings, root constants and type-constructor contexts preserve
  their existing precedence over imported globals. Existing compiler-owned
  declaration/reference records carry tooling identity.
- Typed IR owns ordered global metadata and an explicit guarded cell operation.
  Independent validation checks identities, ownership, visibility, nominal storage,
  initializer signatures, startup task/callback restrictions and worker effects.
- LLVM uses ordinary complete managed construction, places, copies and root frames.
  Its host adapter initializes dependency-first SCC/source-order globals once.
- Runtime persistent Handles retain complete cells, owner admission and owned
  failed-startup diagnostics. Native shutdown precedes dropping global roots.
  Root setup builds a local
  Handle vector, then publishes it under the Runtime lock so shutdown cannot
  race its vector mutations.
- Fresh compile/cache paths isolate applications; compatible reload reuses Runtime
  admission and exact initialization/dependency contracts.

## Verification

Focused final checks pass: 101 native global checks on a real 1 MiB stack,
157 real source/JIT/AOT commands, bounded/annotated global inference on 1 MiB,
and actual Neovim, Vim and VS Code global workflows. The root-setup/shutdown
stress fixture also passes 128 overlaps (256 assertions) in Debug and strict
ASan/UBSan, both on a 1 MiB stack. The final frozen revision passes all
**203/203 tests in each of Debug, Release and strict ASan/UBSan: 609/609 total**.
Exact results, commands, logs and artifact hashes are in
[local verification](ACCEPTANCE.md).
Regressions cover source semantics,
real cross-module normal/O2 JIT and independently linked AOT, readiness failure,
GC between publications and calls, native cleanup, isolation, reload, cache hits,
worker rejection/snapshots, malformed IR and compiler-owned LSP/overlays/rename.
Function globals retain stable dispatch
across compatible reload while obsolete generation leases retire. Changes to
visibility, layout, type, order and initialization-helper bodies reject replacement.
The [source freeze](SOURCE_FREEZE.json) identifies the reviewed code/test bytes.
No authored test is counted as a passing result until executed.

Engineering failures are retained in local build logs. The initial compile caught
an incorrectly placed completion enumeration and an aggregate-Module initializer
compatibility break; both were repaired. One build read a source file during an
in-place rewrite and Clang crashed; subsequent engineering edits use atomic file
replacement, and that build is not acceptance evidence.

Native Windows/macOS and final hosted CI remain Boss-controlled. Stage 10's
accepted native editor/second-volume exclusions are not converted into passes.
The earlier intermittent Debug LLVM DSE failure remains an explicit
[reliability finding](OPTIMIZER_FAILURE.md), even when later runs pass.
Golden Wrench must review the final implementation and exact local evidence before
Boss-controlled acceptance.
