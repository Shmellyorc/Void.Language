# Stage 8 Sprint 6 — local acceptance and Wrench handoff

21 October 2026. **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending.**
Baseline and unchanged HEAD: `3d61279237c62157f2ecfff01aa24d6c937f9a0d`.
[Contract](../STAGE8_ERGONOMICS.md), [approved decisions](../DECISIONS.md),
[results/log hashes](sprint6-results.json), [source/test hashes](sprint6-source-hashes.json),
[stack evidence](sprint6-stack-frames.json) and [final hosted checklist](FINAL_HOSTED_GATE.md).
No hosted lock is claimed.

## Delivered scope

- Compile-time `Type has Target`, including qualified/generic types and generic
  parameters. Exact direct/inherited using storage; existing structural constraint
  checking for interfaces. No recursive search, value operands, runtime reflection,
  conversions, refinement or new authority. Both generic branches type-check.
- Indented typed if-expressions with mandatory else, one value expression per
  branch, nesting/else-if and existing bool/pattern conditions. Existing expected
  typing or first-branch inference, copy/reference/readonly facts, typed branch
  slots, precise roots and checked cleanup. Exactly one selected branch executes.
- Delimiter-aware multiline ordinary/default/named/constructor/method/generic
  calls, nested arguments, comments/blank lines and trailing commas. Real
  function/match/if bodies retain indentation. Function-type annotations do not
  disable subsequent call continuation; existing collection defaults survive.
- Automatic string-only interpolation with ordinary single-line expressions,
  immutable UTF-8/NUL, doubled/Unicode-escaped braces, pure constants, written
  evaluation order and original literal spelling in exact restart contracts.
  Existing concatenation/IR/Runtime/LLVM 19.1.7 lowering remains authoritative.
- Compiler-owned navigation, hover, rename, completion (including inside string
  substitutions), multiline signatures, diagnostics/repair and formatter
  equivalence/idempotence. Updated VS Code/VSCodium and Vim presentation.

No numeric/bool/custom conversion, format/locale/alignment/triple strings,
unrestricted continuation, conditional compilation or private-field expansion.
No new Runtime string representation, IR opcode or conformance implementation.

## Permanent coverage

Three suites extend 153 to **156**, without duplicating earlier suite outcomes:
`ergonomics_sprint6`, `ergonomics_sprint6_host` and `lsp_sprint6`.

Process checks cover normal/O2 JIT and independently linked AOT, Unicode/NUL,
LF/CRLF/CR, selected branches and side effects, value copies/shared identity,
managed/value/interface/generic relationships, imports and inaccessible original
providers, named/default calls, constructors, methods, generic function values,
nested function/match/if bodies, collection defaults, resource limits, errors and
application statuses. New file/process I/O explicitly uses UTF-8.

Host checks force allocation-budget-one GC through string/value/managed branch
results; verify checked failure and recovery, post-call collection, compatible
body reload and old code, exact constant-spelling restart, Debug cache reuse,
async workers/repeated awaits and explicitly admitted native sink text/cleanup.
The full retained suites also cover prior numeric/binding/text/collection, native,
async/lifetime/cancellation/closing and VFS/readonly/IR contracts.

## Final consolidated local results

All corresponding builds finished before these unfiltered runs. Final command
lines and log/source hashes are retained in the linked JSON evidence.

| Configuration | Result | Elapsed |
| --- | --- | --- |
| debug | 156/156; zero failures | 559.83 s |
| release | 156/156; zero failures | 229.57 s |
| sanitizer | 156/156; zero failures | 476.57 s |

**468/468 final unique configuration/suite outcomes.** Earlier matrices, focused
reruns, standalone probes, historical controls and per-process checks are not
added to this count. Both **complete** Debug/Release frontend gates passed with
real 1 MiB stacks and unchanged recursion guards/legal cases. Extension Node
24.19.0 typecheck/build/unit checks passed **4/4**. Headless Vim syntax assertions
passed; no fresh interactive GUI/client acceptance is claimed.

Strict ASan/UBSan/leak acceptance ran outside ptrace with leak detection and
halt-on-error enabled. No test assertion, sanitizer category or leak check was
disabled. Instrumentation covers C++/C compiler, Runtime and provider code;
LLVM-generated application instructions are not automatically instrumented.

## Unsuccessful engineering attempts and repairs

Raw logs remain locally in `build/stage8-sprint6/evidence/`; tracked hashes/results
preserve their identity. WORK_LOG records the sequence without inflating totals.

- Corrected FieldType access, synthetic literal-token spelling, if/else suspension,
  inferred-default queries against partial method headers and Boolean has
  lookahead. Preserved whole literal spelling for exact constant/default contracts.
- Two full Debug 1 MiB attempts failed in the existing 400-receiver-chain negative.
  Borrowing the new handler's expected Type restores expression_impl from 1456
  to **1312 bytes** in this Linux Clang Debug artifact. Actual full gates then pass;
  no depth/stack/assertion relaxation. GDB trace is retained.
- Fixed fixture assumptions about duplicate diagnostic wording, in-scope completion,
  function-reference generic inference and keyword/brace highlighting. A blanket
  header-line guard was rejected because it broke existing collection defaults.
- First full Debug/Release passed 156/156 each before final parser repairs; they
  are historical engineering evidence. A strict exact-spelling probe exposed a
  stale core object after an included-file edit during compilation. That strict
  CTest run was stopped (exit 130, 12 completed suites), a fresh rebuild was forced,
  and stable final sources received the consolidated three-configuration runs above.
  Subsequent source changes were completed before final builds. No stale result
  is presented as final acceptance.

## Six-sprint status and remaining gates

| Sprint | Bounded local evidence | Remaining |
| --- | --- | --- |
| 1 | Boss locally accepted; original 423/423 and focused CI #82 repair preserved | Windows Debug/Release physical-path/reparse execution |
| 2 | 429/429 calls/numeric baseline preserved | Wrench/final hosted review |
| 3 | 435/435 text/collection baseline preserved | Wrench/final hosted review |
| 4 | 444/444 application/static-provider baseline preserved | Wrench/final hosted review |
| 5 | 459/459 measured frame-retirement baseline preserved | Task/result/Work/Handle-capacity ownership remains deferred |
| 6 | 468/468 final bounded ergonomics acceptance | Wrench and final hosted/editor gates |

These are successive evidence snapshots, not values to add together. All six
planned engineering sprints have local evidence; deferred Bible features are not
implemented by implication. Signed numeric and text/collection approvals remain
unchanged. The CI #82 protected source/test hashes and public root README hash
match the baseline.

**Stop for Wrench review.** Boss controls commit/push and explicit final hosted
authorization. No Git/remote mutations, hosted run/request, publication, website,
stdlib implementation or Stage 9 occurred. Windows/macOS and fresh actual
VS Code/VSCodium/Neovim/Vim validation remain pending; Linux cannot establish them.
Stage 8 is **not hosted locked**. Documentation/future prompts stay under docs/;
root README work needs separate explicit Boss authorization.
