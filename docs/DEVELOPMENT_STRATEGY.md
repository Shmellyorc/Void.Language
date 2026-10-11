# VOID 2.0: Development Strategy and Compiler Architecture Roadmap

**Project/repository:** `Void.Language`  
**Language:** VOID 2.0 (`.void`)  
**Status:** Proposed development plan, agreed direction; implementation not yet started by this document  
**Date:** October 10, 2026  
**Companion document:** `VOID_2_0_Language_Bible.md`

> **Purpose:** This document preserves our agreed development strategy so a future conversation, contributor, or milestone team can continue without rebuilding architectural context. The Language Bible defines what VOID 2.0 should do. This roadmap explains how to develop it, validate its foundations, and decide when a milestone is truly complete.
>
> **Important naming decision:** The project is called **`Void.Language`**, not `Void.Language2`. The earlier project has been backed up. This document does not assume any existing files have been modified.

---

## 1. Overall direction

VOID 2.0 is a new native programming language with a deliberately small, readable surface and a capable compiler/runtime underneath it. Unlike VOIDC 1.0, VOID 2.0 is **not obligated to reproduce C# language semantics**.

Priorities:

1. **Easy and enjoyable to use.** Simple syntax, useful defaults, helpful errors, strong IntelliSense and documentation.
2. **Fast native execution.** LLVM ahead-of-time (AOT) compilation for release builds.
3. **Automatic memory management.** Precise GC without exposing ordinary pointer management or forcing manual allocation/freeing.
4. **Built-in hot reload.** Compatible function-body changes apply while the application is running. No automatic restarts or nagging dialogs.
5. **Excellent developer tooling.** Compiler-quality source diagnostics, source-level runtime stack traces, documentation, completion, navigation and library discovery.
6. **Cross-platform consistency.** Windows, Linux and macOS use the same language rules. Target-specific implementation work stays inside the compiler, runtime and packages.
7. **Simple native interoperability.** Existing C libraries are accessible through typed bindings and normal VOID wrappers, without requiring application developers to learn C pointers or C string conversions.

**Core principle: The compiler should work hard so the developer does not have to.**

### Five development rules

1. **KISS and DRY.** One implementation of each language rule. Avoid duplicate compilers, unnecessary abstraction layers, redundant paths and competing subsystems.
2. **Prove difficult mechanisms early.** Validate LLVM code generation, precise GC references, native interop and hot reload in small isolated experiments before creating hundreds of dependent features.
3. **One milestone, one responsibility.** Complete, test and audit each milestone before advancing. Use focused deliverables and clear acceptance conditions.
4. **Cross-platform from the beginning.** Treat Linux and Windows as first-class targets, not a port deferred until late in the project. Add macOS as soon as a suitable test environment is available.
5. **The Language Bible is the source of truth.** Every feature must conform to its documented semantics. Deliberate design changes update the Bible and a decision record.

### Non-negotiable performance principle

**Never sacrifice GC, safety, language semantics, or ordinary developer simplicity solely to improve a benchmark. Optimize the implementation instead.**

This specifically means no requirement for unsafe application code, manual memory management, explicit pointer manipulation or platform-specific source just to obtain good baseline performance. Optimization claims must be validated on representative workloads.

---

## 2. One compiler, two execution modes

```text
                     VOID 2.0 source (.void)
                               |
                               v
                      Compiler frontend
                  lexer / parser / resolver
                 semantic analysis / types
                    source diagnostics
                               |
                               v
                       Typed VOID IR
                               |
                    Shared LLVM lowering
                               |
                   +-----------+-----------+
                   |                       |
                   v                       v
               LLVM AOT                LLVM ORC JIT
             Release mode             Development mode
                   |                       |
                   v                       v
        Optimized native executable   Running native application
                                     + compatible hot reload
                   |                       |
                   +-----------+-----------+
                               |
                               v
                      Shared VOID runtime
            GC / allocator / strings / threads
               tasks / bindings / diagnostics
```

**One parser, one type checker, one set of semantics, one typed IR.** The AOT and JIT execution paths differ in optimization and code-lifetime policy, not in what the language means.

