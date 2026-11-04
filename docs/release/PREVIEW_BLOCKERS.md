# VOID 2.0 preview blocker register

4 November 2026. Private qualification; no release authorization.

| ID | Class | Issue / evidence needed |
| --- | --- | --- |
| OPT-LINUX19 | CONDITIONAL | Historical Linux LLVM 19.1.7 DSE cause remains unresolved. [Classification](../log/CRASH_CLASSIFICATION.md), [input review](../log/DSE_INPUT_REVIEW.md). Per Boss's clarification, assess present release risk using LLVM23 evidence: [three bounded DSE-associated checks](../llvm23/ACCEPTANCE.md#bounded-measurements-and-historical-incidents) and [all seven hosted jobs](../net/http/ACCEPTANCE.md) passed. The upgrade may have eliminated an upstream defect; this is unproven. This historical uncertainty is not a demonstrated current-toolchain defect. Exhausted reproduction campaigns stay closed. Golden Wrench/Boss retain risk disposition. |
| OPT-WINDOWS23 | CONDITIONAL | Separate earlier Windows LLVM23 optimizer incidents remain unresolved. The [HTTP acceptance](../net/http/ACCEPTANCE.md) records that the intermittent Windows Debug failure did not recur in the accepted seven-job run. Assess this current-toolchain evidence separately from Linux LLVM19; passing CI does not prove causal repair. Golden Wrench/Boss retain risk disposition. No crash campaign authorized. |
| LOG-PERF | BLOCKER | Historical correctness 263/263 does not close logging performance. [Authoritative handoff](../log/HANDOFF.md). `text.translate_scalars` retains its crash stop condition. No logging work in this assignment. |
| LICENSE | BLOCKER | Boss has not selected a public redistribution license. Private artifacts only; dependency notices do not grant VOID distribution authority. |
| INSTALL-LINUX | CONDITIONAL | Fresh Ubuntu 24.04 x64 container source build, install, relocation, installed stdlib/native execution and LSP handshake passed. [Qualification](LINUX_CLEAN_HOST_QUALIFICATION.md) is ready for Golden Wrench review. Separate-machine/VM and other ABI acceptance are unverified; container userspace shares the development kernel/CPU. No new environmental blocker occurred. |
| DOC-FIRST-INSTALL | CONDITIONAL | Protected README specifies LLVM19; source guide omits required HTTP preparation/CA/source cache and full install workflow. Current recipe and exact gaps are documented in the qualification report, without altering protected or historical records. Boss authorization is needed for README changes. |
| PLATFORM | CONDITIONAL | Windows/macOS source-install qualification follows Linux; hosted regression CI does not establish fresh installation/editor portability. |
| INSTALL-PORTABILITY | CONDITIONAL | Installed manifest retains absolute SDK paths and generated native executable RUNPATH retains the install prefix. Source installation works with those declared prerequisites; portable binary/application relocation is not established. No speculative repair authorized. |
| NOTICE-LLVM | CONDITIONAL | Actual CMake installation includes HTTP/common notices and GNU corresponding-source archives, but no LLVM notice rule/file for linked LLVM. Separate notice/license review is required before distribution; this is not a legal clearance decision. |
| BUILD-WARNINGS | FOLLOW-UP | Existing date, console and proof-source warnings retained in the fresh build receipt. Build exited 0 with no crash/sanitizer marker. No warning suppression or production repair performed. |
| EDITORS | FOLLOW-UP | A server handshake alone cannot establish actual editor usability; existing editor evidence remains bounded to its recorded versions/platforms. |

The narrow Linux container installation checks now pass; no release hold is
closed by that result. Historical optimizer uncertainty is conditional risk,
not a demonstrated current LLVM23 defect. Golden Wrench reviews
evidence; Boss controls repairs, Git, hosted CI and publication decisions.
