# Stage 8 Sprint 6 — selected language ergonomics

21 October 2026. **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending; not hosted locked.**
Baseline `3d61279237c62157f2ecfff01aa24d6c937f9a0d`.
[Decisions](DECISIONS.md#21-october-2026--stage-8-sprint-6-owner-approvals).
Approval is separate from implementation and acceptance evidence.

## Type relationships

`Type has Target` produces a compile-time bool. Operands use existing type syntax,
including qualified and concrete generic types and generic parameters. Nonnullable
managed/value structs and interfaces are supported. Primitive, collection, enum,
nullable, callable and value-expression operands diagnose.

A struct target tests exact nominal identity of directly stored `using` fields,
including inherited storage. Repeated matches are true; ordinary fields, self
identity, ancestors without matching storage and recursively nested components do
not confer the relationship. An interface target reuses `ir::satisfy_constraint`,
including exact signatures, accessible original methods, ancestry, one-hop
promotion and ambiguity rules. Failed conformance is false; resource exhaustion
must not become false. No value erasure, reflection, conversion or authority is
introduced. Generic bodies check at concrete specialization; both conditional
branches check even when `has` is constant. This is not conditional compilation.

## Value-producing if

```
var label = if ready:
    "ready"
else:
    "waiting"
```

Each branch contains exactly one value expression; else is mandatory. Nesting and
`else if` work. Conditions use existing bool/pattern rules; captures belong to the
true branch. An expected type contextualizes both branches; otherwise the first
branch anchors existing compatibility for the second. Both branches check, while
only the selected branch executes. No numeric widening, truthiness or new null
rules apply. Existing branch-local IR, typed slots, copies, capability provenance,
precise root homes and checked cleanup carry results across the join.

## Multiline calls

Ordinary, constructor, method and explicit generic calls accept argument
continuation inside matched parentheses, including blank/comment lines and a
trailing comma. Nested delimiters retain their own parsing. Spaces on continuation
lines are cosmetic; tabs retain existing rejection. Actual function/match/if bodies
remain indentation sensitive, including when nested in a call. For an indented
value-producing argument, put its separating comma on a dedented line after the
last branch. Existing positional-prefix/named/default binding is unchanged.
No continuation outside supported delimiters, backslashes or multiline generic/
type headers is added. The formatter preserves cosmetic continuation indentation
and still proves token equivalence and idempotence before returning edits.

## Bounded automatic interpolation

`"Hello, {name}!"` accepts ordinary single-physical-line expressions that produce
string. No prefix is needed. Substitutions evaluate once in written order, using
existing immutable UTF-8 concatenation, pure constant evaluation, checked allocation
and precise GC roots. Embedded NUL survives. `{{`/`}}` and Unicode-escaped braces
produce literal braces; decoded escapes are never reparsed. Embedded quoted
strings and nested delimiters are accepted. Empty/malformed substitutions,
unmatched braces and format specifications diagnose.

Automatic numeric/bool/custom-value conversion, locale/formatting, F2/N2/D5,
alignment and triple-quoted strings remain deferred. No formatting engine, public
string conversion API, new Runtime representation or new IR opcode is added.
Source spans remain on the original snapshot; compiler-owned tooling sees written
references inside substitutions. Nested interpolation uses the existing bounded
source diagnostic approach, with a 64-level lexical nesting guard.

## Ownership and gates

Keep all earlier Stage 8 contracts, CI #82 repair and Sprint 5 async retirement.
Only local work is authorized. Wrench review, owner Git operations, explicit final
hosted authorization, seven hosted jobs and actual editor-client validation are
separate gates. Root README stays unchanged. Stage 9 and library implementation
are not part of this sprint.


## Local acceptance

[Acceptance evidence](stage8/SPRINT6_ACCEPTANCE.md) records 156/156 Debug,
156/156 Release and 156/156 strict ASan/UBSan/leak suites: **468/468 final unique
outcomes**. Both full 1 MiB frontend gates passed. The [hosted checklist](stage8/FINAL_HOSTED_GATE.md)
separates local checks from the remaining owner-controlled platform/editor gates.
