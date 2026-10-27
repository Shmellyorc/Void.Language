# JSON engineering roadmap


Implementation authorization, 27 October 2026: Boss approved the complete
contract and all three decision bundles. Earlier proposal/approval wording is
historical. The explicit `use json as json` example route was subsequently
approved to preserve the existing no-shadowing rule. See [current status](IMPLEMENTATION.md)
and [the public API](API.md).
27 October 2026. Research baseline: `194603ea00eb38aff72b7d6e950865b3a23df92a`.
**Historical research-stage gate:** no implementation authorization at research
time. The subsequent Boss authorization above supersedes that gate.
[Proposed complete contract](DESIGN.md); [source audit and decisions](RESEARCH.md).

## Outcome and completeness matrix

One cohesive implementation sprint must deliver dynamic JSON and the approved
automatic typed-data subset together. Sequential steps below are dependencies,
not requests for new approval or a full hosted run after each step. No package
manager, networking, format-family framework, nullable-language expansion or
Runtime reflection is part of this mission.

| Obligation | Baseline | Required final acceptance |
|---|---|---|
| Module discovery/docs | Standard source/VFS/hover/install infrastructure exists; JSON absent | Installed `use json`, original source navigation, documented API and incomplete-call signature help |
| Syntax and Unicode | UTF-8 boundaries exist; JSON lexer/parser absent | All six roots, exact whitespace/escapes/surrogates, strict malformed/trailing/duplicate handling |
| Dynamic representation | Managed/list/enum representation proven by small probes | Private valid tree, correct kind/get/at/accessors, missing distinct from null and wrong kind |
| Numbers | Existing numeric parse/display reusable; no JSON policy | Exact lexemes and all integer widths, direct float precision, nonfinite/range/underflow behavior |
| Dynamic write | No JSON writer | Shared compact bounded writer, exact escaping/order and retained numeric tokens |
| Typed scalars/collections | Generic specialization exists; adapters absent | Numeric/bool/string, payload-free enums, list/fixed arrays, string-key dictionaries and supported nullables |
| Typed structs | Field metadata/ordinary construction exists; no generic enumeration | Both managed/value and concrete generic DTOs, public const fields, nested data, data inheritance/using under DESIGN rules |
| Unsupported/invariant types | Existing visibility/construction restrictions | Clear diagnostics; explicit application DTO/factory mapping example; no private/init/property bypass |
| Readonly results | Source/IR discrepancy reproduced | Reduced shared repair or demonstrated existing safe route, preserving const authority and independent IR checks |
| Bounds/security | Collection allocation checks exist; JSON budgets absent | Iterative parse/write/typed traversal, limit/work checks before operations, bounded duplicates and dictionary conversion |
| GC/cache/reload | Existing precise Runtime and generation facilities | Roots under pressure/failure, clean teardown, real cache hit and conservative reload compatibility |
| Execution | LLVM 19.1.7 JIT/O2/AOT exist | Matching successful/error semantics across normal/O2 JIT and independently linked O0/O2 AOT |
| Profiles/platform | Linux toolchain available; JSON untested anywhere | Focused Debug/Release/strict checks and real 1 MiB stack tests; explicit later native-platform receipt or unverified status |
| Incidents | Existing exact-source preservation infrastructure | Capture every abnormal status/timeout/internal failure before retries or cleanup; never erase historical incidents |

## Ordered engineering plan

1. **Freeze contract and evidence.** Resolve the three decision bundles in
   RESEARCH. Record actual HEAD, initial dirty files, compiler/toolchain hashes
   and test configuration. Preserve the existing unrelated Math reruns. Adopt the
   existing Random/text-encoding crash wrapper before constructing any fixtures.
   No Git or hosted CI action follows from approval of a design document.

2. **Close the demonstrated readonly dependency.** Reproduce the retained
   fixture against a frozen current Debug/Release compiler; inspect the emitted
   receiver/list/result paths and compare frontend effects with independent IR
   effects. Reduce before editing. Correct the narrow shared discrepancy, if
   confirmed, with positive direct/forwarded `Ok`/`?` lookup and negative
   mutation/escape checks. Keep the IR validator. Require actual execution,
   since `voidc check` already passed the failing fixture. Do not claim an
   unrelated filesystem/LLVM SIGSEGV repair. If a safe source representation
   avoids the discrepancy, demonstrate complete const lookup/return behavior;
   merely changing the sample to mutable roots is not acceptance.

