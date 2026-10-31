Current implementation and evidence are maintained in
[CURRENT_ENGINEERING.md](CURRENT_ENGINEERING.md). The exact fourth curl HTTP/2
correction is approved and directly integrated; the three cookie corrections
remain intact. Public URL/HTTP source, native protocols, bulk/multipart, TLS/DNS,
security/policies, async lifetime, docs/examples, tooling and install closure are
implemented. Consolidated local tests and focused integration repairs are recorded
in the [Golden Wrench handoff](HANDOFF.md).
Native platform execution and provider worst-case memory certification remain open.
Full HTTP acceptance and independent Golden Wrench source review are pending.

# HTTP engineering record

30 October 2026. Baseline `15f7868b0af177f8f7d49c548392c79c7bd921d2`;
LLVM/Clang/LLD 23.1.4. [Boss authorization](AUTHORIZATION.md),
[approved contract](DESIGN.md), [dependency sequence and acceptance](ROADMAP.md).

Status: **Linux implementation handoff ready; complete HTTP acceptance pending**. The exact
four curl corrections are applied directly in `third_party/curl/` and selected by
CMake. URL source/compiler integration and the native HTTP multi kernel now exist.
Native bulk/file continuations and the `net/http` source facade/bridge are implemented.
The complete platform/provider-memory/acceptance matrix remains unfinished. Current
functionality and exact receipts are maintained in
[CURRENT_ENGINEERING.md](CURRENT_ENGINEERING.md); the upstream OpenSSL callback
conflict and the approved, production-selected AWS-LC resolution are in
[DEPENDENCY_CONFLICT.md](DEPENDENCY_CONFLICT.md).

The Runtime uses existing external completion, precise GC and structured cleanup.
Completed result/Work/Handle retirement and subsequent quiescent native Task/group
record retirement have focused local evidence. Retained handles preserve repeated
await. Actual sustained HTTP **source** streaming has passed 2 GiB normal/O2 JIT and
independently linked AOT probes; the complete provider allocation audit remains
unfinished. No full memory or cross-platform claim is made.

Everything below is historical investigation. Its "not implemented", patch-only,
root-owned record and unselected-dependency statements describe the initial state;
they are superseded by the current engineering record and direct-vendoring
instruction. Boss does not need to apply or copy a patch or dependency.

Engineering source inspection and a standalone native experiment demonstrate
an ambiguous cookie enumeration result in the curl 8.23.0 candidate. This prevents
the proposed exact jar census from distinguishing valid empty state from failed
accounting. The approved roadmap requires exposing an unenforceable provider
budget for review rather than forking curl or adding another cookie engine.
No silently reduced HTTP contract or production dependency patch is selected.

## Historical blocker and review decision (resolved by direct-vendoring authorization)

The required in-memory jar budget is 1,024 cookies / 1 MiB serialized state,
checked before further hops/admissions. In the signed curl 8.23.0 archive:

- `lib/cookie.c:1615` returns NULL both for an empty jar and allocation failure
  while formatting or appending the enumeration. Partially built lists are freed.
- `lib/getinfo.c:605` maps both outcomes to `CURLE_OK` from
  `CURLINFO_COOKIELIST`; the caller receives no error distinction.
- `lib/cookie.h:81` limits individual cookie/response/outgoing sizes, not total
  session state. Those caps do not replace the Runtime's jar budget.

The [standalone probe](evidence/cookie_accounting_probe.c) constructs no network
connections and reads/writes no cookie files. It demonstrates real in-memory jars
of 1,025 cookies (44,075 serialized bytes) and 300 cookies (1,233,600 serialized
bytes). Selected allocation failures at the beginning, middle and end of census
return `CURLE_OK` plus NULL. A subsequent successful census proves the jar was
still present and over the relevant limit. These are provider-API observations,
not an exploit or a bypass in a VOID HTTP implementation, which does not exist.

