# G82 — Generic structural interfaces

Baseline: `fa8e9958503ced757cdc3ba30705c363fff0733e` (G81).
Linux candidate, 20 November 2026. Independent Golden Wrench review and Boss
integration authorization remain pending. Nothing staged, committed, pushed,
installed or published.

## Implemented contract

`interface reader<t>:` specializes through the existing nominal generic declaration
machinery. Closed requirements substitute exact ordered parameter/result types,
const qualifications and nullability. Imported/nested applications retain their
original owner; identical arguments reuse one identity. Distinct specializations
are invariant, even with identical layouts. Bare generic names require arguments.

Existing accessible managed methods satisfy specialized requirements structurally,
including inheritance/virtual dispatch and unambiguous one-hop composition. File
privacy, internal visibility, import ownership and readonly effects retain their
existing rules. An authorized factory may expose a private provider; arbitrary
consumers cannot establish conformance through inaccessible provider methods.
Value providers can satisfy compile-time constraints without acquiring erased
value boxing.

Typed interface arguments anchor ordinary generic inference. Explicit type
arguments permit contextual concrete-to-interface conversion. Inference does not
search arbitrary concrete methods to guess interface arguments, and constraints
alone do not infer otherwise unbound parameters. Dependent constraints work on
functions, generic structs/interfaces and generic-owner methods, including
self-referential constraints. Requirement methods remain nongeneric and bodyless.

Nullable views, nullable payload arguments, collections, nested generic values,
multiple returns and bound callbacks use existing lowering and ownership. There
is no new Runtime representation: the object/witness pair, managed identity,
precise roots, conservative effects, generation leases and reload checks remain
shared. No backend, native ABI, GC implementation or security policy was changed.

Compiler-owned tooling publishes symbolic interface/requirement declarations and
concrete use-site signatures separately. Hover, documented parameter help, active
parameter selection, completion, source navigation, references, document symbols,
semantic tokens and invalid-to-valid edits cover generic interfaces.

## Implementation decisions

- `compiler.cpp` parses interface parameters and retains template requirements
  alongside existing generic syntax. A closed requirement's synthetic receiver
  points at its actual interface definition; it is not a user variable.
- `generics.inc` reparses through the ordinary Parser, uses canonical generic
  identities and adds closed requirements to the existing interface catalog.
  Constraint syntax is rebound through the same type parser with owner and method
  bindings; no parallel type substitution language was added.
- Requirements discovered in other requirement signatures are prepared by
  draining the existing declaration lists. Stored specialization depth and
  diagnostic context follow each header. Constraints run after the required
  headers exist, through the existing pending-constraint mechanism. Reentrant
  `has` queries finish their dependency headers, and actual header cycles retain
  the existing diagnostic. Validation-frame callbacks are retired at completion.
- Generic declaration checks preserve requirement visibility, exact signatures,
  malformed-header diagnostics and the exclusion of receiver/static
  implementations on interfaces. Nullable source payload visibility is checked
  rather than the private nullable storage wrapper, in both frontend and the
  independent IR validator. Genuinely private payloads still diagnose.
- `generic_tooling.inc` records interface/method kinds, source provenance,
  symbolic type parameters and documented signatures, including multiple returns.
  Concrete calls retain their resolved types without replacing template hover.

