# VOID 2.0 private preview qualification handoff

4 November 2026. **REPORT READY — STOP FOR GOLDEN WRENCH REVIEW.**

Linux x64 source installation passed the bounded fresh Ubuntu container checks.
This is private qualification, not public-preview/release acceptance, independent
machine/VM certification or permission to distribute compiler/dependency binaries.

## Identity, mission and authority

Initial and final source HEAD: `97aa4e4334f1b9354ba651770b5cf13e74da03d0`,
branch `main`, tracking `origin/main`. Initial working tree was clean. No applicable
ancestor/repository `AGENTS.md` was found. All twelve mandatory files were read
before checks; [initial hashes](evidence/qualification-start.json) and
[exact examined-file inventory](evidence/examined-files.json) preserve the inputs.
Additional large files were inspected in relevant sections as marked there.

Boss authorizes Linux source-build dependency inventory, available fresh-environment
verification, classification and durable reporting. Production, CMake and Runtime
repairs require separate Golden Wrench review and Boss authorization. Root README
is protected/read-only. No production/test/Runtime/CMake change was made in the
actual checkout. No staging, commit, push, branch creation, hosted CI dispatch,
package publication, deployment or public redistribution occurred.

HTTP/HTTPS remains [accepted](../net/http/ACCEPTANCE.md) at the recorded seven-job
LLVM23 hosted run; it was not reopened. Website modernization is outside this task.
No historical investigation or acceptance record was rewritten.

Boss further clarified that historical Linux LLVM19 DSE cause remains unproven;
LLVM23 may have eliminated an upstream defect, but this is not established.
Current optimizer risk must use LLVM23 evidence: its three passing bounded
DSE-associated checks, seven passing hosted jobs and separately tracked current-
toolchain incidents. Historical uncertainty is not itself a demonstrated current
LLVM23 defect. Exhausted LLVM19 reproduction campaigns remain closed. Earlier
Windows LLVM23 incidents remain separate. None was rerun or causally reclassified.

## Completed work and every outcome

The [qualification report](LINUX_CLEAN_HOST_QUALIFICATION.md) contains the complete
source dependency/install contract, exact successful recipe, observed runtime
closure, missing-prerequisite classifications and seven first-install guidance gaps.
[Command receipts](evidence/commands.jsonl), [all-command result index](evidence/results.json)
and [probe oracles](evidence/checks.jsonl) include passes and failed attempts.

- PASS: independent initial Git inspection and all mandatory preparation.
- PASS: new rootless Podman Ubuntu 24.04 x64 userspace, no host mounts or initial
  build tools; exact image/package versions recorded. It shares host kernel/CPU
  and network. This is not a relocated checkout test on the existing host OS.
- PASS: official full LLVM 23.1.4 archive hash checked before/after transfer,
  extracted fresh; versions/commit, Release, assertions OFF and RTTI NO confirmed.
- PASS: all seven HTTP dependency source downloads/builds, 22 child commands,
  167 prefix files verified. No development-built HTTP library was reused.
- PASS: plain Release CMake configure including genuine ORC link/run probe,
  264 production build actions, compiler and tooling component installs.
- PASS: prefix relocation; original container source stdlib moved out of reach;
  installed PATH discovery, minimal `main.void` check/run and installed `console`
  import check/run, default O2. Standard module output matched exactly.
- PASS: installed `voidc build` and direct execution of its native executable.
- PASS: installed `voidls --discover` and real framed initialize/initialized/
  shutdown/exit; protocol 1, shutdown null, exit 0. No editor UI claim.
- PASS: 86 installed files/aliases hashed; all 21 installed standard sources
  byte-match preserved source; actual ldd/readelf closure resolves successfully.
- PASS EXPECTED REJECTIONS: missing HTTP prefix, unavailable stdlib root,
  absent native manifest, missing configured Clang. Each exits 1 with useful
  diagnostics. Disposable negative-control copies were never used for positives.
- FAIL, harness preparation: first fixture-generation command had Python quoting
  syntax error before VOID execution. Original preserved; corrected fixture passed.
- FAIL, harness oracle: first minimal check exited 0 with correct current success
  text, but the harness incorrectly required empty stdout. Source inspection
  confirmed the CLI contract; corrected check passed. Original failed oracle
  remains in receipts. No production repair or required status change occurred.
- FOLLOW-UP: existing date/console/proof warnings retained without suppression;
  no crash/sanitizer marker found in completed build output. Release is not
  sanitizer coverage. CMake's test-only dwarfdump variable was unused as expected.
- FOLLOW-UP, documentation recorder: no-index new-file diff returned 1 without
  whitespace output; its recorder expected 0. Original receipt retained; the
  final direct Markdown/JSON/link/preservation validation passed.