Treating NULL as empty can miss the budget. Treating every NULL as failure rejects
valid empty/expired jars; the controls demonstrate those valid states. Counting
cumulative Set-Cookie bytes/lines imposes a different lifetime quota: 2,048
replacements in the probe leave one 43-byte cookie. Replacing provider parsing
or silently discarding a valid session would change the approved contract.

`curl_global_init_mem` can observe failures in an isolated experiment, but its
callbacks are process-wide and are not installed when a host already initialized
the same libcurl. The separate embedding control confirms zero replacement
allocator calls despite successful initialization and enumeration. Taking over
host allocation policy or depending on being the first user does not satisfy
the approved embedding/process-lifetime contract. No such hook is added to VOID.

The current upstream master inspected on 30 October also returns NULL for census
allocation failures. That mutable inspection is context only; the reproduction
and proposed patch target the signed immutable 8.23.0 release.

**Subsequent Golden Wrench ruling, 30 October 2026:** Option 1 is selected.
Waiting for an upstream release is unnecessary. A narrowly pinned correction
may be carried **subject to independent review of the actual patch**; that source
approval has not occurred. Keyboard may continue unrelated HTTP engineering while
review is pending. The exact 94-line diff has been supplied inline in chat, and a
portable review archive is being prepared. No contract weakening is approved.

The [candidate provider patch](evidence/curl-8.23.0-cookie-oom-candidate.patch)
remains a review artifact applied only in isolated
build directories. It makes both enumeration allocation failures propagate
`CURLE_OUT_OF_MEMORY`, preserves valid empty success and partial-list cleanup,
and changes no public ABI or cookie grammar. It is not independently reviewed,
upstream-accepted, installed or connected to the compiler/Runtime build.

This stop follows the approved ROADMAP delivery 3 instruction:

> if a public budget cannot be enforced through supported APIs, expose that as a design blocker for review, not permission to fork curl or introduce a second cookie engine.

The subsequent ruling permits independent engineering to continue while the
exact source review is pending. No repeated approval of the five settled
architecture decisions is requested. The remaining complete engineering scope
is still required in its dependency order.

## Historical isolated candidate identity and limits

| Item | Actual status |
| --- | --- |
| curl candidate | 8.23.0, released 14 October 2026; downloaded official source archive, not a selected production transport profile |
| Archive | `https://curl.se/download/curl-8.23.0.tar.xz`, 2,990,916 bytes; SHA-256 `8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5` |
| Signature | GPG VALIDSIG for `27EDEAF22F3ABCEB50DB9A125CC908FDB71E12C2`; isolated public-key keyring; original trust warning retained in [receipt](evidence/curl-archive-signature.log) |
| License | curl-specific permissive license; original [COPYING](evidence/curl-8.23.0-COPYING.txt) accompanies the proposed patch |
| Patch | SHA-256 `6a558dc87faec9bbba68056ee79903dff0c05b5e61ba7b08a513b2e21b817920`; three private provider files; no production integration |
| Transitive closure | nghttp2, c-ares, platform TLS/trust, zlib, libpsl/PSL, IDN/Unicode releases, licenses, CRT/framework/link/install closure remain unselected and unverified |
| Native experiment | Static cookie-enabled libcurl without TLS, HTTP/2, PSL, IDN or async DNS; not the required transport feature set, an HTTP client acceptance build or a replacement architecture |
| Provider memory | Source shows 10 MiB h2 maximum stream window, 10 MiB connection receive queue and spare-pool settings; total native memory has not been measured or admitted. 512 MiB remains provisional, not an RSS claim |

The detached signature and public release key are retained beside the
[source identity manifest](evidence/cookie-provider-source-identity.json).
No user keyring or trust store was changed. No uncontrolled system-libcurl fallback
or curl executable was used. No upstream issue, PR or message was sent.

## Initial missing inventory (superseded by CURRENT_ENGINEERING.md)

The full signatures/defaults remain in DESIGN. Every public API row is still
missing, including all sync/async pairs; no partial URL or HTTP facade is shipped.