- **Release:** AOT native executable. No JIT required at runtime. Compiler may inline and specialize aggressively when safe.
- **Development:** Native code generation via LLVM ORC. Functions eligible for replacement have stable dispatch identities. Compatible edits can be compiled and applied safely while the application stays alive.
- **Both:** The same GC model, string rules, type behavior, native bindings, error semantics and runtime contracts.

LLVM provides native code generation, optimization and ORC JIT infrastructure. It **does not** implement our complete GC, source language, hot-reload safety or threading policy for us.

### Architecture independence

**VOID 2.0's language specification must not depend on LLVM-specific concepts.** LLVM is the intended implementation backend, not part of the observable language contract. The IR should make it possible to replace or supplement LLVM later without changing VOID syntax or semantics.

---

## 3. Architectural boundaries

| Component | Responsible for | Should not be responsible for |
|---|---|---|
| **Frontend** | Lexer, indentation, parser, VFS/module resolution, symbol resolution, type checking, language diagnostics | Native machine-code instruction selection |
| **Typed VOID IR** | Well-defined typed operations, control flow, object/reference information, lowered language semantics, source locations | Platform-specific OS implementation |
| **LLVM backend** | Optimizations, native machine code, object files, calling convention lowering, debug metadata | Deciding what `match`, `defer`, `Ok`, structs or properties mean |
| **Runtime** | Memory allocator, GC, safepoint coordination, strings, tasks, threads, panic/runtime handling, native-boundary services | Parsing `.void` source files |
| **Standard library** | Collections, files, synchronization APIs, documented helpers, platform abstractions | Compiler parsing/type-checking rules |
| **Language server** | Completion, hover, signatures, definition/reference lookup, rename, diagnostics | An independent, competing parser and type checker |
| **Development host** | Watching files, incremental compilation, JIT loading, versioned function dispatch, compatible reload coordination | Different semantics from release builds |

### Sharing and reuse rules

- The language server **reuses the compiler frontend**, including symbols, types, documentation comments and source spans.
- Both AOT and JIT paths **reuse the same LLVM lowering** as far as practicable.
- The runtime has explicit, small contracts with generated code. Avoid forcing high-level language implementation details into every subsystem.
- Common standard-library behavior should live in readable `.void` source wherever practical. Reserve compiler/runtime primitives for operations that actually need low-level support.
- Do not copy VOIDC 1.0's implementation wholesale. Reuse proven ideas, diagnostics conventions and tests without importing its architectural baggage.

---

## 4. Eight development stages

Each stage is divided into small milestones. Stage completion requires integration validation, not merely feature demonstrations.

| Stage | Focus | Required outcome |
|---|---|---|
| **0. Architecture proofs** | LLVM, GC roots, JIT replacement, native calls | Evidence that the foundation is technically viable |
| **1. Compiler frontend** | Lexer, indentation, parser, VFS, diagnostics | Compiler understands VOID source faithfully |
| **2. Core native language** | Types, `var`, `const`, functions, branching, loops, structs | Useful programs compile and execute natively |
| **3. Managed runtime** | Allocator, GC, reference and value structs, strings, lists, dictionaries | Automatic managed memory is correct under optimization |
| **4. Advanced language** | Generics, interfaces, properties, `match`, `result`, `defer`, closures, callbacks, events | Language systems cooperate with predictable semantics |
| **5. Hot reload integration** | Incremental compilation, dispatch, compatibility checks, old-code retirement | Running programs update compatible behavior without restart |
| **6. Concurrency and native libraries** | Async, scheduler, thread pool, synchronization, C bindings | Managed concurrency and native interop work safely together |
| **7. Tooling and optimization** | LSP, extension, library discovery, benchmarks, packaging | Developer-ready toolchain and validated performance |

**Important:** These stages express emphasis, not a rule to defer architecture-sensitive concerns. GC metadata and reload identities must influence early IR design. Source spans, documentation metadata and LSP-ready symbol information must begin in Stage 1. Windows/Linux checks must begin in Stage 0/2, not Stage 7. Native bindings also need an early Stage 0 proof even if their full library ecosystem comes later.

