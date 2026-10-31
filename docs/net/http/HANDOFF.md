# Golden Wrench HTTP implementation and evidence handoff

31 October 2026 UTC. Baseline `15f7868b0af177f8f7d49c548392c79c7bd921d2`;
LLVM/Clang/LLD 23.1.4. **Architecture GO; full HTTP acceptance remains pending.**
Linux implementation and local verification are ready for independent review.
This is a source/evidence candidate, not an independent review or release lock.

## Implemented scope

| Acceptance area | Implementation / evidence |
| --- | --- |
| U — URL | Shared CURLU parser/resolver, HTTP(S) scope, strict UTF-8/percent bytes, IDN, immutable address fields and ordered queries; native/source/IR/LSP checks |
| H/B — HTTP and bodies | Real HTTP/1.1/ALPN HTTP/2, actual response version/status, duplicate/binary headers, metadata/trailers/history; empty/bytes/text/form/MIME/file/stream bodies and checked signed 64-bit counts |
| S — streaming | Fixed native rings, bounded decoder/publication/admission, duplex early response, paused H2 plus fast siblings, source Task/result retirement and repeated await; public 2 GiB source and >4 GiB native bulk proofs |
| F — files | Borrowed existing IO authority/cursor, native bounded upload/download and mixed multipart, same Runtime worker queue, single-worker dependency progress, partial results and queued/entered cancellation; no implicit seek/truncate/flush/close |
| T — TLS | Chain/hostname verification, minimum TLS 1.2, Linux distribution roots, custom replacement roots for origin/proxy, owned CA/PEM/P12 snapshots and mutual TLS; negative peers/corpus tests. Native OS acceptance remains open |
| N — DNS/pools | c-ares UDP/TCP, IPv4/IPv6/dual-stack, IDN, bounded cache capacity and TTL, per-client connections/origins/streams/idle limits, isolation and cancellation; native VPN/search/store scenarios remain open |
| D — deadlines | Connect plus separate header/read/write/total clocks, Digest per-attempt timing, demanded-input/output pause rules, actual TLS/proxy/DNS phases and quiescent held-file timeout |
| A — async/lifetime | Existing task/Work/group/precise-GC model, native completion tickets, shielded acquisition abandonment, structured transfer lifetime, generation leases and two-phase Runtime shutdown; no second source scheduler/pool |
| R — redirect/replay | Relative/absolute policy, method rewrite, history, no downgrade/cross-credential migration/consumed-file replay; peer-observed REFUSED_STREAM POST transmitted once |
| P/C — auth/proxy/cookies | HTTPS Basic/Bearer/Digest; explicit/environment HTTP/HTTPS/SOCKS routing, proxy trust/auth isolation and CGI-safe environment snapshot; provider-owned opt-in in-memory cookies with checked census/limits/clear |
| E/V — representations/protocol | Bounded gzip/zlib deflate, concatenated gzip, checksum/end/trailing-data checks, independent encoded/decoded limits, raw mode, strict UTF-8 text; literal malformed H1 and real H2 reset/GOAWAY/window tests |
| X — systems | Normal/O2 JIT, independent sync/async AOT, verified native link/install closure and relocation, managed failure sweeps, reload/cache, Debug/Release/strict and 1 MiB process stack limits. Native Windows/macOS execution remains open |
| L — language/editor | Exact stdlib/private ABI/typed IR/ownership/effect validation, worker/readonly/startup restrictions, documented declarations, public/private/member inventory, hover/definition/active parameters/incomplete input and read-only stdlib rename |
| O — owner scope | HTTP/3/h2c/WebSockets/servers/SSO/OAuth/public raw sockets and persistent cookie storage remain explicitly excluded; no claimed native-platform acceptance or whole-process RSS ceiling |

Public entry points are in [url.void](../../../stdlib/url.void) and
[net/http.void](../../../stdlib/net/http.void). [User guide](README.md),
[five runnable examples](../../../examples/net_http/README.md),
[current implementation](CURRENT_ENGINEERING.md), [approved contract](DESIGN.md)
and [acceptance roadmap](ROADMAP.md) provide the detailed inventory.

## Exact dependency decisions

`third_party/curl/` is authoritative and left unstaged for Boss. Its verifier
checks all 1,586 retained upstream file identities and exactly four corrected
files. CMake builds those sources directly. `.toolchains/` is disposable;
production has no patch-application step or required ZIP.

- curl 8.23.0 archive SHA-256:
  `8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5`.
- Exact three-file cookie diff:
  `6a558dc87faec9bbba68056ee79903dff0c05b5e61ba7b08a513b2e21b817920`.
- Exact approved fourth-file H2 diff:
  `f109b12c2c5b327fb9b2da2b9d302cb111d936097b98ace830ea886db7d4f1a2`.
- Approved unmodified Linux AWS-LC 5.12.0 revision:
  `cd481c207e38f06aeb5766f4a1cdfce10a4c0af8`; archive SHA-256:
  `1de549ad9ac709d08a7e26ac5aa77dc90d55811c5c5c54b4ec3e442b00e98e68`.

