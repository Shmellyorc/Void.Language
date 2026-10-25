# Golden Sacred Wrench: Stage 10 final-native-access package review

**Review date:** 24 October 2026  
**Basis:** Boss-supplied `local-closeout.patch`, `local-closeout.tar.gz`, `HANDOFF.md`, `changed-files(1).json`; read-only GitHub comparison to `92136acaf114a492452a7d84b31f174f5b305def` and CI experiment #4.  
**Disposition:** **Package integrity accepted. Two concrete documentation/provenance corrections required. Stage 10 release lock HELD for genuine native acceptance gaps and Boss decision.**

## Independently reproduced

- SHA-256 `local-closeout.patch`: `61607afa1d74cb7f1d10ab0cf59119399103938764fc3997f2ec4e844e8da921` (matches handoff).
- SHA-256 `local-closeout.tar.gz`: `50c492f20740dd802f85d7e5a9a466f6c892f9fac91f7f121fd6d77ba41ae39f` (matches handoff).
- The archive contains exactly 39 safe, ordinary files, with unique paths corresponding to the original 15 modified and 24 added inventory entries. Every non-null inventory SHA-256 matches extracted payload bytes. Null self/rolling-hash fields are appropriately not interpreted as a successful digest check.
- Binary-capable patch references exactly those 39 paths. A scratch-only `git apply --reverse --check`, full reverse, `git apply --check`, and full forward succeeded; final source payloads re-matched the manifest. **No live project checkout was modified.**
- The 15 restored pre-change files were independently compared with GitHub's tree at `92136aca`: **15/15 Git blob IDs match exactly**. The patch is genuinely based on the claimed revision.
- The patch modifies only documentation and adds an example plus archived evidence. No compiler, Runtime, production test, workflow, dependency or public root README change was found in this 39-file delta.
- All seven preserved compressed GitHub Actions job logs independently decompress and match both their compressed and raw SHA-256 receipts. Their CTest summaries yield **7/7 successful jobs, 1,383/1,383 main CTest executions** at `90c0cf64b9aba60d7c642096d2aaac45aae70d56`. Original Linux Debug and Release frontend reruns are additional, not counted in 1,383.
- The local closeout records contain **28 command outcomes**: 6 check/JIT, 8 native build/execution, 7 design probes, 3 strict compiler configurations and 4 existing-example executions. Exactly one nonzero outcome is the expected `VOID4001` rejection of an application `main(raw: [string])`; no unexpected recorded failures. All successful runnable new-example outputs agree with the declared exact UTF-8 bytes. These are **record checks**, not freshly executed compiler tests on this review host.
- The updated Stage 10 `IMPLEMENTATION.md` has **all 44 unique roadmap rows**. Steps 42, 43 and 44 are not inaccurately marked as a release lock.

## Findings requiring small documentation correction

1. **Historical Step 42 checkpoint is internally contaminated.** In `docs/stage10/engineering/step42-results.json`, `historical_pre_ci4_checkpoint` holds its old date/status but contains *post-CI4* `platforms.windows` and `platforms.macos` status, job lists, passing counts and receipts. The original pre-CI4 platform records (available by reversing this patch or reading original GitHub blob `52cebf0d00ae6f57bb08f1fcd32a25038ed5a339`) must be used inside the historical snapshot. Keep CI4 results in the current fields. This is an evidence chronology issue, not a code failure.
2. **Research provenance lost.** `docs/stage10/EVIDENCE.md` replaced its opening with a current-status section but deleted the original `Research date: 21 October 2026`, inspected source baseline `e25816bdb80e3f32613edf77c9cf3258232af966`, and research-only qualification. Restore those lines beneath a historical-baseline heading while keeping the current acceptance link. Do not rewrite history.

After these two text-only corrections, update the affected JSON/hash receipts and regenerate the exact archive/patch inventory as appropriate; run **focused documentation/patch validation only**, not CI. The current archive must not be described as byte-identical to a subsequently corrected one.

## Language and future-design observations

- The runnable probe supports nullable `if maybe is Ok(var value)` in the current language. This corrects the earlier Wrench conjecture that nullable values could not use this pattern.
- `func main(raw: [string])` is valid to check as a function but is **not** currently selected as the application entry; `VOID4001` is an expected negative.
- Official `stdlib/` and project-local `libs/` remain different import domains; purpose-based `sync/mutex`, `net/http`, etc. use the existing official exact-file resolver and compiler-owned LSP discovery. File-level runtime `var` and broader stdlib modules remain future proposals.

## Native acceptance and review boundary

- The handoff says local execution had Linux only; native Windows/macOS probes: **zero**. Failed VirtualBox inventory calls are *reported*, not independently reproduced here. `2843` preserved-input matches is likewise a supplied follow-up claim: the corresponding ten-file native-final evidence collection was not fully uploaded; the 39-file closeout integrity was independently checked.
- **Windows:** a real PE `StackReserve` readback of `voidc.exe` and `void_tests.exe` (expect 1,048,576) for Debug/Release; exact native frontend/depth probes; genuine traditional-console/Windows Terminal/ConPTY UTF-16, key, mode, code-page, borrowed-handle/cancellation/cleanup observations remain unverified. Existing redirected/injected adapter suites do not replace real console evidence.
- **macOS:** ARM64 Debug/Release original frontend/depth probes with *both* `RLIMIT_STACK` soft and hard values set/read back at 1,048,576 bytes remain unverified. Ordinary macOS 199/199 CTest success is meaningful but not proof of this explicit boundary.
- Native Windows/macOS Vim/Neovim, second Windows volume and a fully controlled same-commit ThinLTO speed comparison are distinct known exclusions, not phantom pass claims.
- Do **not** repeat full seven-job CI merely for docs. With Boss authorization, run only native-focused checks on real supported hosts. Offline PE inspection of existing binary artifacts can settle *metadata alone* if exact binary provenance is available. No speculative Windows/macOS simulation, relaxed tests or invented host access.

## Golden Wrench decision

**GO:** accept the independently reproduced 39-file artifact/CI proof and Step 43's substance, subject to the two small evidence corrections. **HOLD:** Stage 10 release lock until Boss disposes of the native support requirements using real evidence or an explicitly revised supported-scope decision. The limitations found are *verification gaps*, not independently reproduced compiler defects. Neither a new stage nor another routine full CI is authorized by this review.

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS.**
