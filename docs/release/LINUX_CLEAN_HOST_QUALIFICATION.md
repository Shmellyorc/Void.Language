# Linux x64 source-install qualification

4 November 2026. **REPORT READY FOR GOLDEN WRENCH REVIEW.** Fresh Ubuntu x64 container
installation checks passed within the scope below. Independent-machine and
public-preview acceptance are not claimed.

Source HEAD: `97aa4e4334f1b9354ba651770b5cf13e74da03d0`.
Scope and authority: [handoff](PREVIEW_QUALIFICATION_HANDOFF.md).
The dependency inventory below describes this exact source, not a portable
binary release or a claim about other platforms.

## Environment and evidence boundary

The orchestrator is the existing PikaOS 4 x86-64 / glibc 2.44 development host.
Its installed libraries and existing VOID builds do not count as fresh results.
Rootless Podman 5.8.6 successfully started a new Ubuntu 24.04 x64 image with no
host source, SDK, dependency prefix or home-directory mount. The initial image
reported Ubuntu 24.04.5 LTS, glibc 2.39 and no build tools. It shares the host
kernel and network; this is fresh container userspace, not an independent
machine, VM, kernel compatibility test or different CPU test.

Image ID: `d9b8c34d61a9b9a3c44cab883d7d2f2619d674966ab520186f799cf32e3e0377`.
Recorded image digest:
`sha256:78d4744ead1f5b19b04d0df558ab182a5300903900755e515b2ca4d10eb2044d`.
Exact preparation/check argv, exit status and output are in
[command receipts](evidence/commands.jsonl). Raw output remains private under
`$HOME/.local/state/void-preview-qualification/97aa4e43-20261104/`.
Path bindings in compact receipts are normalized: `VOID_SOURCE` is the inspected
checkout, `VOID_WORK` the private evidence root, `VOID_LLVM_SDK` the development
SDK path used only for inventory, and `VOID_HTTP_PREFIX` its existing Release
prefix used only for inventory. These variables redact actual host paths; they
are not an assertion that shell expansion occurred in recorded argv.
Container paths such as `/src/void` are deliberately reproducible, not host paths.

Pristine source is a `git archive` of the recorded HEAD. No dirty documentation,
existing build outputs or development dependency binaries are transferred.
The official LLVM archive is transferred as a source-build prerequisite, with
SHA-256 checked against the repository's pin. HTTP dependencies were built
inside the container from their declared source archives. All seven archives were fetched afresh (no source-cache hits), verified and built successfully; [dependency receipt](evidence/dependency-build.json) records the 22 child commands and source hashes.

## Linux x64 source-build contract (read-only inventory)

| Requirement | Actual contract at this HEAD |
| --- | --- |
| OS / architecture | Linux x86-64 (`x86_64` or `amd64` in HTTP setup), ELF/glibc native host. Ubuntu 24.04 is the hosted baseline. No lower glibc or distro-wide support floor is declared or certified here. |
| CMake | Root minimum 3.24. Native configure must execute compiled programs, including the LLVM ORC probe; this is not cross compilation. |
| C / C++ | Both required; C++20, exceptions retained, RTTI matched to SDK. Supported qualification selects full official Clang/Clang++ 23.1.4 with matching LLD. A compiler alone lacks LLVM development libraries. Host C/C++ development headers, CRT, libstdc++ headers/library and linker startup objects are needed (`build-essential` on Debian-family systems). |
| Python | Required during ordinary CMake configuration, even with `BUILD_TESTING=OFF`, for curl/prefix verification. The practical preparation recipe requires Python 3.12+ (`tarfile` extraction filter); LLVM installer also uses `hashlib.file_digest` (3.11+). Root README's Python 3.9-for-tests description is obsolete. No minimum is enforced by the CMake `find_package(Python3)` call. |
| Build tools | Ninja for `-G Ninja`; no explicit repository minimum. HTTP setup additionally invokes GNU Make and release `configure` scripts; Perl is used by AWS-LC assembly generation. Go is explicitly disabled for AWS-LC. Autoconf/Automake/Libtool regeneration is not invoked by the recipe. Recorded fresh versions: CMake 3.28.3, Ninja 1.11.1, Python 3.12.3, GNU Make 4.3 and Perl 5.38.2; host GCC/libstdc++ development package 13.3.0. The runtime libstdc++6 package is 14.2.0. |
| System development libraries | Build guide lists libffi-dev, libedit-dev, libxml2-dev, libzstd-dev, zlib1g-dev, libtinfo-dev. LLVM's exported targets require real transitive development packages, particularly zlib/zstd/LibXml2. Do not replace missing targets with stubs. Which optional packages are strictly necessary is not proven by installing the declared superset. |
| Other preparation inputs | Distribution `ca-certificates` and readable CA bundle/directory; Python TLS trusts downloads. xz/tar, network access to pinned archive locations or preverified source cache. Git obtains source; curl is a convenience downloader, not the selected native curl provider. |
| Full test matrix | Additional HTTP Python fixtures and certificate tooling are prepared by `tools/setup_http_tests.py` / `tools/ci/http_bootstrap.py`; Node and real editors are separate editor tests. Not required to claim the narrow installation smoke checks and not executed here. |

