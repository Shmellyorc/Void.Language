# Current architectural decisions

Current contracts govern implementation. This ledger records established owner
choices; it does not authorize new features, staging, publication or another stage.
The original detailed decision history is retained in
[the recorded snapshot](https://github.com/Shmellyorc/Void.Language/blob/df33af0a7e8ae75b97d2ab27395a683b69d3994f/docs/DECISIONS.md).
Earlier approvals describe their original scope, not blanket approval of later
engineering. The current assignment suspends publishing implementation.

## Approved architecture and semantics

| Owner-approved choice | Current contract and rationale |
| --- | --- |
| One frontend, typed IR, shared LLVM JIT/AOT and coordinated Runtime | [Compiler](SOURCE_COMPILER.md), [GC](GC_RUNTIME.md). Preserve one semantic authority and independent IR validation. |
| LLVM/Clang/LLD 23.1.4 | [Toolchain](SOURCE_COMPILER.md#active-toolchain), [migration](llvm23/COMPATIBILITY.md). Older pins are historical; changing versions does not establish historical crash causes. |
| Value copies versus managed identity; deep readonly authority | [Values](VALUE_SEMANTICS.md), [reference-containing values](REFERENCE_VALUE_STRUCTS.md). Preserve alias/capability distinctions across calls, collections and initialization. |
| C1/S1 initialization and private construction views | [Initialization](INITIALIZATION.md), [managed initialization](MANAGED_INITIALIZATION.md). Complete values precede publication; incomplete self cannot escape. |
| Managed deterministic destruction; existing with/defer ordering | [Resources](ADVANCED_LANGUAGE.md), [initialization](MANAGED_INITIALIZATION.md). Value copies have no shared once-only destruction identity. This approves the original bounded model, not a permanent ban on a future ownership design. |
| U1–U7 stored composition, direct-member precedence and one-hop promotion | [Composition](COMPOSITION.md). Stored identity, unambiguous selection, visibility and qualified access are preserved without synthesized forwarding or a transitive search. |
| I1–I8 single managed inheritance and exact virtual families | [Inheritance](INHERITANCE.md). Original member ownership, exact overrides, private layout and conservative effect families remain authoritative. |
| Structural interfaces and concrete generic constraints | [Interfaces](INTERFACES.md), [generics](GENERICS.md). Views preserve managed owner identity; generic interfaces do not imply value boxing or open-world ABI. |
| Ordered dictionaries and bounded data-only keys | [Dictionaries](DICTIONARIES.md), [equality](COLLECTION_EQUALITY.md). Equality/hash consistency, stable stored key values and GC tracing are required. The original bounded key policy was approved; later custom hashing needs its own contract. |
| Separate standard/project identities, exact imports and compiler-owned editor semantics | [Modules](MODULE_SYSTEM.md), [server](LANGUAGE_SERVER.md). Immutable source snapshots and the same resolver own compilation and assistance. |
| Ordinary named/default calls and exact numeric semantics | [Call/numeric contract](STAGE8_CALLS_NUMERIC.md), [numbers](NUMERIC_TYPES.md). Arguments evaluate in written order; defaults are pure templates; no implicit widening. |
| Synchronous application entry, explicit i32 returns and no import-time executable statements | [Application workflow](STAGE9_APPLICATION_WORKFLOW.md), [CLI](CLI.md). The old --app option is removed. Project output is under .void/build; explicit output retains caller-relative semantics. |
| Structured async, cooperative cancellation and bounded worker transfers | [Async](ASYNC.md), [retention](STAGE8_ASYNC_RETENTION.md). Owners drain children before scope/root retirement; transfer proofs preserve roots, effects and generation leases. |
| Runtime globals | [Globals](RUNTIME_GLOBALS.md). Dependency-first initialization, failure without replay, persistent compatible reload state and worker restrictions supersede old constant-only module exclusions. |
| Private/readonly fields, display, formatting and custom operators | [Fields](stage10/FIELD_ACCESS.md), [formatting](stage10/DISPLAY_FORMATTING.md), [operators](stage10/OPERATORS.md). Current contracts supersede string-only and all-public-field historical exclusions. |
| Explicit project/native consent, typed C boundaries and verified package inputs | [Projects](PROJECT_ARCHITECTURE_ROADMAP.md), [native libraries](NATIVE_LIBRARIES.md). Metadata alone grants no execution authority; preserve verified payloads and native lifetimes. |
| Project artifacts versus engineering retention | [Storage](PROJECT_STORAGE.md), [archive policy](projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md). Generated engineering output stays outside source/docs; maintained fixtures and required metadata remain source controlled. |

## Restrictions without a demonstrated permanent prohibition

No explicit Boss decision was found permanently prohibiting weak references,
generators, async destructors, full incremental compilation or broader constant
operations. Original exclusions bounded earlier engineering; they do not make
these current requirements. Detached tasks require a new lifetime/error owner;
module-time patterns need constant-pattern semantics; automatic application reload
needs a running host and state/restart contract. Specialized section tooling is an
approved organizational-comment direction, with implementation still absent.

Rename remains limited by proven symbol identity and transactional version-safe
edits. Broader categories are technically possible; existing guards remain required.
The current pure constant evaluator's limits protect deterministic work and the
supported stack; excluded pure operations are not all fundamentally impossible.

Any expansion needs an explicit product decision and reviewed semantics. This
cleanup neither implements an expansion nor silently abandons it. See
[known issues](KNOWN_ISSUES.md) and each current feature contract.