| Required capability | Current implementation / acceptance |
| --- | --- |
| URL parse/resolve/normalize/components/ordered queries | Not implemented |
| Methods, duplicate/binary headers and all body variants | Not implemented |
| Actual response metadata, trailers, history and strict text | Not implemented |
| Buffered and genuine bidirectional streaming APIs | Not implemented |
| Native bulk file operations with existing IO reservations | Not implemented |
| Client/transfer acquisition, finish, close and scoped cleanup | Not implemented |
| Verified TLS, custom trust and supported client identities | Not implemented |
| Async DNS, IPv4/6, isolated pooling and finite lifetimes | Not implemented |
| All five monotonic timeout intervals | Not implemented |
| Secure redirects, replay and credential isolation | Not implemented |
| Auth, proxies and bounded in-memory sessions | Not implemented; cookie census blocker demonstrated |
| Bounded gzip/deflate and raw content mode | Not implemented |
| Genuine h1/h2 negotiation, multiplexing and required-h2 policy | Not implemented |
| Runtime external completion, structured cancellation/shutdown | Private foundation implemented and locally tested; actual transport integration and native platform acceptance outstanding |
| Complete API docs/examples/compiler LSP/install support | Not implemented |
| Async-result lifetime correction and sustained-transfer proof | Quiescent result/Work/Handle retirement implemented; retained source handles preserve results; native records remain root-owned, and actual HTTP sustained-transfer proof is outstanding |

HTTP/3, h2c, servers, WebSockets, browser SSO/OAuth flows, persistent cookie
storage/import/export, arbitrary URI schemes and raw public sockets remain
approved exclusions. None is marked implemented. The complete U–O acceptance
matrix, HTTP performance measurements and native Windows/macOS results remain
outstanding. No JIT/O2/AOT, multi-GiB, real h2/TLS, GC/reload/install or whole-HTTP
memory acceptance can be inferred from this standalone provider experiment.

## Actual verification and retained failures

All final runs used the verified LLVM/Clang 23.1.4 executable. The provider and
probe were both instrumented in strict mode, with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Each probe child reported its
actual soft **and hard** stack limit as 1,048,576 bytes after enforcement.

| Profile | Reproducible receipt | Actual outcome |
| --- | --- | --- |
| Release | [Commands, hashes, times and sizes](evidence/cookie-Release-20261030T122547143010Z/commands.json) | Original and candidate built; 12 original OOM ambiguities, 12 candidate checked-OOM outcomes, valid-state and embedding controls passed |
| Debug | [Corrected harness receipt](evidence/cookie-Debug-20261030T122727912022Z/commands.json) | Same native checks passed |
| Strict ASan/UBSan/LSan | [Corrected harness receipt](evidence/cookie-strict-20261030T122727909757Z/commands.json) | Same native checks passed; no sanitizer or leak diagnostic |

There were 12 final native probe invocations across those three profiles, 36
allocation-failure observations per provider variant, 48 valid-state observations
and six embedding controls. Original-provider observations deliberately reproduce
the defect; their exit-0 test harness status is **not** a repaired-provider pass.
Those initial provider-only checks ran no VOID CTest matrix. Subsequent
[Runtime foundation checks](RUNTIME_FOUNDATION.md) include Release 10/10 selected
CTests, Debug/strict native selections, source normal/O2 retention, linked AOT and
enforced 1 MiB native-process checks. They do not establish complete HTTP acceptance.

The original [four-failure discovery log](evidence/cookie-accounting-original-initial.jsonl)
is preserved separately. The expanded probe adds middle/end allocation failures
to exercise partial enumeration cleanup. Initial
[Debug](evidence/cookie-Debug-20261030T122558103393Z/commands.json) and
[strict](evidence/cookie-strict-20261030T122558104999Z/commands.json) compilation
attempts failed with compiler exit 1 because the harness requested `libcurl.a`
where curl's Debug default produced `libcurl-d.a`. No probe process launched on
those attempts; they were missing-input harness failures, not source compiler
crashes or sanitizer failures. The [initial driver](evidence/reproduce_cookie_accounting-initial.py)
and original stderr are retained. The corrected driver explicitly sets
`CMAKE_DEBUG_POSTFIX` to empty; Debug symbols and sanitizer options remain enabled.

