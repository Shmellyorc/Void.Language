# Golden Wrench — Sprint 4 integration repairs

Baseline: `a4fd7fb0b79c1fb2ac17073bde492118a50516b3`.
Both requested repairs are complete locally. **Stop for Golden Wrench review.**
This receipt is separate from the original Sprint 4 acceptance and incident record.

## Exact changes

- `src/ir/module.cpp`: both `Op::random_xor` and `Op::random_seed` now require
  `f.module_identity == "/@stdlib/random.void"`, matching other private-operation
  ownership checks. Existing operand/result/coordination checks remain intact.
  Ownership follows the containing function, including canonical imports into
  project modules. XOR remains pure and valid without coordinated Runtime entry.
- `tests/random/host.cpp`: independent canonical XOR and seed operations validate.
  Five forged identities (empty, project main, project random, canonical prefix
  lookalike and canonical symbol suffix) are rejected under both project and
  spoofed canonical roots: **20 ownership rejection cases** across the two ops.
  Deterministic worker IR explicitly validates; a forged ordinary call into the
  canonical entropy provider still fails with `worker cannot acquire OS entropy`.
  Existing malformed-operation tests remain. The host total increases from
  **2,281 to 2,325 assertions**.
- `src/frontend/compiler.cpp`: the random-name documentation fallback in
  `editor_call` now requires the candidate unit's exact canonical random identity.
  Exact-target documentation lookup retains its ordinary behavior. These are the
  only **three added production predicate clauses**. No new handler or local
  state was introduced; actual 1 MiB regressions pass.
- `tests/random/lsp.py`: a real imported module supplies conflicting module
  `rand_i32` and receiver `rand_f32`/`try_rand_f32` declarations. Tests assert
  canonical hover prose, signature labels and exact active maximum/lower/upper
  documentation, plus positive controls for the unrelated module's own docs.
  Existing completion, navigation, reference and readonly rename checks remain.
  The total increases from **40 to 61 checks**. Project-scoped working-directory
  handling lets the existing crash capture retain both imported and entry bytes.

[Exact four-file patch](evidence/changes.patch).
The numerical source, public APIs, seed replay fixtures, native entropy bridge,
backend, readonly slicing implementation and stack repair are unchanged.

## Focused verification

| Linux profile | CTests | Elapsed test time |
|---|---:|---:|
| Debug | 12/12 | 77.59 s |
| Release | 12/12 | 26.82 s |
| Strict ASan/UBSan/LeakSanitizer | 12/12 | 73.88 s |

**36/36 CTest executions passed, with no failed or skipped tests.** Each profile
ran this exact selection:

```text
random_host, lsp_random, random_frontend, random_frontend_stack,
random_worker, random_reference, core, frontend, numeric, modules,
lsp_documentation, lsp_binary
```

Each profile reports **2,325 host assertions**, **61 random LSP checks**,
**116 normal + 116 actual 1 MiB frontend processes**, and **8 worker processes**
covering source normal/O2 execution and independently linked AOT modes.
Host coverage retains seeded replay, function values, globals/reload, GC,
allocation failures, readonly collection ownership and checked native entropy.
The independent reference and existing general compiler/LSP suites also pass.
No full test matrix or full random execution-vector matrix was rerun.

The existing Clang/LLVM 19.1.7 profiles rebuilt `voidc`, `voidls`,
`void_random_host`, `void_tests` and `void_io_runner`. Compiler flags are retained
in [configuration.json](evidence/configuration.json). Strict options were:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Strict tests ran outside the ptrace sandbox, using the already established
LeakSanitizer execution path; leak detection remained enabled. No sanitizer
diagnostic or new native crash occurred. Linux results add no Windows/macOS native
execution claim. JIT-generated instruction instrumentation has the same limits
as the original acceptance.

Exact build/test commands, status, durations, JUnit, complete verbose output,
source hashes and executable hashes are in `evidence/build-*.json`,
`evidence/test-*.json`, their adjacent logs/XML and
[FINAL.json](evidence/FINAL.json). [verify.py](verify.py) records the commands.
The separate [repair manifest](MANIFEST.json) hashes all retained repair files.
An additional instrumented Debug LSP invocation passed 61 checks; its original
source/log/receipt are retained under `evidence/additional/`, outside the 36 CTests.

## Baseline reproduction and preservation

The unchanged baseline Debug IR library accepted all eight independent probe
combinations: both operations with canonical, empty, project and lookalike owners.
The baseline LSP returned `CONFLICT maximum documentation.` for canonical
`rng.rand_i32(`. The instrumented reproduction shut down the server cleanly before
reporting the expected Python assertion. Original probe source, commands, output,
LSP source/overlay/import bytes, stdin and hashes are retained under
[baseline evidence](evidence/baseline/MANIFEST.json).

[preservation-before.json](evidence/preservation-before.json) captured **932
existing random documentation/evidence files** before this repair. Final hashes
match all 932 files byte for byte, including the historical timeout/SIGKILL,
sandbox incidents, classifications, manifests and source freezes. No original
incident was removed, relabeled or overwritten; all new receipts live here.
There are no new incident receipts in this repair. Root README SHA-256 remains
`c287f35842b03a1c5ac170718e674cc5498f877ade76eede5273859921d62add`.

No staging, commits, branches, pushes or hosted CI occurred. Engineering stops
for independent **Golden Wrench source review**; no approval or release is claimed.
