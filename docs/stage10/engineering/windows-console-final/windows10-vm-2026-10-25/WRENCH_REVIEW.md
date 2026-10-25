# Golden Wrench review — Windows 10 native console VM, 25 October 2026

**Decision: ACCEPT the bounded VM evidence and the documentation-only observer repair. Stage 10 remains INCOMPLETE; no release lock.** This report supersedes the 24 October "no interactive Windows host" statements *only for the precise Windows Console Host cases observed here*. Earlier files remain historical evidence, not edited-away failures.

## Source and scope

- Test host: Boss's Windows 10 Home **22H2 (19045.6456), x64** VirtualBox VM. Fresh native PowerShell **5.1.19041.6456** sessions created with `CREATE_NEW_CONSOLE` and no redirected standard handles; direct console handles checked. Host identification recorded **conhost.exe 10.0.19041.1** and `ConsoleWindowClass`. This is **classic Console Host, not Windows Terminal/ConPTY**.
- Prior Git receipt: `046fcf152a786ce6fe27d58ea4bfb709028f4a21` on `main`; acceptance session performed **no Git operations**. Windows checkout used `core.autocrlf=true`. The historical 47 raw-hash differences were reconciled to LF→CRLF; 260 actual source/build-input bytes were unchanged through the final physical check. No claim of a byte-identical LF worktree or a new Git HEAD query during tests.
- Built **only** `voidc`, `void_runtime_console_keyboard`, and `void_runtime_console_terminal` for Debug and Release using the existing CMake/Ninja configurations. Both builds succeeded; CMake caches and six executable hashes matched pre-test records. The test host had MSVC 2022 Build Tools 17.14.42, MSVC 14.44.35207, Windows SDK 10.0.19041.0, DIA SDK and complete LLVM/Clang **19.1.7**.
- The 7/7, 1,383/1,383 cross-platform experiment #4 and separate Windows/macOS native 1 MiB stack runs were **previously** accepted. No full seven-job CI, stack gate, JIT/AOT matrix or hosted workflow was rerun by this VM exercise.

## Independently checked uploaded evidence

| Test boundary | Native evidence | Status |
|---|---|---|
| Focused CTests | Debug **2/2**, Release **2/2** (`runtime_console_keyboard` **131 checks** and `runtime_console_terminal` **482 checks** per configuration); JUnit and `LastTest.log` were examined | **4/4 passing**, injected regression scope only |
| Real Console Host flows | Five modes in each configuration (key closed/unclosed, shared input, Unicode prompt, presentation); each child had attached handles; `WriteConsoleInputW` supplied test input and output CP was deliberately **65001** | **10/10 passing**, **scripted input, not physical** |
| Physical Up-arrow press/release | Boss attested four distinct runs after READY with **no scripted input**: Debug/Release × closed/unclosed. Each had child exit **0** and two printed `5 1 1 1` rows | **4/4 attested and machine-receipted**, narrower than full modifier coverage |
| Parent-facing state | In successful sessions, parent input/output/error modes **[487,7,7]** and input/output CP **437/65001** matched before/after | **Observed only at parent boundary**, not in-process proof |
| Artifact integrity | I independently SHA-256 checked all **366** frozen evidence entries and all six executable bytes against their reported hashes; the compiled LLVM/libs were not used as a new CI acceptance claim | **Pass for recorded bytes** |
| Observer fix | PowerShell source observer initially used local `$LASTEXITCODE = $null`, which shadowed native exit status in nested script contexts. Reviewed successful sessions used a copy with exactly `$global:LASTEXITCODE = $null`. Reverting the one change reproduces original SHA-256 | **One-line docs/tooling repair included**; patched repo copy itself was not rerun here |

