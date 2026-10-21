# Stage 8 Sprint 4 local acceptance

Status: **IMPLEMENTED / LOCALLY VERIFIED; Wrench review pending; not hosted locked.**
Baseline: `5b40d892aed7d5a2a5e2ac02f3c9c55917e9fed1`.
Scope and owner decisions: [application/static-provider contract](../STAGE8_APPLICATION_NATIVE.md),
[DECISIONS.md](../DECISIONS.md).

## Implemented work

- Explicit `--app` run/emit policy, selected top-level body or zero-argument main,
  ordinary return validation, status/stdout separation and preserved legacy entry
  precedence. Applications use the existing i32(i32) host adapter, typed IR,
  LLVM 19.1.7 JIT/AOT and Runtime.
- Entry identity in analysis, Debug object cache and reload. Synthetic entry names
  are excluded from written editor declarations; actual local navigation/rename
  still uses compiler validation. Editor analysis cannot execute applications.
- Independent `void.sink.v1` static catalog and C implementation, explicitly
  admitted through `native_sink_project()`. Original fixture remains supported.
  Both use the same private bridge, Runtime registry, resources, owned strings,
  callback lifecycle, precise roots and generation leases. Provider resource
  identities are checked in IR and Runtime; no CLI/LSP native authorization.

## Final consolidated local verification

All final builds completed before their corresponding final CTest runs. No
executables were relinked during these clean acceptance runs.

| Configuration | Final result | Elapsed |
| --- | --- | --- |
| Debug | 148/148; zero failures | 512.25 s |
| Release | 148/148; zero failures | 226.46 s |
| RelWithDebInfo / strict ASan+UBSan+leaks | 148/148; zero failures | 445.58 s |

**444/444 final unique configuration/suite outcomes.** Earlier full attempts,
focused reruns and process-level checks are not added to this suite total.
Complete Debug and Release frontend suites also passed with a real **1 MiB**
stack, including diagnostics, recovery/resource limits, legal/rejected deep
chains, numeric values, value structs, methods and initialization. Gate commands
ran from isolated temporary working directories while reading repository examples.

Strict tests ran outside ptrace with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
No assertions, depth limits, sanitizer categories or leak checks were disabled.

[Machine-readable results](sprint4-results.json) record commands, exact preliminary
and final outcomes and log hashes; [code/test hashes](sprint4-source-hashes.json)
identify the final source. CI #82 repair files and root README retain their exact
hashes, and HEAD remains the specified starting commit.

## Permanent regressions

Three new CTest suites bring the existing 145-suite baseline to 148:

- `application_native_sprint4`: 163 process checks covering explicit CLI,
  status/stdout, main/top-level forms, signed process statuses, invalid signatures
  and missing returns, competing styles, imported bodies, alternate files, legacy
  numeric output, checked failures, fresh-process cache identity, proof-mode
  rejection, and normal/O2 JIT plus independently linked AOT. Native sink main
  and top-level AOT need the explicit C++ admission host; CLI imports reject.
- `application_native_sprint4_host`: descriptor and symbol/signature/flag rejection,
  normal/O2 sink execution, failed acquisition, owned NUL/UTF-8 transfer and
  retention, byte readback, one-shot callbacks, defer/failure cleanup, GC roots,
  application reload and entry-kind rejection, old callback generation leases,
  shutdown retirement, cross-provider identity, thread affinity, source/IR guards.
- `lsp_sprint4`: nonexecuting application analysis, actual local hover/rename,
  protected compiler-owned names, typed-main diagnostics, competing styles and
  untrusted native imports. Linux stdio tests do not establish actual editor-client
  or Windows/macOS execution.

Existing native/source/integrated/shutdown suites cover the shared callback
reservation/drain machinery, foreign-thread dispatch, allocations and failure
cleanup. The sink catalog adds no foreign callback operation or worker privilege.

## Retained engineering failures and corrections

- Preliminary Debug 1 MiB frontend and full frontend failed the obsolete assertion
  that top-level `pass` must be rejected. It is now an executable application
  statement under the approved grammar. The assertion now verifies that selection;
  no depth guard or production return validation was weakened.
- I relinked `voidc` while preliminary Debug/Release validation was active. That
  sequencing mistake caused transient permission-denied/missing-executable CLI
  failures. After builds finished, all six Debug failures and the one Release
  failure passed focused reruns. These preliminary runs are retained and do not
  count as clean full acceptance.
- Review found that the synthetic body was exposed through a fabricated source
  declaration span: hovering the initial `var` showed a function signature.
  Compiler validation rejected the attempted invalid rename, but the hover was
  wrong. The body remains in native compatibility contracts and is now hidden
  from written editor declarations/rename targets; ordinary local rename passes.
  A preliminary strict run reproduced the new LSP regression (147/148 passed);
  final consolidated runs follow this metadata repair.
- The first strict build stopped because LeakSanitizer cannot operate under ptrace.
  The same build resumed outside the tracing sandbox. Leak detection and strict
  ASan/UBSan settings remain enabled; no sanitizer suppression was added.
- Early focused process assertions exposed an unhelpful unsupported-entry message
  and a case-sensitive test expectation for `Type mismatch`; both were corrected.

Raw logs stay in `build/stage8-sprint4-acceptance/`; final tracked JSON records
results, failures, log hashes and code/test hashes. Reruns are not additional
unique suites.

## Limits and next owner gate

One explicit provider descriptor per source graph remains the admission API.
There is no native package discovery, dynamic loading, arbitrary pointer ABI,
source FFI DSL, general IO library, new worker privilege, general argv, async main
or top-level await. Sprints 5–6 remain future work.

Strict instrumentation covers the compiler, Runtime and compiled C providers;
LLVM-generated application instructions are not claimed to gain ASan/UBSan
instrumentation automatically. Preserve existing checked operations and roots.
Windows/macOS execution, hosted CI #82 Debug/Release and fresh actual editor-client
verification remain pending. The seven-job hosted gate remains deferred until the
end of Stage 8 unless Boss explicitly authorizes an earlier run. No Git mutation,
root README edit, remote change, publication or Sprint 5 work is authorized here.
