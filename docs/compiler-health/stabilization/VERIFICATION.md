# Stabilization verification

Frozen production source and build identities are recorded in the evidence manifest. Every executable referenced by the selected CTest commands was rebuilt; the extra Debug cache assertions received an explicit subsequent target rebuild before acceptance. The original failed attempts remain preserved.

| Profile | Passed | Failed | Driver seconds | Receipt |
|---|---:|---:|---:|---|
| release | 94 | 0 | 289.43 | [acceptance-release-1793118874837282109](../../json/evidence/implementation/acceptance-release-1793118874837282109.json) |
| debug | 94 | 0 | 980.95 | [acceptance-debug-1793118701113778258](../../json/evidence/implementation/acceptance-debug-1793118701113778258.json) |
| san | 93 | 0 | 827.64 | [acceptance-san-1793119282860763137](../../json/evidence/implementation/acceptance-san-1793119282860763137.json) |

All three receipts confirm unchanged source and build hashes before/after the pass. Sanitizer means the existing RelWithDebInfo ASan+UBSan configuration, with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. The existing sanitizer selection excludes `lsp_const_recovery_stack`; Debug and Release run it. No test timeout or stack budget was increased.

The selection covers JSON execution/precision/schema/readonly/budgets/metering/GC/failure/stress/install/LSP, shared frontend/generics/numeric/pattern/object/collection behavior, native bindings, JIT and reload, cache processes and forced Runtime instances, eager globals/AOT, closures and async execution/AOT, GC descriptors/callbacks/allocation failures, application workflow, original source CLI stack/recovery gates and crash evidence preservation. Exact selection and test-level results are retained with the receipts.

Linux JSON execution, metering, hint execution and LSP stack probes use or observe actual 1 MiB child stacks. The unchanged semantic-depth suite verifies legal chains and rejected deep chains, exact nesting locations and later-error recovery. The manual frozen-fixture benchmark matrix independently applies a hard/soft 1 MiB limit to compiler, native and audit processes, including inherited AOT linker/child processes. General C++ CTests retain their existing stack environment.

The original baseline compiler generated the exact 2,048-row JSON budget oracle. Every budget 1–1024 is compared across valid nullable/inherited decoding and competing unknown/type/missing errors, in JIT/AOT with/without IR O2. Oracle SHA-256: `7c89496e1987b39f786e8e2cb7cee93487d82f8b701145b519ab931e5cce3462`. This complements existing offsets, paths, numeric precision, ordering, readonly, allocation and GC checks.

The additional manual DTO ladder matrix, Runtime/retention measurements and compiler lifetime cycles are listed with the controlled measurement ledger. Historical H1–H7 remain unresolved observations; passing current suites does not establish their cause or repair. No ptrace-based debugger or tracing was used.

Additional explicit entry-emission evidence exercises six preserved programs
(virtual dispatch, interface views, bound/generic callbacks, escaping closures,
events and cleanup). Baseline output is checked, then all three final profiles
pass 72 JIT/AOT executions plus 36 independent builds on 1 MiB stacks. Another
helper passes 48 async checks across all/entry emission and both IR policies,
covering activation, closures, lifecycle/cancellation and worker spawn. Its first
ad hoc sanitized build omitted the required no-RTTI project flag; that failure
is preserved, and the final helper uses the exact configured profile flags.
The full production selections above were already clean and unchanged.
