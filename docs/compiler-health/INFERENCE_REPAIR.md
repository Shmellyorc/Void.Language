# Shared frontend hint repair specification

The shared repair is implemented and locally verified; see
[verification](stabilization/VERIFICATION.md) and the
[review handoff](stabilization/HANDOFF.md). This original specification remains
the semantic regression contract.

Baseline `1d4ad15f5215838d209e48bb47ac7c3f52b05371`; research only.
This specifies a repair for Keyboard after review, without modifying production
source. Exact independent reductions/diagnostics and timing receipts are in the
[benchmark evidence](BENCHMARKS.md). [Issue inventory](ISSUES.md),
[ordered roadmap](ROADMAP.md).

## Confirmed failures

| Expression | Current hint / result | Passing control |
|---|---|---|
| `identity(read()?)` | Infers `result<i32,string>`; real argument is i32, producing mismatch. | `identity<i32>(read()?)` or extracted payload local. |
| `first([read()?])` and generic dictionary values | Wrapper infects nested element inference. | Explicit payload type. |
| `identity(nested()??)` | Outer result hint survives both projections. | Explicit i32. |
| `var task=ready(); identity(await task)` | Task hint survives await. | Explicit i32. |
| `identity(await ready())` | Currently passes because two wrong hints cancel. | Must keep passing after both corrections. |
| `var task=identity(ready()); return await task` | Async call hint uses declared payload rather than task. | The real async-call checker constructs a task type. |
| Generic argument containing a multiline `if` expression with branches 7/8 | Uses condition bool rather than result branch. | Explicit i32. |

Sources: `anchor` [compiler.cpp:2147–2224](../../src/frontend/compiler.cpp),
generic argument inference [generic_inference.inc:65–70](../../src/frontend/generic_inference.inc),
actual async-call type `compiler.cpp:1980`, await checker `2287–2290`, propagation
checker `2508–2516`, choice checker `2737–2745`, choice AST
[pattern_parser.inc:55–62](../../src/frontend/pattern_parser.inc).

## Smallest coherent semantic correction

1. **Propagation hint:** obtain the operand hint once. If it is a result, use
   `ir::field_type(*operand,1)` to project its successful payload. Otherwise
   return no hint. The ordinary propagation checker remains responsible for
   non-result errors, enclosing return/error compatibility and deferred-code
   restrictions. Raw descriptor field types lose readonly/reference-path
   authority; use the existing projection (`src/ir/module.cpp:300–310`).
2. **Async-call hint:** ordinary, qualified and method call hints must describe
   the actual expression type, including the existing task wrapper for async
   functions. Audit specialized-method and selected-method branches, not only
   the last ordinary-call return. Reuse `unit.make_task` and existing result
   metadata; do not invent a second task shape.
3. **Await hint:** project a valid task hint through the same result type shape
   as `awaited_expression`; unsupported shapes produce no hint, leaving the
   actual checker authoritative. Test existing-task and immediate-call forms
   together: changing await alone can regress the currently passing cancellation
   of wrong hints.
4. **Choice hint:** inspect value branches, never the condition. Select available
   hints conservatively and preserve contextual literal typing. The fallback
   `inference_type` path also needs branch handling when both branches are
   untyped literals; otherwise a bool mismatch merely becomes “cannot infer”.
   Pattern-bound branch names require their ordinary scope: do not run a second
   checker or emit an early unknown-name diagnostic during a hint query. Leave
   branch compatibility and error reporting to existing expression checking.

These are **type-only queries**. Do not call full `expression`, evaluate, lower,
or rewrite arguments during inference. Preserve existing generic specialization
and authority validation. Source diagnostics must retain original spans.

Actual propagation lowering already evaluates the operand once and branches
through existing cleanup (`compiler.cpp:4189–4198`). Calls materialize source
arguments in order (`4495–4514`). These paths need regressions, not replacement.

## Remove exponential arithmetic hint traversal

`operator_anchor` (`2107–2119`) recursively queries operands; primitive operators
return no selected custom operator. `anchor` (`2218–2220`) then recursively
queries the same operands again. This doubles work per left-chain depth.
`binary_expression:3291` and the two inference passes add further requests.

Recommended bounded correction:

- Compute the vector of **original** child hints once in the arithmetic helper.
- Reuse that vector for existing exact custom-operator lookup, with its current
  literal/null candidate adaptation.
- If no custom operator result exists, return the first original available hint
  from the vector, preserving builtin fallback ordering.
- Return that result directly from the arithmetic branch of `anchor`, including
  an empty result. Do not fall through into the generic child traversal again.
- Do not return a candidate-adapted literal type as an authoritative new hint;
  untyped literals must retain their normal contextual/default treatment.

No global memoization is required. Persistent hints risk becoming stale when
contextual checks rewrite expressions or specialize generics. This removes the
demonstrated exponential duplication; ordinary repeated checking at successive
AST nodes may still be polynomial and must be measured rather than assumed gone.
Keep actual operand validation, ambiguity/effects and lowering unchanged.

## Required focused regressions

Use existing generic/calls test infrastructure, independent of JSON:

- Every positive/negative in the table; nested lists/dictionaries/results,
  qualified generic functions, methods/constructors and explicit arguments.
- Managed payloads with readonly nested paths: valid const forwarding passes;
  unauthorized mutation remains rejected. Preserve established authority rules.
- `?` on non-results, wrong enclosing result/error type, non-result return and
  deferred use retain ordinary diagnostics.
- Side-effect counters prove exactly-once operand evaluation and source-order
  arguments. `Err` skips the outer generic body and later arguments, with one
  resource cleanup. No duplicated async task creation or await.
- Choice literals, typed/mixed branches, nested choices, pattern-bound names,
  deliberate branch mismatch and error recovery. Test both hint and fallback
  inference paths without widening language syntax.
- Left/balanced/extracted arithmetic controls; existing custom operators,
  overload ambiguity, contextual literals, null and readonly effects.
- Source check, normal JIT, O2 JIT and independently linked AOT; compiler-owned
  LSP concrete signatures/completion/navigation and source diagnostics.
- Real 1 MiB stack and existing depth-limit regressions.

Rerun the preserved bounded 4/8/12/16/18/20 arithmetic series. On this machine,
20 additions should approach the balanced process floor rather than 1.514 s;
the proposed acceptance gate is below 50 ms and below 5× its contemporaneous
balanced control, with no two-operator quadrupling. Treat timing as a manual
before/after gate, not a flaky universal CI assertion. Extend to depth 64 only
after the small series proves the exponential walk is removed.

No claim here explains the historical DSE or async IO SIGSEGV. The source-check
defects and performance recurrence stand on their own evidence.
