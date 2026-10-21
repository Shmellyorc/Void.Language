# Stage 9 proposed roadmap: a complete application workflow

> Superseded CLI/authorization status, 21 October 2026: Boss authorized all three
> Stage 9 milestones and removed `--app` entirely, including compatibility use.
> [Current workflow](STAGE9_APPLICATION_WORKFLOW.md) and
> [acceptance](stage9/ACCEPTANCE.md) supersede conflicting proposals/status below.
> Historical decisions, experiments and verification records remain intact.


21 October 2026. **Research proposal for Wrench review and Boss authorization.**
Engineering has not begun. [Independent audit](STAGE9_RESEARCH.md) records current
source, test/probe evidence, historical issue dispositions and remaining gaps.
Baseline: `10f77fac23273c5af3fa3be6683c72c8ac059584`, with the pre-existing CI #87
workflow/documentation repair preserved. Stage 8 hosted lock is not claimed.

## Recommended stage outcome

A developer can check, run or build a conventional project or explicitly selected
VOID file. Application mode follows the compiler's existing validated entry kind;
main remains optional. Legacy entry(i32) examples keep input, printed result and
successful process status zero. `voidc build` produces a real native executable
using the existing LLVM object emitter and matching Runtime. Failures retain a
previous good artifact and explain the actual source/toolchain problem.

This is one cohesive stage, organized into three meaningful milestones. It does
not require a new compiler, project framework, package manager, watcher, scheduler,
verification platform or general FFI. Correctness work takes priority if a new
reproducible defect appears; no confirmed unresolved memory-safety failure was
found that presently calls for an unrelated preliminary repair sprint.

## Milestone 1: one predictable CLI and entry-selection path

**Benefit:** run top-level code/main without --app, use alternate application files,
and preserve old examples. **Dependencies:** dated approval superseding Sprint 4's
mandatory-mode policy, not a redesign of already-approved entry semantics.

Recommended command contract, proposed until approved:

| Command | Behavior |
| --- | --- |
| `voidc check <file-or-directory>` | Existing full source checking; libraries need no runnable entry. Directory uses shared main.void discovery. |
| `voidc run [file-or-directory]` | Default absent path to cwd; discover main.void for a directory, analyze once and use compiler-classified application/legacy entry. |
| `voidc run <file-or-directory> <i32>` | Numeric operand only for classified legacy entry; application operands reject clearly. No new string argv API. |
| `voidc emit <file-or-directory> -o <object>` | Existing object-only output, with automatic compiler-derived application/legacy behavior. |
| `--app` on run/emit | Keep as optional application assertion/compatibility spelling initially; no longer mandatory. Reject a legacy graph forced into app mode rather than silently change it. |
| `--no-opt`, `--cache` | Preserve established optimization and Debug-JIT cache meaning; reject cache for check/emit/build. Parse flags deliberately, not only a trailing suffix. |

An explicit source file already is an entry-file override. Do not add a competing
source `--entry` flag merely because voidls uses that flag for server startup.
`run tool.void` should work for either valid top-level code or eligible main,
without requiring the filename main.void. Explicit file parent remains the VFS
root; no hidden parent-project rebasing or package search.

Use `vd::discover_project`, `ValidatedAnalysis::application()` and
`entry_symbol()`, `require_source_entry`, retained `lower()` and existing JIT/cache
paths. The ordinary run path currently uses compile_project, while cache uses
analysis directly; share the smallest selection/validation helper that prevents
their dispatch contracts drifting. Keep all graph validation before execution.
Do not scan source again for main, infer mode from argv shape, or independently
select an imported main in the driver.

Preserve these already approved rules:

- No-result main and top-level fallthrough/bare return mean status 0; explicit
  typed main requires a return on every path. No implicit typed-main zero.
- Competing eligible main/top-level bodies reject; imported executable bodies
  reject and imports never execute source.
- Declaration-only legacy entry plus ordinary main keeps legacy precedence;
  malformed legacy entry does not silently become main mode.
