# Phase 12 value struct contract — Option A approved

**Design/proposal history.** For delivered behavior and superseded activation gates, read the [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status) and the corresponding implementation guide.

Reviewed 12 October 2026; Option A approved by the project owner on the same date.
This preserves the design proposal and alternatives; VALUE_STRUCTS.md records the
implemented surface and limitations. The Language Bible
remains authoritative. Phase 11 Actions run #16 at `1b32151` passed Linux, Windows
and macOS according to the project owner.

## Established requirements

The Bible's safe defaults (§7), value structs (§12), and construction (§16)
establish recursive initialization, copying and construction with optional named
fields. Phase 12 adds numeric/bool fields and acyclic nested value structs,
mutable local fields, independent copies, function parameters and returns.
Readonly bindings protect their entire field access path. Named-field argument
expressions execute once, left to right in written source order, as explicitly
required by the phase request. Fields not named by a construction retain defaults.

```python
value struct point:
    x: i32
    y: i32

func translate(original: point) -> point:
    var copy = original
    copy.x += 1
    return copy

func entry(value: i32) -> i32:
    var original = point(x = value)
    var moved = translate(original)
    return moved.x - original.x
```

Construction does not allocate a managed object. Nested defaults terminate only
for acyclic value layouts; direct and indirect recursive layouts are errors.
Unsupported field types, duplicate fields/initializers, missing field names,
invalid mutations and exact type mismatches receive ordinary source diagnostics.

## Approved Option A: declared field default expressions

The Bible illustrates declared defaults such as `health: i32 = 100`, but does
not specify whether a default can execute a function on each construction. Its
§16 explicitly leaves named construction/custom `init` interaction open. The
development strategy also identifies the construction sequence as a design risk.

**Recommended Phase 12 contract:** a declared field default must be a pure,
typed compile-time value. Reuse the existing constant expression rules and
numeric evaluator, including their rounding and checked-conversion rules. Nested
value construction can participate when every component is compile-time valid.
Calls, `self`, and references to other fields are rejected in defaults. With no
declared default, use zero, false, or the nested type's recursive defaults.
Named-field expressions remain ordinary runtime expressions; this restriction
applies only to defaults written in the type declaration.

```python
const initial_x: i32 = 4

value struct point:
    x: i32 = initial_x
    y: i32

# Proposed: x defaults to 4, y to 0.
# point(x = compute_x()) is allowed in function code.
# x: i32 = compute_x() in the declaration is rejected in Phase 12.
```

This bounds Phase 12 without claiming that runtime field defaults are permanently
forbidden. Custom `init`, positional constructors, and default expressions with
runtime effects remain unsupported pending their own construction policy.

**Alternative:** allow runtime calls in declared defaults now. That requires
approval of when defaults execute, their order relative to explicit named
arguments, whether overridden defaults still execute, and failure propagation.
Those observable rules must be settled before implementation; they cannot be
chosen merely as LLVM lowering details.

## Proposed implementation boundaries

- One nominal type model shared by parsed/validated source, public analysis,
  tooling and VOID IR. Preserve existing primitive type and numeric contracts.
- Extend the existing IR with aggregate construction, field extraction and field
  replacement. Continue using typed local slots and the shared LLVM lowering for
  both execution modes. No alternate struct frontend or type checker.
- Fields retain declaration order internally. LLVM's target data layout supplies
  native alignment. Offsets, packing and binary serialization are not public
  promises. Generated callers and callees share a compiler-private convention;
  no external C ABI for aggregates is claimed or added. The existing i32 host
  invocation boundary remains intact.
- Resolve type names through the existing module ownership/import rules. Exact
  nominal identity matters, even for types with identical fields. Update shared
  declaration/reference records rather than maintaining a tooling-only model.
- Include field types, declaration order and declared defaults in exact dependency
  compatibility checks. A changed value layout or construction contract requires
  manual restart; incompatible hot reload must fail before replacing live code.
- Struct methods, custom initializers, field visibility modifiers, aggregate
  equality, aggregate numeric conversions and external aggregate bindings are
  outside this slice. Unsupported forms must diagnose rather than appear to work.

## Validation after approval and implementation

Exercise real source through normal/optimized ORC JIT and native AOT: defaults,
nested construction, source evaluation order, independent copies, nested mutation,
readonly rejection, parameters, returns, and source-located invalid programs.
Check generated caller/callee boundaries and reload compatibility. Preserve and
rerun every existing 21-suite native matrix entry in Debug, Release and supported
sanitizer configurations, including installed and official LLVM 19.1.7 SDK
validation. Extend shared analysis/LSP checks and run extension verification.
Windows/macOS execution requires hosted CI and will be reported separately from
local Linux results. No Phase 12 build or test result is claimed by this proposal.