The two initial manual configurations also retained an unused
`CURL_USE_LIBIDN2` warning; the reproducible driver uses the actual `USE_LIBIDN2`
option. The attempted upstream `.sha256` URL returned HTTP 404; no upstream hash
file is claimed. The locally computed immutable archive hash and detached GPG
signature verification are recorded instead. Initial `git diff --check` flagged
new documentation trailing whitespace; that whitespace was removed.

This is correctness evidence for the isolated provider error path, not HTTP
performance acceptance. Per-command elapsed times and standalone artifact sizes
are in the receipts; no repeated benchmark, RSS ceiling, throughput, CPU or
application compile/dependency-size conclusion is inferred from them. No Windows
or macOS execution was available or performed in this investigation.

Reproduce from the repository root with the official archive at the manifest's
path (or pass `--archive /absolute/path/curl-8.23.0.tar.xz`):

```sh
python3 docs/net/http/evidence/reproduce_cookie_accounting.py --profile Release
python3 docs/net/http/evidence/reproduce_cookie_accounting.py --profile Debug
python3 docs/net/http/evidence/reproduce_cookie_accounting.py --profile strict
```

The [driver](evidence/reproduce_cookie_accounting.py) verifies the archive hash,
extracts separate original/candidate sources, applies the exact patch without
fuzz, records commands/stdout/stderr/status/hashes, and enforces actual child
stack limits. It retains each invocation in a new evidence directory and never
edits production sources, Git state, user keyrings or trust stores. These are
Linux-native reproduction commands, not portable native-platform acceptance.

## Earlier modified files and review handoff

- `docs/README.md`, `docs/WRENCH_CLANKER.md`,
  `docs/VOID_STDLIB_BACKLOG_2026-10-27.md`: current continuity/status corrections.
- Existing, initially untracked `docs/net/http/{RESEARCH,DESIGN,ROADMAP}.md`:
  subsequent approval annotations and the explicit cookie-inventory correction;
  original proposal/history retained.
- `docs/net/http/AUTHORIZATION.md`: exact pasted Boss mission.
- This record and `docs/net/http/evidence/`: native reproducer, proposed provider
  patch, license/signature/identity evidence, original failures and final receipts.
- [Runtime foundation record](RUNTIME_FOUNDATION.md), `docs/ASYNC.md` and the
  historical Stage 8 retention annotation: ownership changes, actual results,
  failures, native record memory costs and verification limits.
- `include/void/{async,native_resource}.hpp`,
  `src/runtime/{async,coordination,native_resource}.cpp`, `CMakeLists.txt` and
  `tests/async/external.cpp`: private native completion/lifetime/shutdown and result
  ownership integration with the existing Runtime and resource ledger.
- `tests/stage8/sprint5/{retention.cpp,measure.py}`: distinguish retired results
  from native ordering records and bound settled source payload using generated
  root metadata, with original failed assertions preserved.
- Ignored `.toolchains/http/` and `build/http-cookie-*`: isolated downloaded
  source, public-key verification keyring and original/candidate native builds;
  no application/runtime integration or install changes.

[Verification summary and file hashes](evidence/verification.json) identify the
initial provider receipts and protected root README comparison; it is a dated
snapshot predating Runtime implementation. The later Runtime receipts are linked
from RUNTIME_FOUNDATION.md. Option 1 is selected, but independent exact-patch
source approval is pending. This remains an **unfinished engineering mission**,
not the completed full HTTP handoff or a request to authorize another sprint.
The original complete mission remains authorized, including unrelated work while
the provider patch is reviewed.

Current-status documentation now records Boss's hosted LLVM acceptance and
cross-platform `os`/`collections`/`log` results. Historical failed-test records,
the logging optimizer incident, performance OPEN and release HOLD are preserved.
The research inventory's cookie import/export contradiction is explicitly corrected.

No Git staging/history/branch/remote writes, hosted CI or protected root README
changes are authorized or performed.
