# Golden Wrench independent review — Stage 10 final Windows native acceptance (25 October 2026)

**Decision: ACCEPT the five measured Windows native behavior boundaries for the explicit evidence scope. GO for Boss-authorized final hosted Stage 10 CI. Stage 10 remains INCOMPLETE and NOT LOCKED until that CI and Boss's final release decision.** This record supersedes earlier Windows "UNVERIFIED" *status summaries* for the specifically completed native observations; it does not erase earlier failures or turn unexecuted scenarios into passes.

## Provenance and independent verification

- GitHub `main` was independently checked at `6175e83e4ca691fa5f0d4f925c734891296e0a9c` (`test: add Windows native console verification [skip ci]`). The Windows VM's local HEAD remained `046fcf152a786ce6fe27d58ea4bfb709028f4a21`; this is **not** a pristine checkout at `6175e83`. Its nine compiled CMake/test inputs, preserved as raw Windows snapshots, matched GitHub's exact reviewed blobs **after CRLF normalization**; the 20 interposed changed paths were documentation and test plumbing, not production `src/` code.
- The supplied private final Windows archive (named for its last Unicode attempt but containing the complete final-acceptance directory) passed ZIP CRC validation and **228/228 outer manifest SHA-256/size checks**. The nested immutable Round 2 archive passed ZIP CRC validation and **2,431/2,431 SHA-256/size checks**. No unlisted member remained. These are checks of the recorded bytes, not a claim that Golden Wrench independently witnessed Windows executing the tests.
- Independent audit re-parsed the selected **11 human-origin sessions**: seven Round 2 Debug Console Host attempts with **60** returned key events, plus four final Unicode captures with **40**. All **100 returned KeyResults** matched the corresponding consumed native `ReadConsoleInputW.KEY_EVENT` records for identity, Win32 modifier-bit mapping, down/up, repeat, character data and completion counts. Child before/after console state snapshots matched in all 11; four new successful sessions have distinct Boss affirmative replies. Original Round 2 attestation limitations are retained.
- The four new Unicode captures returned `U+00E9` (`é`), UTF-8 `C3 A9`, no reported native/cleanup error: **Debug Classic 13 events**, **Release Classic 15**, **Debug Windows Terminal 10**, **Release Windows Terminal 2**. Different host translations are preserved rather than forced to a common number of events. The input producer was disabled during those human cases.
- Fresh integrated focused CTest XML/logs: **Debug 3/3 and Release 3/3**, no failures or skips. Fresh real-host mode matrix: **4/4 child exits 0**, in-process masks and code pages restored; actual invalid native mode/error `87` cases retained. Both Ninja build commands succeeded with no rebuild required and recorded binary/cache identities stayed stable.

## Five required Windows native boundaries

| Behavior | Independently reviewed recorded evidence | Decision for tested conditions |
|---|---|---|
| Physical keyboard metadata / Unicode | 60 previous + 40 new human-origin native KEY_EVENT-to-KeyResult matches; Shift/Ctrl/Alt, down/up, real repeat records, and four `é` sessions across both hosts and both builds. The previously accepted four Round 1 Up tests are separate and not double-counted. | **PASS** |
| Exact nested mode restoration | 32 accepted real attached-host cases, fresh 4/4 mode matrix and 11 human sessions. Child-process input/output/error mode masks and input/output code pages verified through nested begin/end, non-LIFO rejection, Runtime close/destructor and unclosed shutdown. Actual cleanup failure `6` is reported, with host recovery distinguished from Runtime success. | **PASS** |
| Classic Console Host and Windows Terminal/ConPTY | True Console Host and installed Microsoft Windows Terminal observations, attached client console handles, Terminal/OpenConsole/PseudoConsoleWindow evidence. Native and automated input paths kept separate. | **PASS** |
| Native wide I/O and UTF-16 progress | Actual ReadConsoleW/WriteConsoleW call records, Unicode scalar/NUL/BOM/surrogate checks, **4/4** successful prefix-progress cases (`2048` UTF-16 units / `4096` UTF-8 bytes), and **8/8** completed cooked read cancellation/error cases. Preserved `TRUE/0/error 0` completion-as-EOF; do not invent error 995 where Windows did not supply it. Explicit half-surrogate request clamping and private-handle faults remain labeled stress conditions. | **PASS** |
| Borrowed I/O cancellation and retention | **8/8** synchronous/overlapped pipe probes, genuine pending OS request, targeted cancellation, separate foreign request isolation, precise roots/buffer lifetime through completion and post-close borrowed-handle usability; **4/4** native partial message-pipe cases retain `ERROR_MORE_DATA (234)` and the exact `4096+4096` byte progression. | **PASS**, existing best-effort syscall cancellation contract |