The runnable example is [generic.void](../../examples/interfaces/generic.void).
The canonical guides are [interfaces](../INTERFACES.md#generic-interfaces-g82),
[generics](../GENERICS.md#specialized-interface-parameters-and-constraints) and
[Language Bible Section 13](../LANGUAGE_BIBLE.md#13-interfaces-agreed-phase-21-bounded-implementation).
The original Phase 21 proposal remains historical, with a superseding link.

## Bounded upstream research

- [C# generic interfaces](https://learn.microsoft.com/en-us/dotnet/csharp/programming-guide/generics/generic-interfaces)
  and [type identity](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/types):
  closed constructed identity and substituted constraints are applicable. VOID
  retains implicit structural conformance and invariance; C# nominal implements
  declarations and variance were not imported.
- [Rust trait objects](https://doc.rust-lang.org/reference/types/trait-object.html)
  and [Swift SIL witnesses](https://github.com/swiftlang/swift/blob/main/docs/SIL/SIL.md):
  concrete conformance/witness dispatch supports reusing VOID's existing pair.
  No Rust/Swift Runtime or associated-type system was introduced.
- [C# nullable value types](https://learn.microsoft.com/en-us/dotnet/csharp/fundamentals/null-safety/nullable-value-types):
  source nullability belongs to the underlying type. This informed the visibility
  distinction; VOID's existing nullable representation remains authoritative.
- [Clang PR 88546](https://github.com/llvm/llvm-project/pull/88546), merged as
  `4082a7554521572a65a5a0008c4661a534df659d`, describes excessive native stack
  consumption during instantiation. Clang's
  [pending instantiations](https://github.com/llvm/llvm-project/blob/main/clang/lib/Sema/SemaTemplateInstantiateDecl.cpp)
  and [depth context](https://clang.llvm.org/doxygen/structclang_1_1Sema_1_1InstantiatingTemplate.html)
  are applicable patterns. VOID uses its own existing declaration lists and depth
  bound; no LLVM patch, stack increase or dependency was needed. Online upstream
  source is reference material, not evidence that VOID itself passes.

## Preserved development failures and corrections

Raw evidence is `/tmp/g82-3qde01uj`. Earlier successful results describe their
then-current inputs; the final qualification below supersedes them for this source.

- `baseline-failure.json` retains the original G81 syntax rejection. The accepted
  HEAD and initial clean tree are in `baseline.json`.
- The initial generic receiver was mistakenly treated as a user declaration,
  causing a shadowing diagnostic. Synthetic interface receivers now stay out of
  user-variable header validation.
- `constraints-first.log` and `constraints-backtrace.log` retain
  `bad_function_call`: reparsing an already checked constraint had left an unused
  unresolved annotation to be resolved after callback teardown. Instantiation
  does not republish that duplicate syntax; the separately checked bound syntax
  remains available.
- `source-seventh.json` retains the nullable storage visibility rejection;
  `source-eighth.json` retains the independent IR rejection after the frontend
  correction. Both validators now check the actual payload's visibility.
- `expanding-first.json` retains SIGSEGV under 1,048,576 bytes for an expanding
  requirement graph. `expanding-backtrace.log` and `expanding-frames.log` show
  repeated specialization/header frames: approximately 8,992 bytes in structure
  specialization, 9,424 in header preparation, plus resolver frames; parsing adds
  a roughly 60,544-byte frame. Iterative declaration preparation removes native
  recursion per interface and retains the existing specialization limit.
- `self_constraint-second.json` retains the separate SIGSEGV from checking a
  self-referential interface constraint before publishing its closed identity and
  requirements. Deferred checks reuse that identity after header preparation.
- `has-default-valid-first.json` retains an intermediate false result for a
  matching provider queried in a nested value default. Header dependency
  completion now yields `true`; the missing-provider control remains `false`.
- Test development mistakes are preserved too: incorrect `import` spelling,
  numeric-literal signature cursor positions, an incorrect rejection expectation
  for an owner-file private method, an exact allocation count that omitted the
  string object, a reserved `reverse` test identifier and two test/build spelling
  errors. These were fixture/assertion/build corrections, not language changes.
  `build-strict-graph-final.log` retains a task-owned build interrupted after the
  later default-query defect was found. Final builds completed separately.

## Final local qualification

| Final-source gate | Debug | Release | Fatal ASan/UBSan + leaks |
| --- | --- | --- | --- |
| Selected CTest suites | 23/23 | 23/23 | 23/23 |
| Generic interface source/application checks (within CTest) | 48/48 | 48/48 | 48/48 |
| Additional repeated graph checks, 1 MiB | 30/30 | 30/30 | 30/30 |
| Generic interface LSP metadata/recovery | Passed | Passed | Passed |

Extension unit tests: **14/14**. Final real VS Code **1.141.0: 10/10**;
VSCodium **1.143.1: 10/10**. Both final editor processes exited 0.
`*-final-results.json` lists the ten actual observations in each editor.
Final suite wall times were 58.84 s Debug, 25.24 s Release and 87.04 s strict
(the qualification JSON retains exact durations). These are qualification run
times under concurrent work, not comparative performance benchmarks.

`git diff --check` passes. The index remains empty and HEAD remains the accepted
G81 baseline. All 22 paths belong to G82; no generated binaries or packages are
in the candidate.

The selected suite list and exact commands are in `selected-tests.json` and
`*-complete-qualification.json`. The suites cover generic interfaces and existing
interfaces, interface AOT O0/O2, native generics and inference, imported generic
owners, nested static generic results, nullable behavior/ownership corrections,
shift patterns, frontend/tooling, semantic tokens, editor recovery and the
original source/async/global/depth stack contracts. This is proportionate selected
qualification, not a complete repository run.

`generic_interfaces_source` checks imports, exact/rejected signatures, distinct
specializations, inherited/promoted providers, nullable views/payloads, aggregate
and collection payloads, callbacks, multiple returns, constraints, privacy,
readonly authority, recursive requirements, a finite 96-link chain, a finite
swapped-argument cycle and invalid expansion. The main fixture executes uncached
JIT and independently built AOT at O0/O2. All its compiler/application children
retain the 1 MiB stack limit on Linux. The LSP test also launches its server with
that limit, and checks symbolic/concrete hover, completion, documented signature
help, navigation/references, interface tokens/symbols and versioned repair.

`interfaces_source` adds closed generic witness identities, storage-free
interfaces, managed cycles, interface-containing value aggregates, strings,
bound callbacks, precise roots under forced collection, failure cleanup/retry,
independent Runtime owners, retained old generations, compatible body replacement
and rejected changed requirements. Existing interface admission, active-generation,
IR-corruption and ownership checks also run unchanged.

Fatal policy is unchanged:
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`, with no
`LSAN_OPTIONS` override. No raised stack limits, disabled assertions, weakened
expected failures or increased test timeouts were used. Existing compiler build
warnings remain in the logs; warnings are not claimed absent.

Actual editors use existing Linux test copies, the development extension and the
matching Release server, with isolated task profiles/settings. Provider checks
and parameter/suggestion UI triggers are recorded; no pixel-level visual audit,
latency benchmark or cross-platform result is claimed. User installations and
settings remain untouched.

## Identity, limits and handoff

`final-source-identity.json` records source/test SHA-256 values, accepted HEAD,
profile cache configuration and LLVM/Clang/LLD 23.1.4 identity. Final qualification
JSON records actual compiler/server/interface-test artifact hashes. Original and
final patches, source inputs, diagnostic outputs and editor observations remain
outside Git. Selected records and all 22 candidate inputs are also retained in
`/home/shmellyorc/Documents/Void.Language-archives/g82-generic-interfaces-2026-11-20-3qde01uj.zip`;
the adjacent `.index.json` records CRC, extracted-member SHA-256 verification and
the ZIP hash. Original raw evidence is retained; no binaries or editor profile
copies are included. No performance benchmark is claimed; dispatch representation and
Runtime algorithms remain unchanged, with ordinary compile-time specialization
work for each closed interface.

Windows and macOS native execution remain **UNQUALIFIED**. No hosted CI,
installation, extension publication or independent acceptance is claimed.
Interface inheritance/defaults, variance, method-generic requirements, standalone
value boxing and new dynamic casts remain excluded. Candidate is for Golden
Wrench's independent review before Boss-authorized integration.

## Exact changed-file inventory

22 paths (13 modified, 9 new):

- `CMakeLists.txt`
- `docs/DEVELOPMENT_STRATEGY.md`
- `docs/GENERICS.md`
- `docs/INTERFACES.md`
- `docs/INTERFACES_PROPOSAL.md`
- `docs/LANGUAGE_BIBLE.md`
- `docs/README.md`
- `docs/projects/G82_GENERIC_INTERFACES.md`
- `editors/vscode/src/test/genericInterfacesHost.ts`
- `examples/interfaces/generic.void`
- `src/frontend/compiler.cpp`
- `src/frontend/generic_declarations.inc`
- `src/frontend/generic_tooling.inc`
- `src/frontend/generics.inc`
- `src/ir/module.cpp`
- `tests/generic_interfaces/api.void`
- `tests/generic_interfaces/editor.void`
- `tests/generic_interfaces/lifecycle.void`
- `tests/generic_interfaces/lsp.py`
- `tests/generic_interfaces/main.void`
- `tests/generic_interfaces/source.py`
- `tests/interfaces/tests.cpp`
