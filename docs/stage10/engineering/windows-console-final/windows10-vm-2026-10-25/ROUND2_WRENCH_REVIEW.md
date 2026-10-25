# Golden Wrench independent review — Stage 10 Windows native Round 2

**Review date:** 2026-10-25

**Decision:** ACCEPT the bounded Round 2 native evidence for the cases executed and APPROVE the **test-only instrumentation** for Boss-controlled integration. **Stage 10 remains INCOMPLETE; no release lock, no hosted CI dispatch, no production-code repair authorized by this review.**

## Evidence and provenance

- The privately supplied archive is named `Debug-ClassicConsole-key-closed-calibrate-ctrl-os-transport-11.zip`, but is a comprehensive Round 2 directory archive, containing `REPORT.md`, test patch, full receipts, source/build identities, original physical attempt records, failed trials and diagnostic helpers.
- Current supplied archive SHA-256: `9255c9d71d4a04ab89dbbd603178d680000c543c34cf066e7b170a501225facc`; ZIP CRC test passed. The archive contains **2,447 entries**. All **2,431 files** covered by its `evidence-manifest.json` match the listed SHA-256 and exact sizes. Unlisted entries are three generated `__pycache__` files and directory entries; the manifest itself is intentionally self-excluded. This verifies internal consistency, not independent origin attestation or native re-execution.
- The archive includes private machine paths and host identifiers; **do not commit or publicly upload it**, the raw `complete-working-tree.patch`, executable/source build products or launcher receipts. Retain the original privately.
- Windows test source provenance: historical checkout `046fcf152a786ce6fe27d58ea4bfb709028f4a21` with a later authorized docs integration. **Current GitHub `main` for patch reconciliation was `d5c636775f4778b3fa21c987738a43ceb917562e`.** The Git blob IDs of the four edited existing files (`CMakeLists.txt`, `console_keyboard.cpp`, `console_transfers.cpp`, `console_wide.cpp`) match the old IDs in `test-instrumentation.patch`; the five newly added Windows test files were not in `main`.

## Independent record inspection

| Boundary | Inspected result | Scope |
|---|---|---|
| Genuine Boss-coordinated keyboard captures | **7 successful Debug ClassicConsole captures / 60 returned KeyResults** | Recompared every reported key's identity, modifier mask, down/up bit, repeat and bytes to each consumed native `ReadConsoleInputW.KEY_EVENT`, and checked initial/final in-process modes/code pages. All 60 match. Input producer disabled in receipts. Explicit individual operator attestations are not provided for every entry, so do not upgrade pending fields to a full independent attestation. |
| Attached native console and terminal | **32/32 successful**, Debug and Release | 8 distinct scenarios each across ClassicConsole and Windows Terminal; tested native mode matrix, keyboard, wide I/O, nested shutdown, cleanup failure and fault stress. Host-specific identity and Terminal/ConPTY observations were retained. |
| Real `WriteConsoleW` prefix | **4/4 successful** | Native prefix count and UTF-8 byte progress matched, with an actual cancellation after progress. Explicit request clamping is **separate fault stress**, not an asserted naturally short OS completion. |
| Cooked `ReadConsoleW` | **8/8 completed cases successful** | Two host kinds and two configs: pending input, targeted cancel and invalid private handle. One earlier **PowerShell startup crash** is preserved, and the unexecuted case passed on a separate retry. Windows returned TRUE/zero units for canceled cooked input in observed cases; the Runtime represented that completion as EOF. |
| Borrowed synchronous/overlapped pipes | **8/8 successful** | Native pending requests observed using kernel APIs, stable buffers and precise roots through GC/cancellation/completion, targeted isolation, retained borrowed handles and post-close reuse. |
| Message-mode partial pipe reads | **4/4 successful** | Actual `ERROR_MORE_DATA=234`, 4096-byte accepted prefix, remaining 4096 bytes subsequently read. |
| OS-driven host input | **32/32 selected successful cases** | Actual Windows host-window `SendInput` path, including ConPTY, **AUTOMATED**; three earlier failed attempts retained, not relabeled. |
| Console Host clipboard menu | **2/2 successful** | Real host paste path via automation (Debug and Release), not a physically typed key. |
| Focused native tests | Debug **3/3**, Release **3/3**; source lifecycle **1/1** each | JUnit and native CTest receipts retained. |
| JIT/O2/JIT-vs-linked-AOT source cases | **32 executions + 16 emit/link operations successful** | Separate uninstrumented compiler/Runtime binary hashes reported stable; native binaries not executed again by Wrench. |