The 32/32 earlier native host scenarios and 32/32 selected OS SendInput/host transport cases remain valuable **separate automated evidence**, never mislabeled as human-origin. Prior source lifecycle and normal/O2 JIT and independently linked AOT work (32 executions plus 16 emit/link commands) were accepted and reused, not rerun. The earlier 7/7, 1,383/1,383 experimental CI and exact Windows/macOS 1 MiB stack gates remain historical accepted receipts, **not** a new CI run at this source commit.

## Failures, non-claims and explicit release boundaries

- **Preserved failure:** First Debug Classic Alt+0233 attempt used the top number row, yielding no `é` and child exit 1. A separate right numeric keypad attempt succeeded. All earlier Round 2 failures remain inside the private nested archive; one PowerShell pre-fixture startup crash has no demonstrated VOID cause, and its never-executed case succeeded on retry.
- The full new **human Shift/Ctrl/Alt/repeat gesture-by-host-by-build cross-product was not executed**. Human modifier/repeat coverage is from Debug Classic; both hosts and builds have physical Unicode and automated native host-transport coverage. Do not label unexecuted human combinations as physical passes. The measured Windows native behavior obligations above are accepted on the combined, explicitly bounded physical and native automation evidence; no further manual participation is demanded of Boss.
- VirtualBox host-scancode injection was **not** executed: guest-to-host SSH was unavailable. It is not required to invent a device-origin result because real human-origin Unicode observations were subsequently captured.
- Screen-buffer text and Win32 character counts **do not certify font glyph rendering, colors, or photographic appearance**. Any graphical fidelity obligation must be verified under its own actual acceptance criterion, not inferred from this audit.
- Windows/macOS Vim/Neovim host runs and a physical second Windows volume remain unexecuted in this package. These are recorded as additional editor host scope and a prior Stage 9 exclusion respectively, **not** falsely reclassified as passing Windows console cases. If Boss retains them as required Stage 10 release gates, they still require corresponding native evidence before locking; this review does not waive them.
- The CMake additions are gated to `WIN32` and compile test-only wrappers against the existing production console implementation. Existing production `voidc` / `void_runtime` stay uninstrumented. No production defect, production repair, additional test-source change or hosted workflow run was evidenced by this last package.

## Final disposition and privacy

**Golden Wrench: Windows native acceptance GO for the five measured axes. GO for one Boss-authorized final full seven-job hosted CI run against the integrated current `main` and subsequently owner-controlled Stage 10 lock review. Not an independent stage lock; investigate any CI regression.** Do not defer a demonstrated production defect, weaken a requirement, or move unverified mandatory acceptance to Stage 11. Historical stage/status paragraphs elsewhere remain snapshots from before this verification.

Do **not** publish the raw ZIP, nested Round 2 archive, machine paths, window/session IDs, raw private reports, full patches or command receipts. Keep complete evidence privately with Boss; publish this audit summary only. Cryptographic receipts from the owner's provided archive:

- Final private archive SHA-256: `186ddc86bf70bdd3dd6da98324cbfa9dea44efb55b26842e35f32a62a68ec8f4`.
- Final internal manifest SHA-256: `3e45dd986dfbb5d04f3e267d93137963baa7e73c01b009da11192b73b5d610bd`.
- Preserved nested Round 2 archive SHA-256: `ebf8e89584b91949707be7e3a2da5936495f61e7714e0dfea8b3089cea96569c`.
- Nested Round 2 manifest SHA-256: `4e238c2c425f8ec7a8a5e199ec9969732626455d3c600231cb269adbf7d6b33d`.

**THREE CLANKERS. ONE VOID. STRONG BOLTS. NO GHOST BOLTS!**