### Stage 0: Four early architectural proofs

#### Proof A: LLVM AOT native executable

- Generate and execute a minimal native program from hand-written or trivially generated LLVM IR.
- Call a function, return values and verify control flow.
- Generate usable source-location/debug information.
- Validate toolchain and linking on Linux and Windows.
- Keep the experiment independent of the full VOID parser.

#### Proof B: Precise garbage-collector roots

- Allocate managed objects and construct metadata describing their reference fields.
- Keep reachable references live across calls, loops and optimized code.
- Collect unreachable objects while preserving reachable ones.
- Validate reference tracking under LLVM optimization, not only in unoptimized debug builds.
- Verify GC safepoint and stack-map/statepoint integration against the pinned LLVM version.

#### Proof C: Hot replacement of native functions

- Give a compiled function a stable logical identity.
- Compile a replacement, make new invocations use it and preserve existing process state.
- Demonstrate safe behavior when old implementations are active on another thread.
- Retain old code until it is provably safe to retire.
- Do not assume ORC's ability to remove JIT resources automatically proves safe active-frame retirement.

#### Proof D: Native C interoperability

- Call a real C function through the target ABI.
- Pass a UTF-8 string with the appropriate lifetime and termination.
- Handle a native resource through an opaque wrapper and deterministic cleanup.
- Register and invoke a C callback with a stable callable address.
- Verify basic cross-platform linking and missing-library diagnostics.

**Gate:** If a proof fails, revise the architecture while the dependent codebase is still small. Do not expand the language surface until the relevant risk is resolved.

---

## 5. First ten milestones: #001-#010

Use milestone blocks of ten with an integration audit at the end. Numbering is for focus and traceability, not for racing to high milestone counts.

| Milestone | Responsibility | Minimum acceptance evidence |
|---|---|---|
| **#001** | Project structure, native build system, pinned LLVM toolchain, Linux/Windows smoke-test setup | Reproducible toolchain configuration and basic builds on available target systems |
| **#002** | LLVM AOT executable generation | Native program compiles, links, runs, returns the expected result, with source metadata |
| **#003** | LLVM ORC JIT function invocation | Generated native function can be JIT-compiled and called with validated ABI/signature |
| **#004** | Stable function identities and first hot replacement | Existing dispatch entry uses a new compatible function after an update; state remains unchanged |
| **#005** | Managed allocation and type metadata prototype | Test managed objects can be created and their reference-bearing fields described |
| **#006** | Precise GC root tracking and collection under optimization | Optimized code preserves live references; unreachable objects are reclaimed correctly |
| **#007** | Thread coordination for GC and code replacement | Safe thread rendezvous and no early retirement of currently executing code |
| **#008** | Native calls, UTF-8, ownership and callbacks | ABI tests, string-lifetime tests, callback test and deterministic resource release |
| **#009** | Combined runtime/LLVM stress tests and finalize cross-subsystem contracts | AOT/JIT/GC/native tests remain correct together; known limitations documented |
| **#010** | Architecture integration audit and decision review | Evidence-backed go/no-go decision for the broader compiler implementation |

### Scope cautions for the first block

- **#004 is an initial single-system proof**, not an excuse to claim fully production-ready multithreaded hot reload. #007 and #009 complete the cross-system evidence.
- **#006 cannot be locked solely because an unoptimized test passes.** LLVM-optimized code must preserve all required reference information.
- **#008 uses representative native calls**, not a giant package manager or SDL wrapper implementation.
- If experiments reveal a different dependency order, adjust the milestone plan deliberately and record why.
- Do not introduce the full VOID syntax, editor extension or every collection method into this first block.

---

## 6. Compiler implementation language: decision still open

**Current preference to evaluate: C++ for the compiler**, largely because LLVM's most extensive interfaces and much of the ORC tooling are C++ oriented. This is an engineering choice, **not** a change to VOID 2.0 syntax or a desire for developers to write C++.

Considerations:

