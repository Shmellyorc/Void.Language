# HTTP/HTTPS acceptance

4 November 2026. **ACCEPTED — cross-platform hosted CI green.** Golden Wrench
closes the HTTP/HTTPS cross-platform acceptance workstream at commit
`1afd881ecfa65c616261e7aeb76ecd2bf87770b6`.

[Run 41815539463](https://github.com/Shmellyorc/Void.Language/actions/runs/41815539463)
completed successfully using **Compiler link experiment**, mode **thinlto-cache**,
LLVM/Clang/LLD **23.1.4**. Independent read-only GitHub run/job API checks confirm
all seven job conclusions and each job's Execute tests step are successful.
[Exact run and job receipt](evidence/ci41815539463-acceptance.json).

| Hosted configuration | Result |
| --- | --- |
| Ubuntu 24.04 Release | PASS |
| Ubuntu 24.04 Debug | PASS |
| Ubuntu 24.04 Sanitizers | PASS |
| Windows Server 2022 Release | PASS |
| Windows Server 2022 Debug | PASS |
| macOS ARM64 Release | PASS |
| macOS ARM64 Debug | PASS |

## Accepted repairs and scope

The Windows Schannel caller-provisioned store identity delivery and subsequent
connection-scoped HTTP/2 ALPN retention repair have cleared the full matrix.
**The Windows Schannel HTTP/2 ALPN retention acceptance issue is resolved.**
Connection identity/socket ownership, explicit incompatible-protocol rejection
and replay boundaries retain their tested guards. Normal regression coverage
continues. No further HTTP-specific repair sprint is warranted by this result.

The accepted Windows identity capability is explicit `windows_store` with
caller-owned provisioning. Supplied P12/PEM are rejected before provider work
under the approved Windows contract. This acceptance does not introduce the
previously researched credential-persistence strategy. Other certificate-store
locations/providers, hardware credentials and TLS combinations retain the limits
in [Windows identity acceptance](WINDOWS_SCHANNEL_ACCEPTANCE.md).

## Retained history and independent issues

- [HTTP/2 triage](CI41719924121_TRIAGE.md) retains original failure evidence,
  root-cause analysis, controlled reproduction, connection-proof ownership and
  negative controls. Its dated local-review/hosted-pending statements describe
  the earlier checkpoint and are superseded by this acceptance result.
- The intermittent Windows Debug LLVM optimizer failure did not reproduce in
  the accepted run. Its independent cause remains unresolved. A green run does
  not establish permanent elimination of every intermittent compiler failure.
- [Provider memory review](MEMORY_REVIEW.md) retains the distinction between
  admission reservations and total-process memory, malformed-input proof limits
  and host-dependent trust allocation. Those limitations are preserved within
  the accepted tested scope.
- Windows strict ASan/UBSan support retains its documented native toolchain
  limitation. The accepted seven-job matrix includes Ubuntu strict sanitizers
  and Windows Debug/Release. No Windows strict pass is inferred.
- Broader networking features and untested platform/provider combinations remain
  outside this acceptance decision. The separate logging performance/release
  hold remains tracked in [logging handoff](../../log/HANDOFF.md).

All original investigations, unsuccessful attempts, repairs and tests remain
preserved. This checkpoint updates documentation only. Boss retains Git and
hosted CI authority.
