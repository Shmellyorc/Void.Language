# Original attempts and corrections

All raw runs, sources and logs are retained in the
[engineering archive](evidence/implementation-evidence.tar.gz), authenticated by
its [receipt](evidence/IMPLEMENTATION_ARCHIVE.json). The original raw tree remains
in `build/channel/evidence-raw`. Expected frontend rejections and the deliberate
old-pruning control are successful negative tests, not compiler failures.

| Attempt | Finding and correction | Retained reproduction |
|---|---|---|
| First source declaration | VOID structs require fields; `pass` is not an empty-struct declaration. The opaque handle now has one private const Boolean and a private initializer. | `attempts/channel-01.void`, reproduced check stdout/stderr. The first tool response also contained the harmless unsupported `voidc --help` invocation. |
| First specialization | A nonnullable local cannot await branch initialization. Topic acquisition now returns a held bucket from a private state helper, wholly before publication. | `attempts/channel-02.void`, `attempts/local-01.stderr`. |
| First target build | A newly added target was requested before CMake reconfiguration. Reconfigured the existing build, then built normally. | `build-logs/build-release-01.log` and subsequent configure/build logs. |
| First two host runs | The observer called `Runtime::Handle::get()` without an attached Managed participant. The harness now observes through the existing Attachment/Managed authority. No Runtime exception or root check was weakened. | `implementation/run-cn1skeoc`, `implementation/run-jqu05dn8`; later source snapshots and successful runs. |
| Reload fixture | `elif` is unsupported VOID spelling. Replaced it with existing `else if`. | `attempts/reload-elif.void`, `implementation/run-sq82808s`. |
| Draft topic pruning | An inactive old record could prune a newly created bucket for the same topic after cleanup failure. Compare exact topic bytes across active registrations instead of old bucket identity. | Original draft is in `implementation/run-sq82808s/inputs/stdlib/channel.void`. A separate configured-source control reproduces `inactive cleanup pruned a replacement topic`; its receipt contains `configured-channel.void` and the exact command. The final host sweeps assert recovery across this scenario. |
| Async CLI attempt | Existing ordinary CLI entry execution requires an explicit scheduler adapter. Added channel owner-async coverage through `Jit::compile_async` and `emit_async_object`, then independently linked and executed those objects. No CLI or async permission was added. | `implementation/run-ascmnmzo` and final `channel_host` runs. |
| First example runner | Application `main()` accepts no legacy i32 argument. The harness supplies the legacy argument only for an `entry(i32)` fixture. Expected example output stayed unchanged. | `implementation/run-2snpbs_g`, `build-logs/execution-01.log`. |
| First LSP expectation | The compiler presents source `const` as `readonly` in the concrete callback signature. The test now asserts the actual existing `func(readonly i32)` presentation. | Original assertion/hover output in `build-logs/lsp-01.log`; `attempts/lsp-const-expectation.py` reconstructs the exact old one-line expectation from that correction and is labeled as a reconstruction. Final 55-check LSP runs pass. |
| Reload signature negative fixture | After adding a managed callback frame for active-call pausing, changing the payload to i64 initially made the frame constructor invalid source. Its field and explicit conversion now allow both programs to analyze, so the negative test actually reaches reload compatibility rejection. | `implementation/run-9gqlx2oz`, final reload fixture and successful host runs. |
| Initial benchmark fixture | Unrolling 1024 source registrations made compilation costly. Replaced them with source loops, preserving registration counts and timed checksums. The original run completed successfully; an attempted termination targeted an already exited PID and had no effect. | `implementation/performance-unrolled.json`, `implementation/performance.json`, exact generated sources in each run receipt, `build-logs/performance-01.log` and `performance-02.log`. |

No abnormal compiler/runner exit, timeout, sanitizer report, new LLVM incident or
independently established repair of H1–H7 occurred in this implementation. The
pruning draft defect is a separate demonstrated library bookkeeping error.
