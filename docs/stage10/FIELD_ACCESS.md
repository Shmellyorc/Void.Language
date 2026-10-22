# Stage 10 private and readonly stored fields

## Boss authorization — 21 October 2026

Boss approved implementation of the entire 44-step Stage 10 mission, B1–B4,
and the Wrench amendments. This supersedes the proposal-only authorization
statements below; it does not claim implementation or acceptance.

- One heterogeneous print and shared display/format kernel; exact numeric
  ties-to-even rounding, checked allocation, no arbitrary width/precision cap.
- Declaring-owner field privacy and deep const paths; direct own-initializer
  replacement grant only; existing method/property privacy remains unchanged.
- Owner-contained exact operators include binary `%` and derived `%=` in addition
  to the other arithmetic, unary, equality and ordering families. Floating builtin
  remainder remains prohibited. Independently declared `!=` remains prohibited.
- Approved verified pinned Ryu/date and bundled versioned IANA rules; recursive
  custom-equality dictionary-key exclusion remains required.
- A standalone CR completes a line immediately. Pending CRLF continuation belongs
  to logical line reads; mixed raw reads retain byte ownership.
- Cursor queries require an accessible owned response channel. Redirected stdin,
  unavailable ownership or responses return a recoverable error; no hidden tty.
- Preserve Stage 9 Windows corrections, LLVM 19.1.7, ownership and GC. No Git
  mutation, hosted CI dispatch, publication or root README edit is authorized.


**Approved for implementation; Stage 10 acceptance remains open.** Existing readonly/reference/initialization
contracts remain authoritative. [Main roadmap](../STAGE10_DESIGN.md).

## Pre-implementation gap and approved spelling

At the Stage 10 baseline, the struct parser accepted ordinary/using/event fields and defaults.
`ir::ValueField` records name, type, owner, using and event markers, but has no
declared visibility or readonly-field policy. A readonly reference in `FieldType`
is useful existing infrastructure; it is not enforcement that a field cannot be
reassigned. Existing method/property visibility is module-based, and existing
`const` locals already provide deep readonly access paths.

```python
struct player:
    private health_value: i32 = 100
    const id: i64 = 0
    private const tags: [string]

func (p player) init(id: i64):
    p.id = id

property (p player) health: i32:
    get:
        return p.health_value
    set(value):
        p.health_value = value
```

Use `const` consistently, not a second `readonly` keyword. Omitted visibility is
public; `public`, `internal`, `private` are explicit alternatives. Canonical modifier
order is visibility, then const, then using/event marker where valid. Diagnose
duplicate/conflicting modifiers. A const event is invalid: event subscription and
owner emission remain the existing special event policy. A const using field is
valid and applies readonly projection to promoted paths.

Stored-field policy has two orthogonal parts: who may name it, and through which
paths its storage/referents may be mutated. Neither changes nominal identity,
layout, collector ownership or global object mutability.

## Lexical field visibility

| Access site | Private stored field |
|---|---|
| Owner's own initializer, receiver method, property accessor or operator body | Allowed, including another instance of that same nominal owner |
| Lexically nested closure in such a body | May read/name it with ordinary captured authority; no constructor write grant escapes |
| Same-file free function or another struct's method | Denied; same file alone does not grant field-private access |
| Derived initializer/method/override | Denied for base-private storage; no protected access |
| Component owner accessing its own field | Allowed inside component code |
| Containing struct reaching a private component member | Denied; containment is not friendship |
| Generic owner method specialized with concrete types | Same owner privilege for the matching nominal generic definition/specialization |
| Interface binding or caller using an exposed public operation | May call the accessible operation; never gains access to its backing fields |

The Bible distinguishes declaring-struct field privacy from file-private top-level
functions. Existing receiver methods and accessors retain **their current
file-private semantics**. Do not silently turn every historical private method
into a new type-private method. No friend declarations, protected access, static
members or visibility redesign are needed to enforce fields.

An owner context is lexical compiler metadata attached to ordinary functions and
accessors, including their nested closure bodies. It is not inferred from a
runtime receiver, caller privilege, same-file text proximity, or a function's name.
Ownership remains restricted to the type's defining source module; extension
declarations in foreign modules stay rejected. Generic specialization retains
the original owner identity and source span rather than manufacturing a privilege
for every substituted type in the body.