- **C++ with LLVM C++ API:** Broad direct LLVM access and mature examples, but more C++ and LLVM-version complexity.
- **C using LLVM C API:** Potentially simpler compiler implementation style, but validate that the required ORC/JIT, GC and debug operations are adequately exposed before choosing it.
- **Runtime implementation:** C or C++ depending on actual requirements; the runtime ABI must be explicit and stable.

**Decision gate:** Choose the implementation language only after surveying the LLVM APIs needed by the four architecture proofs. Pin a tested LLVM version instead of chasing every upstream release.

---

## 7. Proposed repository structure

Use the existing project name **`Void.Language`**.

```text
Void.Language/
├── docs/
│   ├── LANGUAGE_BIBLE.md
│   ├── ARCHITECTURE.md
│   └── DECISIONS.md
│
├── src/
│   ├── frontend/
│   │   ├── lexer/
│   │   ├── parser/
│   │   ├── symbols/
│   │   └── semantics/
│   ├── ir/
│   ├── backend/
│   │   └── llvm/
│   ├── runtime/
│   │   ├── gc/
│   │   ├── memory/
│   │   ├── threading/
│   │   └── native/
│   └── tooling/
│       └── lsp/
│
├── stdlib/
│   ├── collections/
│   ├── files/
│   └── sync/
│
├── tests/
│   ├── compiler/
│   ├── runtime/
│   ├── integration/
│   └── performance/
│
└── examples/
```

This is a **starting organization**, not an instruction to create empty directories or speculative abstractions before they are needed. Keep individual milestones small.

### Documentation roles

- `LANGUAGE_BIBLE.md`: Language syntax, semantics, rules, examples and confirmed design decisions.
- `ARCHITECTURE.md`: Compiler, runtime, GC, hot reload and native interoperability contracts.
- `DECISIONS.md`: Short records of major decisions, rejected approaches, evidence and revisions.

Documentation updates should accompany behavior changes, not lag behind for months.

---

## 8. Testing strategy

The original VOIDC 1.0 work showed why platform differences must be discovered early. VOID 2.0 should continuously validate both expected behavior and diagnostics.

| Test category | Cadence / expectation |
|---|---|
| **Focused tests** | Every milestone, targeting exactly what changed |
| **Linux smoke tests** | Every relevant milestone |
| **Windows smoke tests** | Every relevant milestone on a real matching toolchain/environment |
| **macOS smoke tests** | As soon as a suitable test host/toolchain is available |
| **Optimized-code correctness** | From the first LLVM stage, not only after release builds |
| **GC/thread stress** | After changes to object layout, safepoints, allocator, threading or live-code lifetimes |
| **Integration tests** | At the end of each milestone block, especially #010 |
| **Sanitizers and memory checks** | Regularly where supported by the chosen compiler/runtime configuration |
| **Benchmarks** | Establish baselines early; rerun after performance-relevant changes |
| **Diagnostic regression tests** | Throughout frontend and runtime development |
| **Release audit** | Before each releasable version |

### Cross-platform testing contract

- Prefer running **the same `.void` program** on Linux and Windows and comparing observable behavior where platform-independent results are expected.
- Do not confuse cross-compilation success with successful execution on the destination OS.
- Check compiler invocation, linker discovery, paths, Unicode, line endings, library loading, platform calling conventions and runtime shutdown behavior early.
- If a platform is temporarily unavailable, record that as **not tested**, not passed.
- Avoid accumulating dozens of platform-specific failures until a late final audit.

### Source diagnostics and runtime diagnostics

The compiler and runtime should report VOID source locations, not backend implementation details in ordinary operation.

Example compiler diagnostic:

```text
src/player.void:12:19: error: Cannot assign string to i32
    code: VOID2001

  12 | var health: i32 = "hello"
     |                   ^^^^^^^

     Expected: i32
     Received: string

     Hint: Use an integer value or parse a valid numeric string.
```

Example runtime diagnostic:

```text
src/units/player.void:27:26: runtime error: Index out of range
    code: VOIDR1003

  27 | var item = inventory[5]
     |                      ^

     Index: 5
     Valid indices: 0..2

     Hint: Check the list length or use a safe lookup.

Stack trace:
  at player.get_item(index: i32)
     src/units/player.void:27
  at game.update(dt: f32)
     src/main.void:42
  at main()
     src/main.void:10
```