The complete original Windows evidence (406 native files plus nine toolchain receipts, including historical failures and unconfirmed attempts) is **retained privately by Boss**, not embedded in this public repository because original receipts expose machine-specific paths and host identifiers. The unchanged archive SHA-256 is recorded in [the public manifest](evidence-manifest.json); its authoritative raw report remains `native-acceptance-1/REPORT.md` inside the private archive. [Machine review summary](review-results.json) and [path-free derived records for all ten scripted flows](automatic-flow-results.json) support public inspection. These derived records omit absolute launch commands; they are **not** falsely represented as byte-identical raw receipts.

## Important observations, not deleted by later passes

- Several default-output-codepage **437** key-closed attempts failed; one had pre-queued Enter records. Later separate successful sessions were fresh and explicitly configured output CP 65001. No code-page change was made inside the VOID Runtime.
- Scripted default-437 shared runs produced all expected Unicode Runtime output records, but the final **`DONE:├⌐`** narrow C++ `std::cout` trailer differed from **`DONE:é`**. The binary UTF-8-to-CP437 interpretation is consistent with an output-codepage effect in the test fixture; there is **no Win32 API trace to establish the precise cause**. Do not describe default-437 exact-trailer behavior as passed.
- Earlier manually supplied Unicode prompts included inconsistent clipboard content and a Debug `invalid_encoding` error. A zero exit did **not** make those runs passes; later exact scripted UTF-8 greetings cannot certify those earlier physical/clipboard pathways. The unconfirmed Debug unclosed attempt with no observed window is excluded from the four physically attested successes.
- Screen-buffer captures use `ReadConsoleOutputCharacterW` (character data), not exact output bytes or photographs of emoji rendering. Presentation operation error codes and an intentional invalid-title rejection were recorded; audible bell/rendered palette fidelity were not certified.

## Native acceptance still outstanding

1. **Windows Terminal / actual ConPTY host** was not installed/discovered or exercised; `WT_SESSION` alone would not establish genuine ConPTY identity.
2. **Full physical modifier/down/up/repeat metadata** was not printed by the existing fixture. Four successful Up rows do not prove actual modifier masks, key-down booleans, repeat >1 or other virtual keys.
3. **Child-process nested mode and code-page state**: the read-only parent observer cannot prove every active/restored in-process state, error code, non-LIFO close or unclosed shutdown restore.
4. **Native ReadConsoleW/WriteConsoleW API counts, partial/error cases, UTF-16 edge sequences**, hidden NUL/BOM and malformed-surrogate handling were not traced. Correct visible/screen text is not full boundary acceptance.
5. **Borrowed synchronous/overlapped handle cancellation, isolation and retained buffers/GC until native completion** remain unverified; existing Windows standalone native cancellation cases are absent.

No verified production compiler/Runtime defect is concluded from this package; the earlier failures and incomplete cases remain for later targeted investigation. Do not add an injection framework to manufacture missing host observations, run unnecessary full CI, or silently upgrade a non-executed gate. Boss decides whether to defer the remaining host-specific qualifications or authorize further genuine native instrumentation before Stage 10 release. **No Stage 10 lock or Stage 11 start is implied.**

## Package integrity

- Uploaded original `windows-console-evidence.zip` SHA-256: `d61e4769b3a3ba080976cadc8ec0e116fd27d2256b29625ecc7f2c70049199a8`; **3,670 entries, 109 MB compressed on disk**, containing large generated build products that are deliberately excluded from the GitHub documentation bundle.
- **Private**, unchanged curated native-evidence archive SHA-256: `f43d68604a06d6319c68767a6ff76c9eb08a65c28fdcde5d4bb0f9fa7becd4cc`; **406** original native files preserved without content modification, plus **9** selected toolchain text receipts.
- The private original frozen evidence index has **366 SHA-256 entries; all 366 matched** their source ZIP bytes. Live launch receipts excluded from that historical index remain in the native archive without being relabeled frozen.
- Baseline source for this documentation-only increment: GitHub `main` at `046fcf152a786ce6fe27d58ea4bfb709028f4a21`. The accompanying patch modifies only existing `docs/` records and repairs the observer's one native exit-status initialization. All original production code, tests, workflows and root README remain out of scope.