3. **Lay out the public source module and errors.** Add ordinary source enums,
   limits, derived error, private member/payload/node types and documentation.
   Prove owner-private construction and no mutable builder alias publication.
   Validate limits and checked budget arithmetic once per operation. Store byte
   offsets for nodes and keys; build bounded error paths on failure, not a full
   path string at every success step.

4. **Build source byte primitives.** Shared checked append, ASCII token scanning,
   escape decoding/encoding, exact numeric-token validation and error-location
   helpers. Reuse existing UTF-8 validation/conversion and owned lists. Reserve
   neither a new public string builder nor a second native codec. Verify NUL,
   maximum escape expansion and invalid source-vs-JSON escape distinctions.

5. **Implement the iterative dynamic parser.** Explicit heap frames hold array/
   object phase, partial children, pending key and opening offsets. Check duplicate
   decoded keys using metered ordered members. Accept one complete root, then
   only JSON whitespace/EOF. Reject depth/bytes/nodes/string/number/member/work
   limits before growing buffers or entering children. No recursive descent per
   input nesting. Keep failed intermediate trees private.

6. **Implement observations and exact numeric conversion.** `get`, `at`, kind,
   string/bool/number access, list/key snapshots and length. Match existing negative
   list-index rules without i64-min overflow. Integer digit/scale normalization
   must reject fractional/out-of-range values before using exact-width conversion.
   Reuse the compiler numeric registry for type selection instead of duplicating
   width/min/max tables in source/editor code. A canonical-private type/selection
   hook can generate ordinary scalar branches/constants; it does not require a
   new Runtime number parser. Validate huge exponents without giant padding.

7. **Implement the one iterative writer.** Private byte builder, work stack and
   path/depth accounting; emit compact deterministic output and exact dynamic
   tokens. Reject nonfinite typed numbers before shared display. No stdout-as-
   storage, partial public string or repeated string concatenation. Make writer
   error and cleanup paths explicit.

8. **Add compile-time type admission.** In the existing compiler/generic path,
   inspect concrete type graphs including collections, fields, enum cases,
   visibility, init chain and special resource identities. Record a node before
   following recursive managed edges. Diagnose unsupported shapes with field
   declaration/call locations. Enforce the same admitted schema regardless of
   caller privileges. Memoize within existing compilation ownership and bounded
   analysis work, not a process-wide reflection cache.

9. **Generate to-value adapters.** Use ordinary AST and the existing generated
   display/state-machine precedent. Per-concrete-type typed owner stacks and
   continuations traverse readonly inputs and build fresh children/parents.
   Include fixed-array snapshots, effective public field order, nullable checks,
   enum case-name constants and dictionary ordered iteration. Repeated aliases
   are charged each time; cyclic graphs must reach a finite configured limit and
   return limit_exceeded. No separate identity/cycle framework is required.
   Never call user display hooks or equality operators to invent JSON data.

10. **Generate from-value adapters.** Share type admission and scalar conversion
    policy; generate distinct ordinary checked bodies for validation/postorder
    construction. Typed partial-result stacks and completion masks must never
    read uninitialized nonnullable fields. Check unknown keys, required fields,
    fixed extents and all child conversions before named construction; supply
    missing nullable managed fields as null. No setter replay or generic `t()`
    defaulting. Ensure the generated function itself has depth-independent native
    frames even for recursive managed types. Keep all allocation and construction
    subject to existing typed IR checks.

11. **Wire the six public functions.** Ordinary wrappers share contexts and DOM
    parser/writer. `parse_as` must not run another parser; `from_value` does not
    stringify/reparse. `stringify<value>` may avoid an unnecessary DOM clone but
    must use the same writer. Compile-time selection uses canonical module
    identity and normal binder/shadowing/evaluation order. No new overload set,
    general native FFI or resource provider.