The codes and file paths above are illustrative. The **format, source-first philosophy, and stack trace placement** reflect the intended design. The editor should underline the same spans and offer accurate quick fixes where possible.

---

## 9. LSP and extension strategy: begin in Stage 1

The completed editor extension is a later deliverable, but source information must be captured from the first frontend milestones.

Required shared information:

- Stable symbol identities and precise source spans.
- Type and generic information.
- VFS import paths and module relationships.
- Documentation comments such as `##` associated with fields, structs, methods and parameters.
- Completion candidates and visibility (`public` by default, explicit `private`/`internal`).
- Definition locations, references, signatures and rename support.
- The same diagnostic codes, ranges, explanations and hints in CLI and editor.

Example language source:

```python
## Represents a controllable player.
struct player:
    ## Current health points.
    health: i32 = 100

## Restores the player's health.
func (p player) heal(amount: i32):
    p.health += amount
```

When someone types `hero.`, completion should show documented members from actual VOID declarations, even if a library is backed by C. Developers should not see C headers or mangled native names in ordinary popup help.

Future package discovery should allow searching, inspecting, installing and documenting compatible packages through the extension, with helpful diagnostics for missing or unsupported native dependencies.

**Do not create a second independent language parser or type checker for the LSP.**

---

## 10. Performance strategy and acceptance criteria

Performance requires measured tradeoffs, not blanket claims that LLVM or a particular collector is automatically fastest.

Priority order:

1. **Correctness:** GC, error handling, threads and optimization preserve semantics.
2. **Execution speed:** Efficient native code without redundant barriers or helper calls.
3. **Allocation cost:** Avoid provably unnecessary allocations; make remaining small allocations cheap.
4. **GC latency:** Track average and worst-case pauses, including game frame-time consistency.
5. **Memory usage:** Monitor fragmentation, live heap, metadata and allocation overhead.
6. **Compilation speed:** Fast incremental development while permitting aggressive release optimization.

Potential mechanisms to evaluate:

- Precise initially non-moving mark-sweep GC.
- Per-thread allocation caches and reusable size-class storage.
- Inline `value struct` representation where safe.
- Escape analysis / scalar replacement where observable behavior is unchanged.
- Carefully placed necessary GC safepoints; do not insert redundant checks in every statement.
- Concurrency/GC thread coordination and low-contention allocation.
- More aggressive AOT optimizations and inlining in release builds.
- Development-mode indirect dispatch only where required for live code replacement.
- Later concurrent or generational GC work **only after measurements justify it**.

Do not optimize based on source-code aesthetics or comparisons against a C benchmark that omits VOID's safety and GC semantics. Benchmark comparable workloads and preserve checks/behavior or disclose differences clearly.

### Important distinction

- **VOID `struct`:** GC-managed, reference semantics; compiler may eliminate allocation when safe.
- **VOID `value struct`:** Value semantics and copy behavior; may live inline in registers, stack storage or larger managed objects.
- Field initializers such as `= 100` **do not determine stack versus heap allocation**.
- Safe VOID values are initialized. No exposure to uninitialized garbage data.

---

## 11. Milestone completion contract

A milestone may be called **locked** only when there is evidence for all applicable criteria:

- [ ] Scope is focused and the implementation matches the Language Bible.
- [ ] Code builds successfully in the relevant configurations without new unexplained warnings.
- [ ] All focused tests for the milestone pass.
- [ ] Relevant existing regression tests pass.
- [ ] Appropriate Linux/Windows checks pass, or unavailable platforms are explicitly documented as untested.
- [ ] Optimized-code correctness is checked when GC, LLVM lowering, arithmetic or code lifetime could be affected.
- [ ] Source and runtime diagnostics retain readable VOID locations and messages.
- [ ] Documentation and decision records are updated where behavior/architecture changed.
- [ ] No speculative abstractions, untracked temporary hacks or unnecessary duplicate paths are left behind.
- [ ] Performance changes include representative before/after measurement where relevant.
- [ ] Limitations and unresolved defects are reported candidly; a failed or unrun test is never presented as a pass.

