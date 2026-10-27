# JSON research: source audit and feasibility


Implementation authorization, 27 October 2026: Boss approved the complete
contract and all three decision bundles. Earlier proposal/approval wording is
historical. The explicit `use json as json` example route was subsequently
approved to preserve the existing no-shadowing rule. See [current status](IMPLEMENTATION.md)
and [the public API](API.md).
27 October 2026. Audited baseline: **`194603ea00eb38aff72b7d6e950865b3a23df92a`**.
Research and documentation only. JSON is **not implemented or approved** by this
report. [Proposed contract](DESIGN.md); [engineering roadmap](ROADMAP.md).

## Recommendation

Build one `stdlib/json.void`: an ordinary source parser, dynamic tree, numeric
policy and writer. Use existing generic specialization to generate typed adapters
for explicitly bounded, public data shapes. The proposed names are `parse` for
dynamic parsing and `parse_as<t>` for typed parsing; `stringify` works on either.
Keep exact number lexemes until a caller requests a concrete numeric type.

Automatic typed conversion is the substantial compiler task. It is feasible by
generating ordinary checked functions, following the existing display adapters;
it is **not** already supplied by generics or Runtime reflection. Two directions
of conversion, recursive type discovery, construction, readonly propagation and
bounded execution all need engineering and tests. Do not describe this as one
trivial intrinsic. The alternative is fully source-level, explicitly written
DTO conversion functions; it is simpler but cannot deliver automatic conversion
of arbitrary ordinary structs. Boss must approve the chosen boundary first.

A reproduced readonly source/IR discrepancy is a dependency of the proposed
dynamic API. Its evidence and limits appear below. Do not implement a JSON-only
authority bypass to conceal it.

## Verified foundation

References and line numbers refer to the audited baseline. Historical Bible
examples are design history where a newer contract/source establishes a boundary.