- New applications return their status without printing it. Legacy calls still
  print the value and return process status 0 on successful execution.
- Typed IR, precise roots, cleanup, native permissions and entry-kind cache/reload
  contracts remain unchanged. LSP analysis is not execution authorization.

Normalize invocation failures to existing usage status 2, source/tool failures to
failure status 1, and application return values to the established OS exit-status
boundary. Do not confuse application status 1 with a compiler crash or print it as
a diagnostic when it is simply the application's result. Preserve original
compile/runtime diagnostics; source locations remain on captured snapshots.

**Primary files:** src/main.cpp; small shared frontend API only if needed;
src/frontend/compiler.cpp entry/analysis gates; current application/cache tests.
**Not required:** new parser syntax, a new application ABI, async main, custom
function entry selection or a linker driver yet.

**Acceptance:** auto application top-level/main/no-result/i32; typed-main missing
return; legacy default/positive/negative i32 input and main helper; explicit
alternate files; directory/no-path discovery; missing/invalid source and malformed
options; argument rejection; checkable libraries; normal/O2/cache parity. Compare
stdout, stderr and status separately. Update existing Sprint 4 assertions that
currently require no-flag rejection while retaining explicit-mode compatibility
and admission negatives. No test should simply be disabled.

## Milestone 2: native executable build through the existing backend

**Benefit:** `voidc build` finishes the object-to-executable step for ordinary
developers. **Dependencies:** Milestone 1 entry selection and a matching host link
configuration. This is the substantive new engineering in Stage 9.

Proposed interface:

```text
voidc build [file-or-directory] [-o executable] [--no-opt]
```

Omitted path uses cwd discovery, just like run. Recommend default output
`<selected project root>/build/<entry-file-stem>[.exe]`: the same selected entry
has one default regardless of invocation cwd; `-o` remains caller-relative or
absolute. This naming is a recommendation needing agreement, not an implemented
policy. A successful build does not run the executable. No automatic package
download, test execution, link privilege or public publication follows.

Implementation sequence within this milestone:

1. Select/analyze/validate the same program as run. Use normal `emit_object` with
   the existing source-program adapter; preserve emit as object-only output.
2. Resolve a matching host Clang 19.1.7 driver, configured linker, Runtime/support
   archives and C++/thread/platform options from existing CMake/toolchain data.
   Reuse the link shape in `tests/stage8/sprint4.py`, not only scalar tests that
   happen to link against libc. Match Runtime ABI, target/configuration and any
   sanitizer flags. Do not pull LLVM into the executable's runtime dependencies.
3. Provide deterministic build-tree and private install lookup for these resources.
   Today install/package_tooling only deploys server/adapters; installed compiler
   build is not solved. Generate minimal host-link configuration from CMake targets
   and arrange private compiler/Runtime installation, with literal configuration
   overrides where necessary. Do not compile Runtime from source per application,
   search arbitrary workspaces for archives or create a general manifest service.
4. Execute the driver with an argument vector. LLVM's existing process support is
   reusable; keep path encoding compatible with current Windows wide argv/UTF-8
   conversion. Report missing driver/archive/SDK, failed launch, nonzero link and
   crash distinctly. Keep raw child diagnostics available behind concise context.
5. Stage temporary object and executable safely, then publish only after successful
   linking. Protect source files, selected imports, directories and prior output
   from destructive path collisions. Clean temporary files on failure; never
   report a stale output as a newly successful build. Test replacement rules on
   Windows as well as POSIX. No unrelated generic transaction framework.
6. Preserve all source path characters. The independently reproduced LLD absolute
   percent-parent failure needs a real strategy: relative stage output with Clang's
   measured working-directory option is a candidate that avoids global cwd mutation.
   Prove it on native platforms; do not weaken VFS policy or move the acceptance
   output to a cleaner unrelated directory to hide the case.