**Block audit rule:** Every ten milestones, pause feature work and perform an integration audit. The milestone count is not itself the goal.

---

## 12. Architectural risks and review decisions

These are known design risks. They are not necessarily reasons to delay the first prototype, but they should be acknowledged and tested before they become difficult to change.

| Risk | Why it matters | Proposed handling |
|---|---|---|
| **Deep `const`/readonly semantics** | A readonly reference may still alias an object other code can mutate | Specify whether readonly access means immutable object or merely a nonmutating view; tests must match the Language Bible |
| **Root-level constants** | Automatically visible constants in `main.void` may create initialization dependencies | Separate true compile-time constants from runtime module initialization |
| **Struct initialization** | Field defaults, named arguments, `init()` and fallible constructors can conflict | Specify exactly one construction sequence; partially initialized objects never escape |
| **`value struct` GC references** | Inline values can contain managed references | Preserve precise embedded-reference metadata through copies and optimizations |
| **Closures** | Captured locals can outlive declaring frames | Runtime-managed closure environments and correct root tracking |
| **Generic specialization** | Monomorphization can inflate native code size | Choose specialization carefully and benchmark code size and performance |
| **Hot-reload versions** | Old code can remain active in threads, closures and suspended async functions | Stable identity, compatible updates, versioned metadata and safe retirement |
| **C string/handle ownership** | C headers rarely communicate full lifetime contracts | Explicit binding ownership metadata and automated safe conversions |
| **LLVM statepoint/version coupling** | GC metadata is deeply tied to emitted native code | Pin tested LLVM and build optimized-root correctness proofs early |
| **Cross-platform toolchains** | Linking, ABI, Unicode and OS behavior vary | Continuous Linux/Windows smoke tests; clear untested status for missing hosts |

### Current intended language/runtime contract (brief recap)

The full rules and examples belong to the Language Bible, but architectural milestones should preserve:

- `func` for functions, methods, callbacks and callable structs (`call()`).
- `struct` reference semantics; `value struct` value semantics.
- Public by default, optional `private` and `internal`, `static` for type-level members.
- `var` for mutable declarations and deeply readonly access through `const` (with aliasing semantics to be made exact).
- `[]` for growable or explicitly fixed-size lists (`[i32, 4]`), `{}` for dictionaries.
- `match`, `is`, `Ok`/`Err`, `result<t, e>`, error propagation through `?`.
- `defer`, `defer { ... }`, `with`, `init()` and deterministic `deinit()`.
- `async func`, `await`, `spawn`, task groups, cooperative cancellation.
- Automatic precise GC and native handles via bindings; no ordinary `unsafe` requirement.
- Hot reload on compatible body changes; manual restart for `const` values, layout or incompatible signature changes.
- VFS root and relative `use` paths with `/` across platforms.
- UTF-8 `string`, automatic interpolation and developer-friendly formatting.

The compiler's internal runtime and backend can be complex; that complexity should not leak into the language experience.

---

## 13. What to do first

**Start with VOID 2.0 #001: Compiler Foundation and LLVM Toolchain.**

Its responsibility is deliberately narrow:

1. Establish the new `Void.Language` repository's initial build and test structure.
2. Choose a working compiler implementation approach only after checking required LLVM interfaces.
3. Pin and document a specific LLVM/toolchain version.
4. Make the minimal compiler harness reproducible.
5. Prove basic local native builds and establish Linux/Windows smoke-test infrastructure.
6. Record what passed, what failed and what was not tested.

**Do not implement the entire parser, GC, async, native package ecosystem or hot reload in #001.** Each earns its own focused milestone.

**Do not begin #002 until #001's toolchain and portability assumptions are understood.**

---

## 14. Instructions for future conversations or milestone implementers

If this file is used after context has been lost:

1. Read this document and the **VOID 2.0 Language Bible** before proposing changes.
2. Treat confirmed decisions as constraints. Do not quietly replace VOID's chosen semantics with C#, Go, Rust, Odin or C conventions.
3. Distinguish **agreed language behavior**, **proposed implementation choices**, and **unresolved details**.
4. Do not assume the earlier VOIDC 1.0 implementation or a previously delivered ZIP is current source. Inspect the actual current `Void.Language` tree when implementation work is authorized.
5. Keep the project name `Void.Language`. The earlier project was backed up.
6. Follow KISS/DRY, focused milestone scope, periodic audits and source-level diagnostics.
7. Do not trade away GC, automatic safety, simple strings or other core guarantees to make synthetic benchmarks faster.
8. Do not claim a proof, platform test, successful hot reload or locked milestone without actual evidence.
9. If an implementation decision changes, explain the tradeoff and update the decision record and Bible.
10. **No automatic restarts:** hot reload applies compatible code changes, while layout/constant/signature changes wait until the developer restarts manually.

### Final architectural recommendation

**Proceed with the architecture, conditional on the four early proof-of-concept gates.**

Develop VOID 2.0 in two broad periods:

- **Architecture validation:** Prove LLVM AOT, optimized GC root tracking, safe live native-code replacement, and C interoperability.
- **Incremental implementation:** Build the documented language on those foundations, adding one tested feature at a time.

Every ten milestones, pause for an integration audit. Advance based on evidence and maintainability, not milestone count.

**A small language with a sophisticated runtime is the goal. Keep the developer experience simple even when the underlying implementation is hard.**

---

*Prepared as a portable Markdown companion to the VOID 2.0 Language Bible. It is a strategy and specification artifact, not an implementation-status claim.*

## Current execution policy: 10 October 2026

The planning snapshot above is preserved. The user's current instructions
supersede its small numbered milestone process: develop substantial cohesive
phases, complete related work and verification together, and record actual
results. The first phase combines project/toolchain setup with executable AOT,
ORC invocation, leased function replacement, explicit-root tracking and C ABI
proofs. This does not satisfy the full multi-thread GC/statepoint integration
gate and must not be described as a completed production runtime.