Internal field access follows existing project/library ownership boundaries,
including reserved standard source identities; public follows ordinary imported
type access. Exact source identity and direct owner imports continue to apply to
methods. Field lookup must not invent an additional import rule independently.

## Selection, constructors and patterns

Extend **one** shared `ir::select_member` access context with module and lexical
nominal owner. Keep the existing real-member precedence and one-hop promotion.
Selection first identifies the member/ambiguity under existing namespace rules,
then checks its declaring-owner policy. An inaccessible real field blocks fallback
to a same-named promoted member. Inaccessible using storage cannot be traversed
to expose component members. Ambiguity is not resolved by expected type/signature.

No-init construction still supplies/defaults fields in declaration order. An
external named-field argument must be allowed to name that field; `player(secret
= value)` cannot bypass privacy. Public defaulted fields can be supplied; private
defaulted fields may be omitted and initialized normally. If an external caller
cannot supply a private required field, diagnose and recommend an accessible
owner initializer/factory. Do not make the whole type unconstructible merely
because it contains a defaulted private field. Const named fields may be supplied
at creation if visible: initial creation is not post-construction mutation.

Custom-init arguments continue to bind **only** to initializer parameters, never
also to fields. No hidden setters or promoted constructor labels. Explicit base
initialization retains first `super.init(...)`, base-to-derived defaults/bodies
and one final managed commit. A derived init cannot initialize a private or const
base field directly; the base init owns that action.

Field patterns must enforce visibility, including pattern fields inside inherited
or composed types. A private field cannot be read through pattern matching when
normal access is forbidden. Captures retain const/reference capabilities and value
snapshot semantics. Whole-value matching does not invoke custom operators or
grant an addressable mutable view. Compiler-generated tracing, default construction
and structural dictionary metadata are trusted internal operations, not source
privacy bypass APIs; they never expose hidden field values through display.

## Readonly field semantics

After construction, `const field` prohibits replacing that field, nested inline
writes through it, indexed writes, mutating methods, property setters, event-like
mutation and mutable-reference escape through its access path. Projection is deep
across embedded managed/list/dictionary/interface references. The rule holds even
if the containing object/local is mutable.

| Operation through a const field | Result |
|---|---|
| Read scalar/string | Allowed |
| Replace field / compound assignment | Rejected after construction |
| Mutate embedded inline value in place | Rejected |
| Call nonmutating method/getter/display/operator | Allowed with ordinary authority checks |
| Mutate a referenced object/list/dictionary | Rejected through this path |
| Copy a reference-free field value into `var copy` | Independent writable copy, subject to any const fields declared inside its own type |
| Copy a reference-bearing value into `var copy` | Independent value storage; embedded reference leaves retain readonly authority |
| Hold a separate original mutable reference supplied to construction | That independent alias may still mutate its referent |
| Copy entire containing struct into another mutable local | Its declared const fields remain const; declaration policy cannot be erased by copying |
| Return a field through getter/helper/interface/generic code | Retain reference readonly provenance; no authority restoration |