### LLVM provenance and configuration

`tools/ci/llvm_sdk.py` pins official asset `LLVM-23.1.4-Linux-X64.tar.xz` at
`https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.4/LLVM-23.1.4-Linux-X64.tar.xz`,
SHA-256 `25d814fda50d5fb0084b42be09605d8b1595d49d5e2e03a3345d7e112ff6f9f3`.
The development archive was independently rehashed before transfer and matched.
No new remote provenance/signature verification is claimed. The existing
[LLVM acceptance](../llvm23/ACCEPTANCE.md#toolchain-and-builds) identifies SDK commit
`7a941dfb2877a0b8cf30323fefa85ad019e0b0f5`, Release, no RTTI; actual fresh SDK
version/configuration was checked before building: 23.1.4, RELEASE, assertions OFF, RTTI NO, host triple x86_64-unknown-linux-gnu.

Required SDK paths: `lib/cmake/llvm` (`LLVM_DIR`), `bin/clang`, `bin/clang++`,
`bin/ld.lld`; `bin/llvm-dwarfdump` for relevant test suites. The native build
manifest independently checks Clang and selected LLD version 23.1.4.
The SDK helper's script entrypoint assumes `GITHUB_ENV` and `GITHUB_PATH`;
ordinary source users need those bound to private files or need to use its
`install(destination)` function. The HTTP setup hardcodes the SDK at
`<source>/.toolchains/llvm-23.1.4`, regardless of a later custom `LLVM_DIR`.
The full SDK includes headers and static development libraries. Production LLVM
components are Core, Support, native, OrcJIT, Passes, Target, MC, Object, CodeGen.
Optional LLVM research is OFF. No optimizer switches or DSE suppression are used.
Fresh `llvm-config` confirms assertions OFF and RTTI NO. No-RTTI sanitizer vptr coverage remains unavailable as already documented.

### HTTP dependencies and CMake prefixes

The ordinary Runtime unconditionally includes the accepted HTTP provider. This
inventory/build does not reopen its accepted protocol or memory test campaigns.
`third_party/http-versions.json` is the authoritative archive/hash/license pin
set; curl 8.23.0 is vendored, with four accepted corrections verified by
`tools/verify_curl_source.py`. Public suffix data/provenance are also checked in.

| Dependency | Pinned version | Linux link mode |
| --- | --- | --- |
| nghttp2 | 1.70.0 | static |
| c-ares | 1.34.9 | static |
| zlib | 1.3.2 | static |
| AWS-LC | 5.12.0 / `cd481c207e38f06aeb5766f4a1cdfce10a4c0af8` | static SSL/crypto |
| libidn2 | 2.3.8 | shared, replaceable |
| libpsl | 0.21.5 | static |
| libunistring | 1.5 | shared, replaceable |

Supported preparation is `python3 tools/setup_http_deps.py --profile Release
--jobs 3`, run in the pristine source. Profiles are Release, Debug and strict;
CMake selects strict for sanitizers, Debug for Debug and Release otherwise.
Default Linux prefix is
`<source>/.toolchains/http/prefix/linux-aws-lc-5.12.0/<profile>`;
`VOID_HTTP_DEPS_ROOT` can explicitly override it. `CMAKE_PREFIX_PATH` alone does
not replace it. Expected manifest: `VOID-HTTP-DEPENDENCIES.json`.
Verification checks pin-set digest, profile, recorded file digests, shared-library
aliases and all corresponding source archive hashes under the source tree's
`.toolchains/http/downloads`. Copying only a prefix is insufficient.
The selected curl/TLS/DNS/PSL/IDN/compression libraries cannot fall back to ambient
system packages. Linux curl must discover actual distribution CA locations.

### Install, Runtime, stdlib and native manifest

Build production targets `voidc` and `voidls` plus their dependencies. Install
`compiler` and `tooling` components separately with CMake into a private prefix:

```sh
cmake --install /work/build --config Release --component compiler --prefix /opt/void
cmake --install /work/build --config Release --component tooling --prefix /opt/void
```

`compiler` installs `bin/voidc`, `bin/void-native.txt`,
`bin/void-native-resources.txt`, `lib/void/libvoid_runtime.a`,
`lib/void/libvoid_native_fixture.a`, curl/TLS/DNS/HTTP archives and both GNU shared
library families. `tooling` installs `bin/voidls` and Vim/Neovim adapters.
Both components install `share/void/stdlib/**/*.void` and common notices.
Node/editor applications and a VSIX are not part of CMake's source install.

`void-native.txt` header is `VOID native 23.1.4 v2`; subsequent lines record
absolute configured Clang and LLD, relative Runtime/fixture archives, provider
libraries, rpath and flags. Installed provider paths use `@VOID_NATIVE_DIR@`.
Linux flags include `-pthread` and local symbol exclusion for static provider
archives. Sanitizer configurations add their sanitizer link flag.
`void-native-resources.txt` header is `VOID native resources 1`; each entry is a
SHA-256, tab and link-resource path. It covers Runtime/fixture and provider inputs;
shared GNU libraries are included in that inventory. Native build validates
resources rather than searching cwd/PATH. Installed archive paths relocate;
Clang/LLD absolute paths remain host dependencies.

The frontend resolves standard sources in this order: absolute environment
`VOID_STDLIB_ROOT`, installed executable-relative `../share/void/stdlib`, then
configured absolute build-source root. `src/frontend/source.cpp` implements this;
current editor/Stage9 prose still implies relocation always needs an override.
Unsetting the override and checking installed sources is required for qualification.
A relocated prefix test alone does not prove independence from the source tree;
installed-use checks must exclude fallback to original sources.

Native linking uses the configured driver with C++ mode and the Runtime closure.
Resulting applications do not need LLVM compiler libraries; they still need the
host CRT/C++/thread/dynamic-loader/math libraries and the selected GNU IDN/Unicode
shared libraries. Exact loader requirements and RPATH will be inspected only if
fresh binaries are successfully produced. No compatibility floor is inferred
from a development-host `ldd` result.

### Notices and private packaging boundary

The source CMake install includes Unicode, Ryu Boost, date MIT/NOTICE, tzdata
license and stage10 pins. It installs the complete `third_party/http-licenses`
directory (including the old OpenSSL 3.5.9 notice, though AWS-LC is selected),
curl COPYING/NOTICE/provenance, current HTTP pins, public-suffix MPL notice/data,
AWS-LC's combined LICENSE/NOTICE, GNU library license texts, libidn2/libunistring
source archives, setup recipes and dependency manifest.

The source CMake install has no rule installing the LLVM license even though
`voidc`/`voidls` link LLVM. This is a notice-completeness gap to review before any
future distribution. `tools/package_tooling.py` instead requires a pinned LLVM
notice (its defaults assume distro LLVM23 notice paths), audits host dependencies
and tests an extracted server on the same host. It creates a private server-only
archive/optional VSIX, not a compiler SDK bundle; it does not install stdlib or
all notices through CMake. It explicitly disclaims self-contained portability.
No archive/VSIX publication or public licensing decision is part of this work.
This is an install inventory, not a legal opinion on license compliance.

## First-install documentation gaps

1. Protected README still says LLVM19 and Python 3.9 for tests; leave unchanged.
2. Current SOURCE_COMPILER Linux commands omit HTTP preparation, prefix/source
   verification and CA prerequisites. Following those commands cannot alone
   establish the full current contract.
3. SDK acquisition is described without a standalone non-CI entrypoint example;
   HTTP setup also assumes the exact source-local SDK path.
4. Stage9 native artifact section still names Clang19; editor/Stage9 stdlib
   relocation prose predates executable-relative source discovery.
5. Source build guide lacks a complete compiler + tooling component install,
   installed current `main.void` workflow and independent stdlib/LSP verification.
6. `docs/KNOWN_ISSUES.md` / LLVM acceptance opening platform-pending wording and
   `stdlib/README.md` HTTP-pending prose are dated; the authoritative
   [HTTP acceptance](../net/http/ACCEPTANCE.md) supersedes them. Preserve history.
7. No general distro/glibc/libstdc++ floor, portable SDK discovery or complete
   compiler-install LLVM notice inventory is established.

These findings are recorded, not silently repaired. No production, CMake,
Runtime, protected README or historical acceptance document is edited.

## Reproducible source-build sequence

The actual run used the pinned image above, `podman run --network host` with no
mounts, pristine `git archive` source under `/src/void`, and the verified official
LLVM archive extracted at `/src/void/.toolchains/llvm-23.1.4`. The SDK archive was
copied from a verified local download cache and rehashed inside the new container;
its network download and the CI installer entrypoint were not rerun.
The seven HTTP sources were downloaded afresh, hash verified and built there.

The following commands describe the successful container-side recipe. Exact argv
(including negative checks and fixture setup) remains in the receipts. Package
repository state is pinned by observed package versions, not by a snapshot mirror.

```sh
apt-get update
apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build python3 libffi-dev libedit-dev \
  libxml2-dev libzstd-dev zlib1g-dev libtinfo-dev \
  ca-certificates xz-utils curl git perl
cd /src/void
python3 tools/setup_http_deps.py --profile Release --jobs 3
python3 tools/verify_http_dependencies.py \
  .toolchains/http/prefix/linux-aws-lc-5.12.0/Release Release
cmake -S /src/void -B /work/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DLLVM_DIR=/src/void/.toolchains/llvm-23.1.4/lib/cmake/llvm \
  -DCMAKE_C_COMPILER=/src/void/.toolchains/llvm-23.1.4/bin/clang \
  -DCMAKE_CXX_COMPILER=/src/void/.toolchains/llvm-23.1.4/bin/clang++ \
  -DVOID_CLANG=/src/void/.toolchains/llvm-23.1.4/bin/clang \
  -DVOID_LINKER=/src/void/.toolchains/llvm-23.1.4/bin/ld.lld \
  -DVOID_DWARFDUMP=/src/void/.toolchains/llvm-23.1.4/bin/llvm-dwarfdump \
  -DCMAKE_EXE_LINKER_FLAGS=--ld-path=/src/void/.toolchains/llvm-23.1.4/bin/ld.lld
cmake --build /work/build --target voidc voidls --parallel 3
cmake --install /work/build --config Release --component compiler --prefix /opt/void
cmake --install /work/build --config Release --component tooling --prefix /opt/void
```

`BUILD_TESTING=OFF` selects the supported production-only build for this narrow
installation investigation. There is no full CTest, sanitizer, HTTP acceptance or
optimizer reproduction campaign. The optional `VOID_DWARFDUMP` cache input is
reported unused in this configuration because its consumers are tests.
`libtinfo-dev` is provided by Ubuntu's installed `libncurses-dev` package; it has
no separately installed dpkg version in the receipt.

For installed-use isolation, the task moved `/opt/void` to `/opt/void-relocated`
and moved **only the container's pristine source copy's** stdlib to
`/work/source-stdlib-preserved`. Both original paths were confirmed absent.
No production source bytes, install manifest or build recipe were repaired.
Positive checks unset `VOID_STDLIB_ROOT` and `LD_LIBRARY_PATH` and use installed
`bin` plus `/usr/bin:/bin` on PATH. Clang/LLD stay at their configured SDK paths.
[Actual inputs](evidence/probe-inputs.json) contain both source texts and hashes:

```void
func main() -> i32:
    return 0
```

The separate standard-module program is:

```void
use console

func main() -> i32:
    print("VOID installed stdlib OK")
    return 0
```

Commands were installed `voidc check`, `run`, and `build ... -o
/work/standard/hello-native`, followed by direct native execution. These use the
current synchronous application contract and default O2. No legacy entry ABI,
removed application flag or optimization workaround was used.

## Executed verification and classification

[Every command receipt](evidence/commands.jsonl) retains exact argv, selected
explicit environment, status, output and raw-output hashes. Longer stdout is
explicitly compacted; unmodified raw stdout/stderr is private. The
[result index](evidence/results.json) classifies every recorded command and lists
skips/unknowns. [Probe oracles](evidence/checks.jsonl) preserve the two original
harness failures alongside later corrected checks. No historical evidence was
removed or rewritten.

| Requirement / command IDs | Result and evidence |
| --- | --- |
| Initial checkout / preparation | PASS; clean `main` at recorded HEAD, mandatory-file hashes in [start receipt](evidence/qualification-start.json). |
| Fresh environment / `fresh-container-baseline`, `fresh-isolation-mounts` | PASS; new Ubuntu userspace, no mounts, no initial build tools. Shared host kernel/CPU/network limitation retained. |
| `fresh-host-packages`, `fresh-environment-versions` | PASS; exit 0. CMake 3.28.3, Ninja 1.11.1, Python 3.12.3, Make 4.3, Perl 5.38.2. GCC/libstdc++ headers 13.3.0; runtime libstdc++6 package 14.2.0; glibc 2.39. |
| SDK identity / `official-sdk-archive-hash`, `fresh-source-sdk-extract`, `fresh-sdk-provenance-config` | PASS; pinned archive digest, Clang/LLD commit/version, RELEASE, assertions OFF, RTTI NO, x86_64 target. |
| `fresh-http-source-build`, `fresh-http-prefix-verify` | PASS; all seven fresh sources, 22 child commands exit 0; 167 prefix files verified. [Dependency build](evidence/dependency-build.json). |
| `fresh-void-configure` | PASS; exit 0, matching SDK, CA bundle/path found, ORC compiled, linked and executed. This is plain Release, not a repeat of hosted ThinLTO-cache configuration. |
| `fresh-void-build` | PASS; 264 build actions, exit 0, `voidc`, Runtime, required native/provider closure and `voidls` built. [Warning/marker review](evidence/build-log-review.json). |
| `fresh-install-compiler`, `fresh-install-tooling` | PASS; both supported CMake component installs exit 0. |
| `fresh-relocation-source-isolation`, `installed-cli-path-discovery` | PASS; installed prefix relocated, original stdlib unavailable, PATH selects installed compiler/server. |
| `installed-minimal-check-corrected`, `installed-minimal-run` | PASS; check exits 0 with current success text; run exits 0 with empty stdout/stderr. |
| `installed-standard-check`, `installed-standard-run` | PASS; check exits 0; run exits 0 and prints exactly `VOID installed stdlib OK` plus newline. |
| `installed-native-build`, `installed-native-execute` | PASS; build exits 0; real native executable exits 0 and prints the same exact line. SHA-256 in [input/artifact receipt](evidence/probe-inputs.json). |
| `installed-server-project-discovery` | PASS; entry `/work/standard/main.void`, root `/work/standard`, `standalone:false`. |
| `installed-server-handshake` | PASS; real Content-Length framed initialize/initialized/shutdown/exit, protocol 1 advertised, UTF-16 capability, shutdown result null, exit 0, no error response. [Requests and responses](evidence/lsp-handshake.json). No actual editor UI was tested. |
| `installed-layout-loader-audit` | PASS; 86 installed files/aliases hashed, all 21 installed standard sources match original bytes; six ldd/readelf commands exit 0, no unresolved library. [Installed layout](evidence/installed-layout.json). |
| `negative-missing-http` | PASS expected rejection; configure exits 1 with exact `python3 tools/setup_http_deps.py --profile Release` instruction. Class: missing/incorrectly documented dependency; confirms first-install documentation gap. |
| `negative-missing-standard-root` | PASS expected rejection; exit 1, VOID3002, unavailable source root and absolute matching-root hint. Class: missing standard-library prerequisite / install-discovery boundary. |
| `negative-native-manifest` | PASS expected rejection; disposable copied prefix lacks `void-native.txt`, exits 1 naming missing/invalid resource file. Class: native toolchain/packaging prerequisite. |
| `negative-native-driver` | PASS expected rejection; disposable manifest points at `/missing/clang`, exits 1 naming that missing toolchain resource. Class: native toolchain discovery/host path prerequisite. |

These negative controls intentionally alter disposable installation fixtures;
they are not unexpected product regressions and did not require production fixes.
No compiler/Runtime correctness failure or new optimizer incident was observed.
That narrow observation does not establish a cause or cure for older incidents.

### Preserved harness and preparation failures

- `fresh-probe-fixtures`: Python `-c` quoting produced an unterminated string,
  exit 1, before creating/running a VOID source. `fresh-probe-fixtures-corrected`
  passed. The original command, stdout/stderr and failed oracle remain.
- `installed-minimal-check`: VOID exited 0 and printed `Source checked successfully`.
  The probe incorrectly expected empty stdout and stopped. `src/main.cpp:65`
  confirms the current success contract. The corrected harness check passed;
  the original erroneous oracle is retained as FAIL. No output was suppressed,
  compiler source changed or required exit status weakened.
- Read-only inventory tooling: `rg` was absent (initial combined command exited
  127), so subsequent searches used Python/find. Broad home `find` ended 1 with
  stderr suppressed (the precise cause was not retained); targeted ancestor/repository checks
  found no applicable AGENTS file. A guessed example path
  `examples/stage10_console_time/console.void` gave `sed` exit 2; the actual
  `main.void` was read. A guessed `tools/setup_llvm23.py` did not exist; the actual
  helper is `tools/ci/llvm_sdk.py`. These are not installation failures.

## Observed portability and notice limits

`voidc` and the native program both loaded libidn2/libunistring from the relocated
installation rather than the older ambient Ubuntu copies. `voidc` has executable-
relative RUNPATH `$ORIGIN/../lib/void`. The generated application has an **absolute**
RUNPATH to `/opt/void-relocated/bin/../lib/void`; moving/removing that prefix after
linking is not certified. Native driver and LLD paths also remain absolute in the
manifest. This is a source installation with retained SDK prerequisites, not a
portable compiler/application archive.

Actual DT_NEEDED: `voidc` uses libidn2.so.0, libunistring.so.5, libstdc++.so.6,
libm.so.6, libz.so.1, libgcc_s.so.1, libc.so.6 and the ELF loader. `voidls` uses
libm, libz, libstdc++, libgcc_s, libc and the loader. The native standard-module
program uses libidn2, libunistring, libstdc++, libm, libgcc_s and libc; it has no
LLVM library dependency. The install audit establishes this image's loader
resolution, not a general minimum glibc/libstdc++ ABI or target-machine guarantee.

The inventory confirms HTTP/common notices and corresponding GNU source archives
were installed, including an unused historical OpenSSL notice. It contains no
LLVM-named notice and the source CMake rules install none. Review that gap before
future distribution; no legal compliance or redistribution approval is inferred.
Build warnings include date literal-operator deprecations/unused functions and
missing aggregate-field initializer warnings in console/proof source. They remain
in the receipt without changes or suppression. No crash/sanitizer marker appeared
in the completed build log; this Release run is not sanitizer coverage.

## Skips, unknowns and review disposition

- SKIP: full CTest/Debug/strict matrix, HTTP tests, logging/performance work,
  LLVM19 reproductions, preserved-artifact causal investigation, editor UI,
  VSIX/server archive packaging, Windows/macOS, Git mutations and publication.
- UNKNOWN: independent physical/VM execution, other distros/older ABIs, full
  missing-prerequisite matrix, absent-CA diagnostic behavior, SDK-download
  availability from a new network, native executable relocation after prefix
  movement, all real editor/client combinations, complete license clearance.
- Existing historical Linux LLVM19 cause stays unresolved and its campaign closed.
  Current optimizer risk uses LLVM23 evidence: the three previously passing bounded
  DSE-associated checks, seven hosted jobs, and this narrow installed-use result.
  The separate Windows LLVM23 incident remains separately classified. No upstream
  defect or upgrade-related causal repair is proven.

No new environmental blocker prevented this fresh-userspace attempt. The Linux
container baseline is evidenced and ready for Golden Wrench review; acceptance of
that scope, independent-host portability and broader preview risk remain review
boundaries. The [blocker register](PREVIEW_BLOCKERS.md) retains separate licensing,
logging, optimizer, documentation and platform dispositions.

Raw dependency/configuration logs, exact failed commands and artifact identities
remain private. The task container is stopped and retained for resumption, not
removed or published. Its valid install is `/opt/void-relocated`; the intentionally
incomplete negative-control copy is `/opt/void-missing-manifest`. Production files
in the actual checkout and the protected README remain unchanged.

Container cleanup note: `podman stop --time 10` exited 0 after warning that PID 1
(`sleep infinity`) did not stop on SIGTERM and required SIGKILL. No VOID probe was
running then; this is container housekeeping, not a compiler crash or unknown
qualification exit. Final inspection reports `exited` with no mounts.

Documentation-check bookkeeping: `git diff --no-index --check /dev/null` against
an entirely new handoff returned 1 with empty stdout/stderr. The recorder had
expected 0; this difference-status assumption was incorrect. The original
receipt is retained, and the final Python validation checks whitespace across
all three Markdown documents directly. It is not a product/test failure.