| Area | Actual capability and consequence | Evidence |
|---|---|---|
| Text | One immutable, strict UTF-8 string; NUL survives. Owned UTF-8 bytes and checked decoding already exist. Parse bytes once, rather than repeatedly scalar-indexing a string. | [UTF-8 validator](../../include/void/utf8.hpp), [string contract](../STAGE8_TEXT_COLLECTIONS.md), [string lowering](../../src/backend/llvm/backend.cpp), [text helpers](../../stdlib/text.void) |
| JSON syntax | No JSON module or parser exists. VOID string literals have interpolation and `\u{scalar}` escapes, which are different from JSON escapes. Neither the source lexer nor text splitting is a JSON parser. | [strings.inc:1](../../src/frontend/strings.inc), [stdlib inventory](../../stdlib/README.md) |
| Numbers | All eight fixed-width integers, f32/f64, checked casts, wrapping integer arithmetic. No public arbitrary-precision number type. Ordinary `as` can truncate floats, so it is unsuitable for JSON integer validation. | [numeric contract](../NUMERIC_TYPES.md), [numeric.cpp](../../src/numeric.cpp) |
| Conversion | `convert.parse<t>` covers numeric primitives, bool and payload-free enums. It accepts a broader decimal grammar than JSON. Integer targets reject fractions/exponents; unsigned parsing rejects a minus sign, even negative zero. Float range/nonzero-underflow failures are recoverable. | [convert facade](../../stdlib/convert.void), [private binder:1](../../src/frontend/stdlib_conversion.inc), [Runtime conversion:13–87](../../src/runtime/conversion.cpp) |
| Display | Existing Ryu-backed default numeric display emits shortest round-trip finite decimals, retaining signed zero. It also emits `nan`/`inf`; JSON must reject those before display. Enum display is qualified; JSON names need their own explicit policy. | [format.cpp:112](../../src/format.cpp), [format ABI](../../include/void/format.hpp) |
| Lists | Growable lists share identity; fixed arrays copy. Both have checked get and independent slices. Generic `[t]` does not accept `[t,n]`. No public reserve/builder is necessary for ordinary push growth. | [array operations](../ARRAY_OPERATIONS.md), [generic inference](../../src/frontend/generic_inference.inc) |
| Dictionaries | Insertion order, exact UTF-8 string keys, contains/get/upsert and version checks exist. Hashing is unkeyed FNV-1a with linear probing; there is no adversarial-input performance guarantee. | [dictionary contract](../DICTIONARIES.md), [dictionary.cpp:44–59](../../src/runtime/dictionary.cpp) |
| Recursive data | Managed references and growable indirection break inline layout cycles. A managed node with an enum containing `[node]` / `{string:node}` checks and executes. A directly inline recursive enum diagnoses. | [reference values](../REFERENCE_VALUE_STRUCTS.md), capability probes below |
| Generics | Specialization-time checking, nominal identities, concrete fields, caching and original declaration navigation exist. Return-only `t` needs an explicit argument; duplicate generic/nongeneric function names diagnose. No general overload sets or source field-enumeration operation exists. | [generics contract](../GENERICS.md), [generics.inc:26](../../src/frontend/generics.inc), [generic declarations](../../src/frontend/generic_declarations.inc) |
| Nullability | Nullable managed references work. Nullable scalar/string types and nullable arrays are not generally available; generic `t?` is explicitly rejected. Existing Bible nullable-list examples are not current source capability. | [compiler.cpp:830–864, 1384–1402](../../src/frontend/compiler.cpp), probes below |
| Visibility | Stored-field access is declaring-owner based; being in the same module does not grant arbitrary private-field access. Methods/init can own backing-field access. Constructor calls and properties use existing access checks. | [constructor checks:1980–2055](../../src/frontend/compiler.cpp), [construction analysis](../../src/frontend/construction.inc), [time facade](../../stdlib/time.void) |
| Compile-time metadata | `Type`/`ValueType` expose concrete fields, owner, visibility, readonly, cases, collection element/extent and base. They can drive a compile-time adapter. Properties/initializers live in the frontend/member catalog; field layout alone is insufficient. | [ir.hpp:19–85](../../include/void/ir.hpp), [member selection](../../src/ir/interfaces.inc), [frontend compiler](../../src/frontend/compiler.cpp) |
| Runtime metadata | `NominalDescriptor` and `ElementSchema` describe identity, layout, roots and physical data. They are not a public field/property/constructor reflection API. Do not extract application objects by walking GC bytes. | [heap.hpp:23–41](../../include/void/heap.hpp), [gc_schema.hpp:21–114](../../include/void/gc_schema.hpp), [schema validation](../../src/runtime/gc_schema.cpp) |
| Generated functions | Display already generates ordinary AST functions, typed per-owner heap stacks, scalar state and continuations. It uses the ordinary checker/lowerer. Its output syntax, cycle placeholders and Runtime display context are not JSON. | [display adapter](../../src/frontend/display_semantics.inc), [collection_display.inc:1–109](../../src/frontend/collection_display.inc) |
| Errors | `core.error` is managed; stdlib modules derive their own exact error types. `?` propagates compatible result error types, not arbitrary IO/JSON error conversions. Resource exhaustion retains the existing checked failure channel. | [error.void](../../stdlib/error.void), [convert.void](../../stdlib/convert.void), [memory stream](../../stdlib/io/memory.void) |
| GC/execution | Ordinary objects, enum inactive slots, lists, dictionaries and generated calls use existing precise roots, safepoints, construction and generation leases. No new native resource is required. | [IR validator](../../src/ir/module.cpp), [backend](../../src/backend/llvm/backend.cpp), [Runtime](../../include/void/runtime.hpp) |
| Tooling/install | Source generics/docs already feed compiler-owned completion, hover, signature help and navigation. Math repaired incomplete generic signature recovery. Installation recursively copies `.void` modules. | [generic_tooling.inc](../../src/frontend/generic_tooling.inc), [tooling](../../src/frontend/tooling.cpp), [hover contract](../DOCUMENTATION_HOVER.md), [CMakeLists.txt:190](../../CMakeLists.txt) |