C++20 with the LLVM 19.1.7 C++ development SDK is now the implementation choice.
The canonical design companion is [LANGUAGE_BIBLE.md](LANGUAGE_BIBLE.md).
Build instructions are in the repository README; decisions, tests and remaining
work are maintained in [DECISIONS.md](DECISIONS.md), [WORK_LOG.md](WORK_LOG.md),
and [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

Work remains local. The user will create the public GitHub repository and make
the initial push. The local workflow targets standard Linux, Windows and macOS
hosts; no hosted platform result can be claimed until its executable tests run.


## Progress after source compiler and module phases

The user reports the first real source compiler passed Linux/Windows/macOS
Debug/Release and Linux sanitizers. The next cohesive phase implements a real
VFS/import graph, function ownership/visibility and multi-file native JIT/AOT;
see MODULE_SYSTEM.md, DECISIONS.md and WORK_LOG.md for its current evidence and
provisional path policies. Its expanded hosted matrix remains pending. The user
owns commits/pushes and hosted runs; compiler work remains local unless authorized.

The next recommended language phase strengthens const/project-root constants,
numeric policy/conversions and source-first runtime diagnostics. Module cycles
for functions do not settle constant initialization. Managed types still require
the separate runtime root/thread integration gate; packages and external imports
need deliberate identity/permission contracts.

### Current value-semantics phase, 10 October 2026

The user reports the preceding ten-suite module phase passed Linux, Windows and
macOS Actions. Constants, compile-time dependency analysis, numeric parity and
source-first native arithmetic diagnostics now extend the same frontend/IR/backend.
See VALUE_SEMANTICS.md and WORK_LOG.md for scope, exact verification and limits.
Recommended next work: diagnostic structure/recovery and dependency compatibility,
then deliberate contextual literal/conversion policy before wider numeric types.
Managed types still require the separate GC root/thread integration gate.

### Phase 5 diagnostic recovery, 10–11 October 2026

The user reports Phase 4 commit `d78b6b8`, Actions run #6, passed Linux, Windows
and macOS. Structured codes, byte spans, immutable snapshots, related declarations
and bounded lexer/parser/semantic recovery now extend that foundation. The complete
graph is validated before lowering, with invalid source excluded from native
execution, object publication and reload. See DIAGNOSTICS.md and WORK_LOG.md for
contracts, measured local verification and the pending Phase 5 hosted run.
Next design precise validated source/dependency compatibility and analysis-result
APIs before incremental IDE integration. Numeric expansion still needs deliberate
contextual literal/conversion decisions; managed types still require the GC gate.

### Phase 6 approved numeric language

The user reports Phase 5 commit 4f3688d passed hosted run #8, then approved the
numeric policy table before implementation. The full ten-type numeric family,
contextual literals, strict existing-value conversions and checked numeric as
extend the same compiler pipeline. See NUMERIC_TYPES.md, DECISIONS.md and
WORK_LOG.md. Preserve the separate GC integration gate and defer nullable casts,
managed types and arbitrary public ABIs. The next recommended cohesive phase is
validated analysis/dependency APIs and compatibility metadata for incremental
tooling. Phase 6 platform claims must come from its own hosted executable runs.

### Phase 7 validated analysis and dependency compatibility

Phase 6 commit `9b9fdac` passed hosted Actions run #9 according to the user.
Phase 7 exposes reusable owning successful analysis and exact compiler-derived
source compatibility through the same frontend. See ANALYSIS_COMPATIBILITY.md.
Next evaluate an in-process tooling consumer and invalidation planning before
incremental compilation or LSP transport. The GC/thread-root integration gate,
external package identity policy and arbitrary native ABI work remain separate.

### Phase 8 in-process tooling foundations

The user reports Phase 7 commit `04768bf` passed hosted run #10 on all platforms.
TOOLING_FOUNDATIONS.md documents the owned symbol-query index and deterministic
invalidation planner consuming existing validated analysis records. Old graph
plans remain conservative; exact source compatibility remains compiler-owned.
Next establish explicit client document/project revisions and invalidation
orchestration before partial/incremental analysis or LSP transport. Preserve the
managed-runtime/root integration and external-library identity gates separately.


## Phase 9 document revisions and tooling sessions

The user reports Phase 8 commit `f25b268` passed hosted run #11 on all platforms.
[TOOLING_SESSIONS.md](TOOLING_SESSIONS.md) defines owned whole-document overlays,
strict per-document logical revisions, project identity tokens, explicit close versus
unavailable transitions, complete analysis selection and stale-result acceptance.
Current queries never fall back to historical success after edits/failures. Dependency
orchestration reuses Phase 8 and the exact Phase 7 compiler contract; native
publication remains separate. Disk fallbacks are reread and checked during analysis,
but are not an atomic filesystem image; clients explicitly refresh for observed
disk changes. All sixteen preceding suites remain, with sessions/sessions_demo
bringing the total to eighteen. Actual local results are recorded in WORK_LOG.md.
Hosted Phase 9 verification remains pending. No LSP, watcher, worker, persistent
cache, partial compiler, new language feature or automatic native reload is added.
Next consider protocol position adaptation and a minimal tooling client/transport
before partial compilation; retain the separate managed-runtime and package gates.


## Phase 10 minimal language server

The user reports Phase 9 commit `02a9aa7` passed hosted Actions run #12 on Linux,
Windows and macOS. Phase 10 connects that immutable session foundation to real
stdio LSP clients: full-document edits, current diagnostics, definition and
references, using exact UTF-16 snapshot ranges. [LANGUAGE_SERVER.md](LANGUAGE_SERVER.md)
documents its actual scope. Three portable position/process suites bring CTest
to twenty-one without changing the eighteen existing suites or language semantics.
See WORK_LOG.md for local Debug/Release/strict sanitizer evidence from both SDKs;
a new user-triggered hosted run is still required.

Next validate hosted integration, then consider a small editor client/extension
and an explicit disk refresh/cancellation policy before asynchronous or partial
analysis. Preserve exact compatibility and source ownership when designing workers;
do not mistake dependency planning for an implemented incremental compiler.
Completion/hover/rename, caches, watchers and automatic native reload remain
deferred. Managed-runtime integration and package identity remain separate gates.
The user retains control of all commits, branches, pushes and publication.