The physical modifier, key-up, and repeat observations are **real human-coordinated keyboard evidence** as supplied by Boss, **not synthetic test cases**. In the held-Up sample Windows delivered several key-down records each with repeat `1` and one key-up; VOID reflected each count unchanged. The manual evidence and Windows API host-generated test evidence are distinct, complementary layers.

## Patch inspection and disposition

- `test-instrumentation.patch` SHA-256: `5a61197e8772ba1a5584f56530db97154966169e4ffa3cd06ccdcd7a2a0473da`; **nine file paths** changed: one `CMakeLists.txt`, three existing runtime test files and five new Windows test helpers/observers. **No production `src/` files modified.**
- The `WIN32` build path creates a test-only observed object from the existing Runtime console source using wrapper functions around actual imported Windows APIs. The ordinary `voidc.exe` and Runtime library remain uninstrumented according to available build identities. Optional environment variables enable observations; original tests/assertions remain and known failed attempts are retained.
- Source and API semantics align with Microsoft's published `CancelIoEx`/`CancelSynchronousIo` model: cancellation request is best-effort; completion decides the actual result; native buffers/OVERLAPPED must stay alive until completion. See https://learn.microsoft.com/en-us/windows/desktop/FileIO/canceling-pending-i-o-operations .
- Static review plus matching Git blobs supports patch integration; **no fresh Windows compilation or hosted CI was executed by Golden Wrench**. Actual `git apply --check` and focused local build are required on the up-to-date Linux checkout. Boss alone commits/pushes/dispatches CI.
- **Do not apply `complete-working-tree.patch`**: it contains previously reconciled documentation plus a historical raw archive; blindly applying it would conflict with the already-published privacy-safe Round 1 docs and could reintroduce private raw data into public Git.

## Limitations and exact final gate

1. No new **human-pressed non-ASCII physical** receipt and no full new human physical Release/Terminal batch. Boss has expressly ended manual keyboard participation. This does **not** diminish the 60 successfully observed physical records; it does prohibit calling new synthetic tests *human* captures.
2. All requested native behaviors in the available Round 2 suite passed, with historical failures and one non-VOID-attributed pre-fixture host startup crash retained. None demonstrates a production repair requirement. Rendering/font fidelity remains outside these byte/API observations.
3. To close non-ASCII device-path coverage **without further Boss manual work**, the separate Linux VirtualBox host can inject virtual keyboard scancodes into the Windows VM (documented by Oracle: https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/vboxmanage.html ). This exercises an emulated keyboard-device path, **not human physical keys**; the distinction must remain explicit. It requires opt-in automation and checks against raw consumed native records, not invented success. Existing Windows `SendInput`/Terminal and Console Host paste tests already cover non-ASCII processing in separate non-physical channels.
4. Finish the stage **without deferral**: reconcile and execute the integrated focused tests on current source, independently review any new failure, resolve the remaining acceptance classification based on actual proven native behavior, then let Boss authorize final stage-lock CI. A green CI run alone does not promote any genuinely unexecuted physical scenario to an executed one.

**Golden Wrench:** Strong bolts, no ghost bolts. This is a GO for scoped test-evidence integration, not a self-issued Stage 10 release lock.