**Host/link boundary:** choose a configured driver, not bare ld/lld. The driver
owns platform startup files, C++ runtime and SDK conventions. Existing Windows
dynamic-CRT/UCRT definitions and Linux linker overrides must survive; macOS uses
the configured SDK/driver environment. Keep LLVM 19.1.7. Cross-compilation, fully
static OS dependencies, signing, public redistribution and arbitrary linker flags
from imported libraries are outside this host-only command.

**Native providers:** automatic application classification and linking the shared
Runtime do not admit new source native descriptors. Current fixture/sink support
remains host-controlled; imports such as lib/raylib do not authorize C libraries.
Any later user-selectable native provider or external linking interface needs its
own review. Include tests that denied native source stays denied by build.

**Primary files:** CLI helper/driver, existing frontend analysis, normal LLVM object
emitter, CMake Runtime/install/toolchain targets, existing source/native integration
helpers. Avoid another backend or parallel Runtime library implementation.

**Acceptance:** produce and execute real scalar and managed/text/collection
applications and legacy programs in normal/O2 modes. Check status/stdout against
JIT and ordinary emit-plus-link. Exercise imported local libraries, readonly/result/
cleanup paths, default and explicit output, existing output replacement, failed
compile/link preserving good output, malformed/missing tools and resources,
spaces/Unicode/#/% names, long/native Windows paths, and source/output collisions.
Test from an unrelated cwd and a private staged install. All modes keep source
locations. No network, auto native admission or package-manager steps occur.

## Milestone 3: consistent failures and integrated developer acceptance

**Benefit:** the new commands are reliable through real error/cleanup paths and
retain editor/source parity. This completes the same executable workflow rather
than introducing another language-feature sprint.

Resolve the recorded AOT stdout/JIT stderr discrepancy for checked runtime
failures using the existing failure emitter/ABI. Keep successful application
output and legacy result printing unchanged. Preserve the primary runtime failure,
bounded caller locations, cleanup failures and correct exit status; avoid a new
diagnostics framework. Capture link-tool detail without rewriting source failures
as generic backend messages. A linker/OS diagnostic must remain distinguishable
from a VOID compile error and from the application's own explicit nonzero return.

Retain shared voidls selection/completion/docs, installed-standard readonly guards,
local-library identity, snapshots/UTF-16, editor version guards, incomplete typing
and clean shutdown. CLI tests may call voidls --discover to compare file/root
selection, but no editor should acquire a new CLI parser/compiler or native-binding
authority. Do not duplicate project discovery to make the commands seem consistent.

Use the existing actual VS Code/Neovim/Vim harnesses for affected behavior. VSCodium
must be executed before a new support claim; otherwise record it unverified. Keep
the existing CI #87 path override repair intact. A failed unrelated old matrix job
does not justify redesigning executable discovery or removing tests.

**Acceptance:** focused new regressions, then one consolidated full local Debug,
Release and strict sanitizer/leak pass for the completed stage; applicable existing
1 MiB frontend gates; real editor smoke/lifecycle checks; host-native executable
tests on supported Linux/Windows/macOS configurations. Use current suite inventory,
not old 156 or an invented total. Linux/local success is not hosted acceptance.
Boss alone controls the subsequent hosted run and Git actions.

Update internal contracts, examples used by tests, approvals and handoffs under
docs with dated scope/status. Root README changes require separate explicit
authorization. Keep failed attempts and platform limits. Wrench reviews before
Boss authorizes implementation or release actions.

## Remaining language and Runtime backlog