Read alongside the [stdlib handoff, section 10](../VOID_STDLIB_AND_GLOBAL_VARS_WRENCH_HANDOFF_2026-10-24.md#10-json-csv-toml-xml-and-dynamic-lookup)
and [Wrench workflow](../WRENCH_CLANKER.md#permanent-workflow-for-every-new-development-stage).
Math is implemented at this baseline. Its [committed handoff](../math/HANDOFF.md)
records effective local 93/93 outcomes after retained repairs/reruns. Boss reports
independent Golden Wrench source review and identical strict reruns outside the
sandbox. This does not establish hosted/native Windows/macOS acceptance.

## Bounded capability experiments

No compiler, library or test source was changed. Scratch fixtures were created in
`/tmp/void-json-capability-8__hqmmk`; the complete original source/output/incident
archive and its hash are in [the evidence receipt](evidence/RECEIPT.md).
Release compiler SHA-256:
`a892191eda109d6c212d0f6f247aa2979d8307f7933e5c63220bf39df81a6275`.
Existing Debug and Release binaries were used without rebuilding; the receipt
identifies them. These are binary observations plus a baseline source audit, not
a newly frozen/rebuilt acceptance claim.

All 37 compiler invocations used an actual 1 MiB child stack and bounded timeouts.
Twelve JIT executions passed: five small fixtures in normal/O2 Release, plus a
generic readonly identity in Debug/Release normal JIT. Cross-module generic
named construction of public const fields passed check and both JIT modes.
These establish representation/construction feasibility, not JSON correctness,
large-stack safety, sanitizers or linked AOT acceptance.

Negative probes confirmed no overload sets, no inference of return-only `t`, no
general field enumeration, incompatible fixed/list generic shape, rejected
nullable scalar/string/list types and rejected inline recursion. Preserve the
initial fixture mistakes: module functions attempted private-field construction,
one string used undoubled interpolation braces, and a nullable-list expectation
was incorrect. Follow-ups are distinct files; originals were not rewritten into
successful experiments.

### Confirmed source/IR discrepancy; cause not established

This source checks successfully with the available Release compiler:

```void
use error as core
struct node:
    private children: [node]
private func (self node) init(children: [node] = []):
    self.children = children
func (self node) get() -> result<node, core.error>:
    return Ok(self.children[0])
func read(const value: node) -> result<node, core.error>:
    return value.get()
func main():
    var leaf = node()
    const root = node([leaf])
    if read(root) is Ok(var child):
        print("readonly result accepted")
```

Execution fails **before JIT invocation** with status 1:
`VOID IR: call requires mutable reference authority in /readonly_result_corrected.void::read`.
Normal and O2 Release both fail. Removing the forwarding function and calling
`const root.get()` directly fails in Debug and Release. Explicitly pattern-unwrapping
and rebuilding the result in the forwarding function also fails in both profiles.
Six unexpected IR failures were preserved as six original incident archives,
including exact `_crash.void` copies, before further probing. There was no signal,
timeout or sanitizer run; do not call this a reproduced SIGSEGV.

The rejection is emitted at
[reference_capabilities.inc:249–265](../../src/ir/reference_capabilities.inc),
when the independently inferred `required_mutable` effect reaches a readonly
origin. Frontend [capabilities.inc:39–110, 240–275](../../src/frontend/capabilities.inc)
and IR [sum/array/get/return analysis](../../src/ir/reference_capabilities.inc)
must agree. The experiment does **not** identify which inference/lowering step
introduced the mismatch. A plain generic `identity<const input>` returning a
managed reference succeeds; this is narrower than a blanket inability to return
readonly references. A direct private-field access from a module function rejects
normally, as required.

Before engineering JSON, confirm against a frozen current compiler, reduce the
result/list/receiver interaction and repair only the demonstrated discrepancy.
Readonly child lookup must preserve authority through `Ok`/`?` and cannot be
implemented by copying into mutable parameters or weakening IR validation.
The roadmap treats this as a focused prerequisite, not a second language project.

## Architecture alternatives and recommendation

| Approach | What it delivers | Cost/limitation | Judgment |
|---|---|---|---|
| Ordinary source only, explicit application DTO mapping | Complete JSON syntax/dynamic lookup; application code calls checked accessors and constructors | No automatic struct-field discovery or arbitrary `stringify(t)` | Safe fallback if Boss declines typed compiler work; label the missing automation honestly |
| Narrow compile-time adapters | Automatic conversion for admitted data shapes, one source DOM/parser/writer | Two generated traversals, finite type-graph discovery, construction/access/effect checks and tooling/cache coverage | **Recommended for the requested typed experience**, with the exact exclusions in DESIGN |
| Runtime metadata walker | Would require field-name/type/access/constructor metadata and safe heterogeneous ownership | Existing GC metadata cannot supply those semantics; broad new ABI/reflection surface | Reject for this sprint |
| Public reflection, general overloads, external serializer/native provider | Much broader language/dependency scope | Unnecessary to implement JSON; new ownership/ABI/editor risks | Reject |

The private bridge should select concrete adapters, not perform runtime parsing
or type dispatch in C++. Put public source policies in one module. Reuse AST,
typed slots, roots, collection operations, member access and construction; use
generated display code as precedent, not a second serializer. Share only a small
existing helper if actual duplication appears during engineering. No requirement
to introduce a general adapter-generation framework.

Typed traversal must use heap work/continuation stacks, including recursive
managed data and postorder construction. A per-type recursively called helper
with a depth check is not evidence of safe native stack consumption: argument
frames/large fixed values can allocate before that check. The existing display
state-machine pattern is a concrete route to depth-independent native frames.

Cycle handling is deliberately bounded traversal failure, not a new identity
service. Managed pattern equality already bypasses custom operators, but
`pattern_comparable` excludes collections and ordinary collection `==` is not
admitted by IR ([module.cpp:88, 1176](../../src/ir/module.cpp)). The recommended
depth/node/work limits reject cyclic expansion as `limit_exceeded`, also used
for overly deep acyclic values. Do not promise a distinct cycle classification
or reuse Runtime display contexts for this purpose.

For arbitrary user invariants, recommend explicit application conversion through
a public data DTO and its existing factory. Automatically invoking setters,
custom init or private fields is not part of the initial automatic contract.
No magic `from_json`/`to_json` method lookup, attributes or hook protocol is needed.
DTO mapping lets those types participate without granting private access.

## Correctness research and policy choices

These are VOID recommendations, not claims of current implementation.

- RFC 8259 allows a top-level scalar, decimal exponents and implementation limits.
  Duplicate-name behavior varies; generators must emit valid JSON. Our strict
  grammar rejects comments, trailing commas/input, nonfinite literals and a BOM;
  parser limits are explicit. [RFC 8259 §§2–10](https://www.rfc-editor.org/rfc/rfc8259)
- RFC 8259's grammar can admit isolated surrogate escapes; accepting those would
  conflict with VOID scalar strings. Reject malformed pairs without replacement.
  I-JSON additionally excludes Unicode noncharacters and constrains numerical
  interoperability. This proposal retains VOID-valid noncharacters and exact
  numeric text, so it is not advertised as full I-JSON.
  [RFC 7493](https://www.rfc-editor.org/rfc/rfc7493)
- Unicode scalar values exclude surrogates, but include unassigned code points and
  noncharacters. Reuse strict decoding and arithmetic UTF-8 emission; do not add
  normalization or a Unicode database for JSON.
  [Unicode UTF FAQ](https://www.unicode.org/faq/utf_bom.html)
- Stable insertion/declaration order is sufficient for reproducible ordinary
  output. It is not cryptographic canonicalization: JCS has additional sorting and
  numeric rules. No signing/hash guarantee follows from `stringify`.
  [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785)

Mature comparisons support explicit choices rather than copying defaults:

| Primary reference | Relevant observation | VOID consequence |
|---|---|---|
| [Serde derive](https://serde.rs/derive.html) | Typed conversion can be generated; applications can provide explicit implementations for difficult shapes | Compile-time generation is credible, but VOID has no derive mechanism to assume already exists |
| [serde_json Value](https://docs.rs/serde_json/latest/serde_json/value/enum.Value.html) | Six dynamic forms; checked access differs from indexing that returns Null for absent entries | Use a real null variant and result lookup; absence must not become null |
| [System.Text.Json source generation](https://learn.microsoft.com/en-us/dotnet/standard/serialization/system-text-json/source-generation) | Metadata-based and generated serialization paths are explicit facilities | Do not conflate compiler-known type information with an existing Runtime serializer |
| [Go encoding/json](https://pkg.go.dev/encoding/json) | Documented v1/v2 differences include UTF-8 replacement versus error, duplicate rejection, fixed-array length checks and ordering | Pin VOID's own strict behavior in tests; a host library's defaults are not the contract |

Numbers should not be eagerly converted to f64: that would lose large integer IDs
and make accepted dynamic documents depend on machine float range. Store validated
tokens, including `1e400` and `-0`. Integer extraction normalizes decimal digits and
scale with bounded work before range checking; float extraction delegates to the
existing matching-width converter after JSON grammar validation. Nonzero rounding
to zero and overflow are errors. No silent float-to-int truncation is permitted.

## Security, ownership and platform findings

The proposed source object representation is an ordered list of private members.
This deliberately avoids putting untrusted object keys straight into the current
unkeyed hash table. Linear duplicate/lookup scans are metered before comparison,
bounded by member and work limits. Typed user dictionaries still use the existing
Runtime; conversions must precharge conservative worst-case probe/comparison work
or reject before constructing one. Do not claim that input-size bounds alone
prevent hash flooding. No dictionary hash redesign is required by JSON.

Nodes have private payloads and no public mutation. Parser-owned lists are sealed
before publication and never retained by a mutable public alias. Returned child
nodes retain identity; arrays/keys accessors copy outer containers while preserving
readonly child authority. All traversal stacks and partial objects are ordinary
precise roots. Failed parsing/conversion never publishes partial output, but
already allocated garbage is collected normally; there is no new arena/ledger.

Limits count logical bytes/nodes/work, not exact resident memory. A pair of source
trees, spare list capacity, strings, compiler descriptors and allocation overhead
can exceed the input size substantially. Typed `T` fixed extents must be checked
before allocation. Actual allocator exhaustion remains the existing checked
Runtime failure, distinct from recoverable configured `limit_exceeded`.

No IO, global state, entropy, scheduler, OS locale, C ABI or new native library is
needed. JIT/O2/AOT share generated IR. Numeric formatting is already Runtime-owned;
only JSON lexical policy is new. Windows/macOS still need native execution for
float conversion, byte output and installed modules. Linux checks cannot prove
those results. Existing worker transfer rules exclude managed/collection results;
do not promise that a parsed DOM can be returned across a worker boundary.

CSV/TOML/XML may later reuse bounded UTF-8 byte output, error location conventions
or proven typed DTO traversal. No shared serialization framework or alternative
`data/json` module is proposed now.

## Decisions for Wrench and Boss

1. **API and typed scope:** recommend dynamic `parse`, explicit `parse_as<t>`, shared
   `stringify`, and narrow generated public-data adapters. Complex invariants use
   explicit DTO mapping. No overload system or private-field access privilege.
2. **Data policy:** recommend exact numeric lexemes, strict decoded duplicate-key
   rejection, case-sensitive field names, required nonnullable fields, missing
   nullable managed fields as null, and unknown fields rejected unless explicitly
   ignored. Approve these together with the enum/null mapping in DESIGN.
3. **Safety/output defaults:** recommend bounded iterative traversal, the concrete
   default budgets in DESIGN and compact insertion/declaration-order output.
   No pretty-print/canonicalization/streaming expansion in this initial module.

The current language's visibility, readonly, ownership, no general overload sets,
LLVM version and result propagation are settled constraints, not new decisions.
Stop for Golden Wrench architecture review and Boss contract approval.
