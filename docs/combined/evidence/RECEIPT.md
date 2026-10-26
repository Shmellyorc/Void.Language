# Local verification receipt — 26 October 2026

All final local cases pass. Earlier failures and focused corrections are retained;
[final case-by-case results](final-results.json) identify the evidence for each case.

Local Linux x86-64; official LLVM/Clang/LLD 19.1.7. [Exact build flags](toolchain.json),
[build/source inventory](source-sha256.json), [module/readability audit](completeness.json),
and [public result inventory](public-results.json) are retained. Git HEAD remains
`37e98791fcbc3dcc434ab8437c7af1ddf2d8d054`; root README matches its baseline hash.
No hosted CI, branch, commit or push was performed.

## Focused verification

| Profile | Main selection | Corrected/final supplementary evidence |
| --- | --- | --- |
| Debug | 49/50; snapshot comparison failed after documentation changed | [616-check frozen-source console rerun passes](debug-console-final.log); all 50 cases finally pass |
| Release | 48/50; temporary ownership expectation and snapshot comparison | [2/2 corrections pass](release-corrections.log); all 50 cases finally pass |
| Strict sanitizers | [46/46 pass](sanitizers-final.log) | Actual 1 MiB VOIDLS recovery also passed |

The [exact selection and registered commands](test-selection.json) contain 50
native and 46 sanitizer CTests. This is a focused selection, not the entire project
or the reserved cross-platform acceptance matrix. Raw [Debug](debug-integrated.log)
and [Release](release-integrated.log) integrated failures are preserved separately
from corrections. No assertions were deleted to produce a pass.

* Final console source/JIT/O2/linked AOT: **616 checks in every configuration**,
  including PTY input modes/restoration, prompt ordering, partial transfers, broken
  pipes, cache invalidation and installed discovery. Compact byte-digest receipts:
  [Debug](console-summary-debug.json), [Release](console-summary-release.json),
  [strict sanitizers](console-summary-sanitizers.json).
* Final expanded payload/conversion tests: **246 parsing assertions, 74 processes**
  per invocation, normal/O2 JIT and linked native AOT. Both native builds also ran
  the complete harness with an actual 1 MiB stack:
  [Debug](debug-payloads-final.log), [Release](release-payloads-final.log).
* Common error/unwrap/temporal ownership: **4,867 checks**, **351 failing allocation
  budgets per JIT mode**, forced GC, all five module errors, inherited fields,
  temporal display adaptation, async exact propagation and repeated await:
  [Debug](ownership-debug.log), [Release](ownership-release.log).
* Memory ownership: **10,262 checks**, **542 failing allocation budgets per JIT
  mode across 14 operations**, including in-capacity append/overwrite, unchanged
  state on failure, cleanup and successful retry: [receipt](memory-ownership-debug.log).
* Compiler-owned metadata: **268 installed public declarations documented**, all
  explicit parameter/meaningful return descriptions checked, **zero documentation
  warnings**; canonical base has exactly three fields and all five errors derive
  from it. LSP hover/completion/signature/navigation has **97 assertions**.
* VOIDLS: actual original Debug failure was completion **-32603**, `bad_function_call`,
  **exit 2**. [Original failure](voidls-crash-before.log),
  [debugger stack](voidls-crash-stack.log), and
  [repair details](../IMPLEMENTATION.md#voidls-completion-repair).
  Recovery tests have **472 assertions per run** with the verbatim game, minimal
  fixture, typed/inferred globals, incomplete file/local constants, malformed and
  corrected edits and repeated requests. Actual 1 MiB server stacks passed in
  [Debug](voidls-recovery-debug-stack.log) and
  [strict sanitizers](voidls-recovery-sanitizers-stack.log); Release is in its gate.
* Preserved operator GC/effect/failure integration: **1,169 checks per build**;
  compatibility/cache/reload: **248 checks per build**, all three configurations.
  **344 full source/JIT/O2/linked AOT checks per build** also pass:
  [Debug](operators-aot-debug.log), [Release](operators-aot-release.log),
  [strict sanitizers](operators-aot-sanitizers.log).

## Reproduction

Build the listed targets in the [build logs](debug-build.log), using the SDK and
flags in toolchain.json. Run the literal CTest regex in test-selection.json with
`ctest --test-dir build/ci-official-<profile> --output-on-failure -j 2 -R '<selection>'`.
For strict sanitizers, use these exact settings outside a tracing sandbox:

```sh
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

These settings were exported for all sanitizer tests and native child processes;
leak checking and halt-on-error were never disabled. The SDK uses RTTI OFF, so
UBSan vptr checking is unavailable in this accepted ABI. A local ThinLTO link cache
was used to avoid repeatedly optimizing unchanged SDK archive members.

The final source harness commands are recorded at the start of the corresponding
operator/console logs. Additional existing harnesses use
`build/ci-official-<profile>/void_operator_integration "$PWD"` and
`build/ci-official-<profile>/void_operator_reload "$PWD" <isolated-cache-directory>`.

## Performance and limits

At 80,000 writes, append improves **2.306722 → 0.193547 s (11.92×)** and overwrite
**1.838239 → 0.235401 s (7.81×)**. Three samples at each of 20k/40k/80k operations,
[raw before/after data and method](PERFORMANCE.md) support the measured improvement;
checked geometric growth gives amortized linear total copying. Atomicity and
readonly/alias/snapshot semantics have separate failure-budget proofs above.

Remaining limits: local Linux only; Windows/macOS, physical terminals/keyboards,
native big-endian hardware and multi-GiB boundaries await final acceptance.
`convert.to_string` retains ordinary generic-definition hook visibility; caller
imports are not inherited. Use caller-scoped `string(value)` for those custom hooks.
No new generic lookup rule or automatic error display was introduced.

Implementation and local completeness audit are finished. Stopped for independent
Golden Wrench source/evidence review; no release lock or cross-platform acceptance
is claimed.