This extends VOID's existing **access-path** readonly policy. It does not freeze an
object shared with other aliases. C# readonly reference fields protect the field
binding more narrowly; VOID must retain its already-approved deeper authority.
[C# readonly comparison](https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/keywords/readonly).

Implement projection through shared `field_type`, readonly-path and place-selection
operations. Store the field's declared readonly reference profile as immutable
nominal metadata; intersect with the containing access path. Machine layout/type
equality still ignores authority, while capability compatibility and reload compare
it explicitly. Null/default leaves retain readonly policy even before a non-null
value is ever observed. No Runtime permission bit, freeze pass or object clone.

## Constructor-only write grant

Recommend C#-style direct initializer assignment rather than inventing a separate
one-time assignment language:

1. A const field can receive its pure declaration default or creation argument.
2. The **direct declaring owner's init body** may assign that field on its own
   construction receiver while its initialization phase is active. Assignment can
   occur more than once; this is a lexical initialization grant, not a write-once
   dynamic cell. The rule applies to value and managed owners.
3. The grant permits replacement of that field. It does not authorize mutating a
   referenced object through a const field, leaking mutable authority, assigning
   another completed instance's const field, or invoking a mutating helper on
   that const path. Prepare mutable child state before assigning the field.
4. Helpers, nested/escaping closures, setters, operators and ordinary methods do
   not inherit the grant. Passing self/aliases cannot transport it. Such helpers
   may read only what existing construction readiness permits.
5. A base grant ends with its base initialization phase; a derived phase cannot
   reopen it. Failure runs existing cleanup and never publishes incomplete identity.

**Preserve C1:** value fields/defaults are complete before a custom value init runs.
A required nondefaultable value field still cannot be left unavailable and first
initialized later by this extension. For example, a readonly numeric ID may be
default-zero then assigned in init; a new first-assignment system for arbitrary
nondefaultable value fields is not necessary or approved here.

**Preserve S1:** required managed references may be unavailable in the identity-free
stage until directly initialized; readiness must prove every required field on all
successful paths. Const assignment does not publish a staged object, permit
self-links, bypass all-fields-ready helper requirements, or grant parent identity.
Existing effects on completed child objects are not rolled back if later init
fails. Returns/default paths retain current init/failure policy.

Use the actual staged receiver/place origin to validate a grant, not syntactic
member spelling alone. A local alias cannot gain a fresh initialization grant;
direct writes must select the original initializer receiver and a directly owned
field. Record the owner/phase/field in typed operations. A mere `initializing=true`
flag supplied by arbitrary IR must not authorize writes.

## Properties, inheritance, interfaces and generic effects

Properties already lower to ordinary accessors. Getter-only storage is not
automatically deeply readonly today; do not change all getter behavior. A getter
that reads a const backing reference **does** return that readonly provenance.
A setter cannot modify const backing storage after init, even if it is private.
Nested value-property updates preserve the existing getter-once/setter-commit
rules and cannot bypass a const root.

Fields retain their original declaring owner through inherited physical paths.
No field hiding, layout reorder, base privilege or override relaxation. A const
using component produces readonly promoted paths; promotion never copies a
mutable alias out to restore authority. Interfaces still match accessible exact
methods, not fields/properties. Concrete generic constraints and managed interface
views use the same selected methods/effect summaries; qualification survives all
returns, casts, captures and nullable/interface conversions.

Concrete specializations retain declared field policy and leaf maps. Do not erase
readonly because a field's reference shape is unresolved at template declaration
time. Reuse specialization-time analysis and the existing finite effect worklist.
Both sides of an if-expression still type-check; const conditions do not bypass
access checking. Operators/custom formatting are ordinary calls for these purposes.

## Compiler, IR, snapshots and tooling

Carry visibility, declaring owner, const marker and declaration spans through the
source AST, immutable nominal field metadata and analyzed declarations. Extend the
existing semantic checks, capability pass and construction pass rather than adding
a second field validator. The IR verifier independently rejects unauthorized field
loads/stores, const-root mutations, forged constructor grants and incompatible
readonly argument/return capabilities. It must know which function owns which
construction phase; trusting an accessor name or frontend-provided effect bit is
insufficient. Preserve the trusted-host IR boundary; this is compiler consistency,
not a claim that a native embedding host is sandboxed.

No new LLVM layout or GC representation is required. Lower validated accesses and
initializer stores through existing physical field paths, staged roots and complete
publication. Access/const/owner changes join exact compatibility contracts and
require restart even when physical layout is unchanged. Old snapshots/code leases
retain their old policy/metadata; no mutable global descriptor patching.

Compiler-owned completion suppresses inaccessible fields in the actual lexical
context and marks const fields readonly. Hover explains declaring owner and
readonly provenance. Definition/references point to the original declaration;
rename obeys existing graph/version/installed-source restrictions. Diagnostics
include the use span and field declaration, and distinguish private access from
readonly writes, incomplete construction and ambiguous promotion. Failed dirty
analysis never reuses a previously accessible result. All editors consume the
same `voidls` metadata. See [tests](TEST_PLAN.md) for the full cross-product.