[Vendoring/reproduction](VENDORED_PROVIDER.md), [exact H2 review](HTTP2_WINDOW_REVIEW.md),
[TLS decision](TLS_REVIEW.md) and `third_party/http-versions.json` preserve version,
license, checksum and follow-up identity. Upstream follow-up drafts are local;
no issue, PR or external message was sent. Fresh full-provider Debug/Release/strict
and strict disabled-cookie/HTTP builds pass. Two clean Release builds and a fresh
root/custom-output build produce byte-identical curl archives:
`1526915123669110951425c89d1cf51e218aeb172f413149149094684b2c5837`.
All three Linux prefixes independently verify 167 files.

## Verification record

| Run | Actual result |
| --- | --- |
| Original consolidated Release | [382/387](evidence/http-consolidated-tests-release.log), five concrete integration failures preserved |
| Release repairs | [25/25](evidence/http-final-tests-release.log), including all five original failures |
| Release after Runtime cost repair | [79/79](evidence/http-reaping-final-tests-release.log), after the completed-task cost correction |
| Expanded final Release | [92/92](evidence/http-frontend-final-tests-release.log), including broader frontend coverage after the semantic stack repair; all-target rebuild also passes |
| Held public-source HTTP reload | [Release](evidence/http-reload-owner-tests-release.log), [Debug](evidence/http-final-tests-debug-repaired.log) and [strict](evidence/http-frontend-final-tests-strict.log) pass: cold/hit cache, precise GC while a streaming read is held, old/new generation results, incompatible-layout restart and complete retirement |
| Expanded final Debug | [105/105](evidence/http-final-tests-debug-repaired.log), including the twelve private-call probes, held reload and all four original Debug repairs; all-target build passes |
| Original consolidated strict | [374/377](evidence/http-consolidated-tests-strict.log), old stress deadline and old Scope-worker-zero assertions; all three have passing reruns |
| Strict affected run before frontend split | [80/81](evidence/http-final-tests-strict.log); TLS and Stage6 pass. DNS fixture startup hit a UDP/TCP port collision before native execution; bounded pair acquisition and cleanup controls pass, and the final strict run passes DNS |
| Expanded final strict | [92/92](evidence/http-frontend-final-tests-strict.log), full ASan/UBSan/leak checks, after frontend and fixture repairs; all-target build passes |
| Additional final fixture/frontend checks | [Release DNS 1/1](evidence/http-dns-port-final-release.log); channel frontend [Release 1/1](evidence/http-frontend-channel-final-release.log), [Debug 1/1](evidence/http-frontend-channel-final-debug.log), [strict 1/1](evidence/http-frontend-channel-final-strict.log) |
| Original consolidated Debug | [396/400](evidence/http-consolidated-tests-debug.log), semantic stack pressure in two math tests plus old worker assertions; repairs retain the original 1 MiB stack and nesting limits |
| Strict TLS corpus | [Final 256 fresh handshakes plus negative peers pass](evidence/http-tls-corpus-final-strict/commands.json), 757.63 s; [earlier explicit-deadline pass](evidence/http-tls-corpus-explicit-deadline-strict/commands.json), 853.85 s. Full ASan/UBSan/leak checks; binary/runtime/provider hashes remain unchanged after the frontend-only rebuild |
| Post-correction public source | [2 GiB, 9 process checks](evidence/http-reaping-source-2GiB-release/commands.json): normal/O2 JIT and independent normal/O2 AOT |
| Post-correction native bulk | [2 GiB download + 4 GiB+1 upload](evidence/http-reaping-public-bulk-4GiB-release/commands.json), independent byte count/hash and exact file positions |
| Final-source clean pinned build | [18 example process checks / 1 CTest](evidence/http-final-clean-examples-release.log), [2/2 LSP CTests](evidence/http-final-clean-lsp-release.log), explicit LLD 23.1.4. [Eight archives/compiler objects and voidls reproduce byte for byte](evidence/http-final-clean-build-identity.json); all eleven native resources verify |
| Earlier fresh compiler/custom-output control | [18/18 example checks](evidence/http-clean-output-examples-release.log) with Unicode/space/#/% output and adjacent hashed resources. This configuration left `VOID_LINKER` empty and used the default system linker; it is not pinned-LLD acceptance |
| Managed failures | 1,448 sync and 1,607 async injected failures per optimization mode in each profile; zero managed/native source roots after drain and successful reuse; `http-private-form-faults-PROFILE` receipts |

[Machine-readable reconciliation](evidence/http-final-verification.json), produced
by [this local script](evidence/final-verification.py), finds a passing latest
available result for all 389 Release, 402 Debug and 379 strict registered names.
Those are the original consolidated runs plus the explicitly listed focused
reruns, not three new all-tests executions. Every original failure remains in
its original log.

Strict uses `-fsanitize=address,undefined -fno-omit-frame-pointer`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No suppression, ignorelist,
quarantine reduction, recovery or checker exclusion was introduced. Native/source
execution probes inherit enforced 1 MiB process stack limits. Historical toolchain
strict exclusions remain their documented baseline; they are not new HTTP passes.
Do not add process checks, injected failures or handshakes to CTest totals.