12. **Close mutation, hashing and cleanup boundaries.** Derive the conservative
    dictionary probe/reindex charge from actual `dictionary.cpp` capacity rules
    and check it before insertion. Test adversarial keys, partial list/tree
    failure, repeated aliases and const results. Clear finished work roots.
    Source snapshots and typed effects remain authoritative; never install a
    readonly referent into writable managed storage just because JSON hides it.

13. **Integrate editor/install/cache/reload.** Public `##`, `@param`, `@return`
    docs are normal source docs. Reuse generic signature recovery and semantic
    metadata. Preserve canonical imports, Unicode identity, unsaved overlays,
    UTF-16/stale-response protection and installed-source rename restrictions.
    Generated implementation details stay private; no editor-specific JSON lists.
    Add actual installed-root and cache-hit tests using existing infrastructure.

14. **Complete focused validation and handoff.** Run the matrix below, repair
    demonstrated failures, retain first attempts and record effective unique
    outcomes separately from total executions. Publish the API inventory,
    examples, evidence and platform limits under docs/json. Stop for independent
    Golden Wrench implementation review; Boss controls Git/CI/release.

## Concrete architecture dependencies

| Work | Reuse / likely location | Avoid |
|---|---|---|
| Parser/tree/errors/policies | New `stdlib/json.void`, existing error/convert/text byte patterns | Native JSON parser and competing source parser |
| Scalar/type facts | Existing numeric registry, convert binder and scalar parse/display lowering | JSON copies of numeric widths/float parsers |
| Compile-time adapters | `src/frontend/compiler.cpp`, focused include adjacent to `display_semantics.inc` / `collection_display.inc`, existing generic admission/cache | Public reflection, blanket generated access privilege |
| Construction and capabilities | Existing `construction.inc`, frontend/IR reference analysis and named constructors | GC-byte writes, unchecked partially initialized objects, weakened IR validation |
| Runtime/backend | Existing lists/dictionaries/strings/rooting; ordinary generated IR | New provider, native resource ledger, scheduler, linker or LLVM change |
| Tooling | Existing compiler analysis/generic tooling/docs and stdio LSP tests | VS Code/Vim/Neovim semantic engines or JSON-specific declaration catalog |
| Tests | Existing Random preserving process boundary; text encoding, Math, collections and managed probes | Another general-purpose test framework or routine full matrix |

A new IR opcode should not be necessary for typed object traversal. If engineering
discovers an ordinary AST/IR operation cannot express an admitted mapping, present
that exact missing operation and evidence to Wrench before adding a bridge.
Do not add an open-ended native metadata executor to meet a schedule.

## Focused test matrix