| Priority | Candidate | Reason and disposition |
| --- | --- | --- |
| Next completeness work | Primitive display, useful print, bounded numeric/bool interpolation | Most obvious remaining everyday output gap; source print is still unknown. Needs approved display/format/IO placement. Avoid a full culture/custom-format engine as a prerequisite for basic display. |
| Next library foundations | Private/declared-readonly storage | Public wrappers cannot protect fields using method privacy alone. Requires parser/IR/member lookup/promotion/construction/tooling access rules; no new field semantics are approved by Stage 9 research. |
| Approved future direction | Struct-owned operator overloads | Bible §66.28 approves direction, not grammar/resolution/hash behavior. Preserve managed identity and dictionary equality/hash coherence; separate owner-scoped engineering. |
| Measured Runtime follow-up | Retained Task/result/Work/group/Handle records | Frame retirement already landed. Safe record reclamation needs lifetime/handle/repeated-await/unobserved-failure proof; no constant-memory promise or scheduler rewrite now. |
| Later language work | Multiple returns, generic enums/unit syntax, nullable families/as?, runtime field defaults/fallible init | Real incomplete features, but not dependencies of the proposed native executable workflow. |
| Later APIs/design | Custom iteration, value search/removal, richer text/order/views, advanced interfaces/statics | Keep approved copy/readonly/equality/initialization rules; choose demonstrated use cases first. |
| Deferred architecture/performance | Automatic runtime source publication, incremental compiler, per-function cache, advanced GC, weak refs, general shared-memory/async FFI | Existing boundaries remain; no speculative optimization/framework in this stage. |

This backlog is not a list of implicit Stage 9 implementation requirements.
If a new independently reproduced correctness defect appears during engineering,
fix it within a scoped milestone before expanding convenience features. If a
workload only exposes a documented intentional boundary, do not disguise expansion
as a bug fix.

## Decisions genuinely requiring Boss

| Decision | Recommended choice | Why it matters |
| --- | --- | --- |
| Supersede mandatory --app | Compiler-derived automatic mode for run/emit/build; keep --app optional assertion initially. | Changes Sprint 4's explicitly approved CLI contract. Removal is the requested direction; this chooses migration detail, not whether to do the research. |
| No-path/default output policy | run/build with no path use cwd main.void; default build output under selected root/build/entry-stem with native suffix. | Makes command/discovery/output behavior reviewable before it becomes public convention. |
| Matching private toolchain resource layout | Minimal CMake-derived host driver/archive configuration plus staged compiler/Runtime install; no package manager. | Current server-only install cannot supply installed native build. Exact implementation factoring remains engineering judgment. |
| Final stage scope | Three workflow milestones above; display/encapsulation/operators require a separate explicit addition. | Prevents a CLI/correctness stage turning into all remaining Bible features. |

Existing main return/coexistence rules, stdlib/local-library spellings, numeric
edge semantics, collection errors, typed IR and GC/native authority are settled.
Do not repeatedly request those decisions. Argument parsing mechanics, temporary
file helpers, exact internal config types and test organization are ordinary
engineering choices within the approved contract.

## Package-manager separation

The future Git-based package manager remains separate. Preserve current local
source consumption and canonical identities as useful interfaces. Stage 9 must
not add Git/network fetching, version solving, registry/lockfile syntax, lifecycle
scripts, package native-link permissions or an invented external-project identity
contract. No complete separate package specification was supplied by this mission;
the roadmap does not pretend to reconstruct it from names in examples.

## Exit criteria and review handoff

- Main/top-level/legacy commands work with reviewed automatic selection and explicit
  file overrides; no mandatory --app, no silent legacy status change.
- Build produces an actual runnable native executable through existing LLVM/Runtime
  paths, with deterministic dependencies and tested private install behavior.
- Compile/link/runtime failures and output preservation work on the actual supported
  hosts, including percent/Unicode paths and denied native imports.
- Focused, full local and applicable editor/small-stack evidence is recorded without
  inflating counts or claiming an unexecuted hosted lock.
- Production architecture, source identity, readonly, snapshots, UTF-16, precise
  roots, cleanup, cache/reload and provider admission remain intact.

Research delivery: current-source rebuild, 16/16 focused suites, 28 source probes
plus ten CLI cases, six real AOT parity cases and external-linker isolation are
complete. Only internal research documentation was changed. **Stop for Wrench
review and Boss authorization; this roadmap is not implementation permission.**