The Debug ordinary-call frame falls from 22,312 to 2,728 bytes by returning
through a separate builtin handler before ordinary argument recursion.
[Exact inputs, flags and frame comparison](evidence/frontend-stack/summary.json)
preserve the original nesting limits and crash bundles. Shared generated test
objects now have one CMake producer before either consumer links; all twelve
private-call tests pass. DNS fixture startup acquires UDP/TCP as a pair, closes
failed attempts and retries only address-in-use at most 32 times;
[all three deterministic controls](evidence/dns-port-pair-probe.json) pass.
These repairs introduce no sanitizer suppression or transport-policy relaxation.

## Measured memory and performance

[Memory review](MEMORY_REVIEW.md) distinguishes admission, live allocations,
managed payload, native bookkeeping and RSS. The post-correction public source run
records 258 forced-GC samples: 212,924–219,068 managed payload bytes, maximum
Task/group/Handle counts 11/2/11. Retained handles remain repeatedly awaitable.
Root/Runtime cleanup returns source objects and registered handles to zero.

The >4 GiB native bulk run takes 6.494 seconds execution / 8.56 child CPU seconds,
with 10,580–11,200 KiB steady RSS. Its peer hashes exactly 4,294,967,297 uploaded
bytes as `153014c024b3db9a126b40397ded44c0b708efde6f5c89f22407e4089160850a`.
Object/executable sizes are 2,434,192/10,106,360 bytes. Source check, object emission,
link and execution timings are separate in the receipt.

Final strict large-CA live allocations plateau at 4,568,497–4,569,197 bytes, sampled
peak 30,430,868 bytes, against 356,340,600 reserved client bytes. The paused H2
probe stays at 52,270,438 live bytes within 71,303,366 reserved; its original
duplicate-credit run exceeded that reservation at 102,577,468 bytes.

[Performance evidence](PERFORMANCE.md) retains cold/warm h1/h2 JSON latency,
four-concurrent batches, observed connections, partial-response close, compilation,
package cost and one-worker IO absent/idle/paused comparisons. An actual baseline
build exposed a 50% IO CPU regression in redundant completed-task dependency scans.
The retention-preserving short-circuit repair reduces pure-IO CPU median from
1.282 to 0.890 seconds (baseline 0.837); HTTP-capable absent/idle/paused runs have
0.837-second overall CPU median. All ranges and original samples remain recorded.
Historical logging optimizer/performance/release holds remain unchanged.

The final-source clean build uses a Unicode/space/`#` CMake output directory.
Direct external LLD absolute outputs containing `%` reproduce the existing
[Stage9 toolchain limitation](../../STAGE9_RESEARCH.md#reproduced-lld-output-path-issue);
the [new control log](evidence/http-clean-pinned-linker-percent-control-build-release.log)
is retained. The existing compiler-owned relative-stage publication and passing
HTTP install/relocation `%` path tests are unchanged. The raw clean `voidc` has a
different required cache-build identity because its input paths differ. An
[isolated link control](evidence/http-final-clean-identity-link.json), changing
only that identity archive, reproduces the main compiler byte for byte; no
production identity was replaced.

## Open acceptance gates and review focus

1. **Native Windows/macOS execution is missing.** Recipes, adapters and strict
   requirements are present; Schannel store/P12, native trust/revocation, ICU/WinIDN,
   DNS/VPN/search, paths/CRT/SDK and actual native thread-stack acceptance need
   platform evidence. No platform exclusion has been approved. [Exact boundary](NATIVE_PLATFORMS.md).
2. **Provider worst-case memory certification remains open.** Explicit-material
   parser charge is conservative and measured; malformed ASN.1 and host-owned
   system roots/native stores lack a complete enforced heap proof. The 512 MiB
   ledger is provisional, not a certified process-allocation/RSS ceiling. No trust
   disablement, second parser, global allocator hook or dependency fork is hidden
   behind the measured plateaus. [Evidence and limits](MEMORY_REVIEW.md).
3. **Independent Golden Wrench source/evidence review is pending.** Review the
   exact four dependency corrections; async root/Work/Task/group retirement;
   native completion/quiescence and generation leases; Runtime worker/shutdown
   ownership; source file authority; security/policy defaults; and the explicit
   acceptance gaps above. Passing local tests do not close these gates automatically.

[Candidate file/hash inventory](evidence/http-candidate-files.json) identifies the
working-tree source/doc set; it is not staging or authorship attribution.
[Chronological engineering history](ENGINEERING_HISTORY.md) retains original
OpenSSL UB, cookie OOM, H2 credit, retry, timeout, source-input build race, worker
integration, install and fixture failures. The first reload fixture wrongly moved
resource acquisition onto the development host thread; the passing test keeps both
activations on the application owner and preserves affinity.

No staging, commits, branches, pushes, hosted CI, external messages or protected
root README edits. Root README SHA-256 remains
`c287f35842b03a1c5ac170718e674cc5498f877ade76eede5273859921d62add`.
Boss owns Git/hosted CI/release; Keyboard provides implementation; Golden Wrench
owns independent review. Full HTTP acceptance is not claimed.