- INVENTORY TOOL LIMITS: absent `rg`, broad find exit 1 with suppressed stderr,
  and a guessed example-path read failure are recorded separately. Python/find
  and the actual paths supplied the inventory. No test result depends on them.

SKIP: full CTest/Debug/strict reruns, HTTP/HTTPS regression campaign, logging and
`text.translate_scalars`, LLVM19 crash reproductions, preserved-artifact optimizer
investigation, native Windows/macOS, actual editor UI, archive/VSIX packaging and
all owner-controlled Git/CI/publication actions.

UNKNOWN: independent machine/VM and other distro/ABI results, complete missing-
prerequisite matrix, absent-CA diagnostics, fresh-network LLVM SDK download,
post-link application relocation, broader editor compatibility and distribution
license/notice clearance. No unavailable host result was invented.

## Blockers, next step and stop conditions

The [blocker register](PREVIEW_BLOCKERS.md) separately classifies optimizer risk,
logging performance, public licensing, Linux install scope, SDK/application
portability, notice completeness, documentation and platform/editor coverage.
Logging performance remains OPEN and public redistribution is not authorized.
Historical logging correctness is not performance acceptance.

Fresh source installation works on the recorded Ubuntu userspace with its
retained absolute SDK paths. Generated executables retain an absolute installed
library RUNPATH. CMake installed HTTP/common notices and GNU corresponding source,
but has no standalone LLVM notice rule; this needs review before distribution.
The protected README still names LLVM19, and the current build guide omits HTTP
preparation and the complete installed-use workflow. These findings were documented,
not repaired speculatively. No environmental blocker prevented this bounded run.

**Recommended next step:** Golden Wrench reviews this report, evidence and the
fresh-container acceptance boundary. Boss then decides any separately scoped
documentation/notice/portability work and a later independent Linux machine/VM
attempt. Windows/macOS installation qualification follows the accepted Linux
baseline. No implementation, CI or public release action is authorized by this
handoff; engineering stops here for review.

Stop immediately for a new correctness/safety issue outside installation scope.
No crash campaign or logging work may restart under this qualification. Do not
mark historical optimizer cause fixed from passing LLVM23 checks. Do not edit
root README, production/CMake/Runtime, stage/commit/push, create a branch, dispatch
hosted CI, distribute binaries or deploy without the stated owner authority.

## Resumption and evidence

Raw evidence is retained privately under
`$HOME/.local/state/void-preview-qualification/97aa4e43-20261104/`: per-command raw
stdout/stderr/stdin and argv, source tar identity, dependency build evidence and
CMake/configuration archive. Portable receipt variables redact actual host path
bindings; no full environment dump or credentials are included. Large logs,
compiler binaries, SDK and installed payloads remain outside repository docs,
website and public outputs. Only compact metadata/receipts are added here.

Container `void-preview-linux-97aa4e43` is stopped and retained. A later explicitly
scoped resumption can start it; do not treat its already-built state as a new
clean attempt. Valid install: `/opt/void-relocated`; original source stdlib:
`/work/source-stdlib-preserved`; negative-control install:
`/opt/void-missing-manifest`. The SDK remains under `/src/void/.toolchains/llvm-23.1.4`.
No further test or repair is needed to complete this report.

## Exact changed-file inventory

All changes are new, **uncommitted and unstaged** documentation/receipt files.
Tracked source, protected README, historical evidence and HEAD remain unchanged.
The [final preservation/inventory receipt](evidence/final-inventory.json) records
Git checks, protected/input hashes and every path below. Its own hash is omitted
from the digest table to avoid a recursive checksum.

- `docs/release/LINUX_CLEAN_HOST_QUALIFICATION.md`
- `docs/release/PREVIEW_BLOCKERS.md`
- `docs/release/PREVIEW_QUALIFICATION_HANDOFF.md`
- `docs/release/evidence/build-log-review.json`
- `docs/release/evidence/checks.jsonl`
- `docs/release/evidence/commands.jsonl`
- `docs/release/evidence/dependency-build.json`
- `docs/release/evidence/examined-files.json`
- `docs/release/evidence/final-inventory.json`
- `docs/release/evidence/installed-layout.json`
- `docs/release/evidence/inventory-tooling-notes.json`
- `docs/release/evidence/lsp-handshake.json`
- `docs/release/evidence/probe-inputs.json`
- `docs/release/evidence/qualification-start.json`
- `docs/release/evidence/results.json`

Container cleanup note: `podman stop --time 10` exited 0 after warning that PID 1
(`sleep infinity`) did not stop on SIGTERM and required SIGKILL. No VOID probe was
running then; this is container housekeeping, not a compiler crash or unknown
qualification exit. Final inspection reports `exited` with no mounts.