| Group | Required cases and oracle |
|---|---|
| Grammar | Empty/whitespace; all six roots; nested mixed containers; exact separators; comments/trailing comma/input; truncated tokens at every boundary; only four JSON whitespace bytes |
| Text | All short escapes and 32 controls; slash/quote/backslash; escaped NUL; raw DEL; paired boundary surrogates and all lone/reversed/truncated forms; U+10FFFF; noncharacters; invalid UTF-8 through existing byte decode; BOM and U+FEFF-in-string |
| Keys/order | Decoded duplicates (`a`/`\u0061`), equal-value duplicates, case and normalization distinctions, NUL keys, first duplicate location, insertion/declaration order and deterministic output |
| Integers | Every width's min/max and neighbors, u64 maximum, i64 minimum; exact large IDs beyond 2^53; integral/nonintegral decimal exponents, negative zero to unsigned, exponent saturation, number-byte limit |
| Floats | f32/f64 normal/subnormal extrema and half-neighbors, ±0, exact all-zero huge exponents, finite rounding, overflow/nonzero underflow, nonfinite serialize rejection, correct matching-width shortest round trip |
| Dynamic API | Missing versus null versus wrong kind; valid/invalid/negative/i64-min indexes; scalar access no coercion; immutable keys/list snapshots and retained child identity; const chaining, Ok/Err and ? |
| Typed data | Managed/value/generic DTOs, public const fields, every numeric type, nested lists/fixed arrays/maps, recursive nullable nodes, payload-free enums, empty DTO/collections and exact fixed lengths |
| Schema rules | Unknown reject/ignore; required fields even with defaults; missing nullable/null rules; declaration-order failure; stored using/base fields once; original field diagnostics; private/internal/custom-init/properties/resources/interfaces/payload enums/results rejected or explicitly DTO-mapped |
| Safety | Every configurable bound at n−1/n/n+1; 64/65 and 128/129 depth; tiny budgets; invalid/overflowing settings; broad inputs; giant fixed target with tiny JSON rejected before allocation; work exhaustion before duplicate/hash storms |
| Graphs | Managed/list/map cycles reach bounded limit_exceeded; shared acyclic child repeats; stringify input unchanged; node conversion fresh; no escaped builder alias; readonly returned children cannot thaw |
| GC/failure | Existing managed probes force safepoints/collection during byte growth, child/parent allocation and conversion; allocation failure before/at commit; normal failure cleanup then retry; no root/handle/generation leaks |
| Modes | Same source fixture in normal/O2 JIT and independently linked O0/O2 AOT; exact integer/string/error bytes; appropriate float conversion bit oracles; no JIT-only symbol registration |
| Stack | Actual 1 MiB compiler/check/JIT/build/native children, accepted max depth, first rejected depth, wide DTOs, large fixed values and malformed cases. A host shell ulimit without child verification is insufficient |
| Tooling/install | Import discovery, member completion, signature/return docs, argument-only param docs, incomplete generic calls, get/at chains, definitions/references, Unicode-before-call UTF-16 ranges, overlays/stale results, protected installed rename and relocated stdlib execution |
| Runtime lifecycle | Fresh application defaults, body-only reload, field/const/instance compatibility rejection, old-generation lease, actual native-object cache hit, cleanup after failed parse/convert/write |

Use fixed reviewed JSON byte fixtures and explicit expected errors/offsets. A
host JSON library can compare ordinary valid semantic values, but its duplicate,
numeric, surrogate, ordering and invalid-input policies are not the oracle.
For exact number conversion, use integer decimal arithmetic and existing numeric
bit helpers/reference methodology. Do not compare a large JSON integer after
rounding it through Python/JavaScript float. Do not create a timing-sensitive
statistical/performance gate for parser correctness.

Measure a small representative save/config corpus: bytes/nodes, parse/stringify
duration, allocation/root observations and one deeply nested/wide adversarial
case. Use this to catch quadratic concatenation, needless whole-input copies or
retained work stacks. Do not make speculative optimization a prerequisite.

## Profiles, evidence and stop criteria

Primary engineering runs are focused Linux Debug, Release and strict sanitizer
selections. Include affected numeric, strings/text, generics, readonly/result,
collections, GC and LSP regressions when corresponding shared code changes;
repeat only after a change or unresolved failure. Preserve ASan/UBSan/leak flags.
No routine seven-job hosted run or change to workflows is authorized.

Reuse [tests/random/harness.py](../../tests/random/harness.py) and
[tests/text_encoding/crash.py](../../tests/text_encoding/crash.py), including
timeout handling and unexpected `VOID IR:` detection. Preserve exact loaded
fixtures/imports/overlays as `_crash.void`, hashes, compiler identity, command,
stdout/stderr, profile and generator provenance **before** cleanup/reduction/retry.
Reconstructed source is never labeled the original. Keep the original failures
even if later tests pass; no historical filesystem/optimizer/Random incident is
closed by JSON success. Verify preservation with the existing disposable incident
test pattern instead of inventing another incident system.

Before any final cross-platform claim, obtain owner-authorized native Windows
Debug/Release and macOS ARM64 receipts for the focused JSON and installed/AOT/stack
selection. Verify decimal conversion, CR/LF-independent data, Unicode/NUL bytes,
same reference vectors and private source boundaries. Until then report those
platforms as unverified; do not silently count cross-target object generation.

Final acceptance requires every approved row in the completeness matrix, all
public declarations documented, exact policies reflected in tests/examples and
no unresolved JSON-origin crash/internal diagnostic/sanitizer failure. Record
limitations explicitly. A dynamic-only parser or a serializer that skips private
data without diagnosis does not satisfy the recommended approved scope. Wrench
independently reviews source/evidence; Boss authorizes Git, hosted CI and release.
