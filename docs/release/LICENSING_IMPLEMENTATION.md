# MIT licensing implementation — Golden Wrench handoff

Owner selected MIT and authorized implementation after the accepted audit/plan.
Baseline: `95ce0a76e9a95198cd4fe9d5b6bb4cbfad9bd5c5`. Changes are uncommitted;
no public release, package, commit, push or CI dispatch occurred.

## Delivered

- Root `LICENSE`: standard MIT text, approved `Copyright (c) 2026 shmellyorc`
  attribution. Git corroborates that identity/year, not every imported right.
- [LICENSING_SCOPE](LICENSING_SCOPE.md): original compiler, Runtime, stdlib,
  language server, editors and development tools distinguished from vendored,
  generated, borrowed and unclassified material. No repetitive source headers.
- `cmake/InstallLicenses.cmake`, called inside the existing compiler/tooling
  installation loop: MIT, scope/index/inventory, exact LLVM 23.1.4 and embedded
  notices, zstd plus embedded notices, independent zlib text, and Windows-only
  libxml2 notices. No target, dependency, linker or Runtime behavior changes.
- New retained notice copies under `third_party/llvm-licenses/23.1.4/`,
  `third_party/llvm-dependency-licenses/{zstd-1.5.7,libxml2-2.15.4}/`, and
  `third_party/http-licenses/libpsl-0.21.5/LICENSE.chromium`. The existing HTTP
  directory installation rule picks up Chromium without another CMake change.
- VS Code original-source manifest/root lock metadata and source README now say
  MIT. Dependency entries and private packaging restrictions are preserved;
  no extension bundle/VSIX was rebuilt.
- [Component-to-license index](REDISTRIBUTION_NOTICES.md), installed as
  `share/void/licenses/REDISTRIBUTION.txt`, includes prominent GNU use notice,
  source/PSL locations and downstream native-program duties.
- [Machine inventory](NOTICE_INVENTORY.json): 14 new/common entries and 33
  retained input entries, with exact source URLs, source/text SHA-256 values,
  unchanged extraction ranges, component/platform and destination mappings.

The bounded source reconciliation also retained LLVM Support regex BSD notices,
Unicode name-data attribution and hash implementation origins, plus libxml2's
separately attributed `timsort.h`. zstd xxHash uses its own upstream BSD/GPL
alternative; it is not treated as LLVM's differently licensed xxHash copy.
Conservative common notices do not establish binary inclusion of every source.

## Focused validation

[Validation receipt](evidence/mit-licensing/validation-attempt1/validation.json)
and adjacent command logs preserve the actual results:

| Check | Result | Measured coverage |
| --- | --- | --- |
| Source/notice inventory and editor license metadata | PASS | 47 inventory entries match source hashes; original-source metadata MIT |
| Linux compiler component install, separate fresh temporary prefix | PASS | 45 mapped license inputs plus inventory; all 21 stdlib sources byte-identical; compiler, Runtime/native archives and manifests present; GNU source archives match pins |
| Linux tooling component install, separate fresh temporary prefix | PASS | 17 mapped license inputs plus inventory; all 21 stdlib sources byte-identical; server present; no compiler Runtime archive directory |
| Windows compiler notice rules | PASS | 13 common/Windows mapped inputs plus inventory, including libxml2 |
| Windows tooling notice rules | PASS | Same independent coverage, including libxml2 |
| Native Windows/macOS binaries or installations | UNKNOWN / NOT RUN | Windows cases exercise the actual production CMake notice helper with WIN32 selected in an isolated no-language file-install project; they are not native-platform qualification |

Reproduce the focused Linux check with:

```sh
python3 tests/release/check_licenses.py --build-dir build/llvm23-release --evidence-dir /tmp/void-license-check
```

The existing Release build was reconfigured to regenerate installation rules
([configure log](evidence/mit-licensing/configure.log)); no compiler/Runtime
rebuild or regression campaign ran. Existing install manifests were preserved
before the temporary installs. Temporary prefixes were removed after assertions;
the receipt and logs remain. These are focused licensing checks, not a reopening
of accepted installation or HTTP qualification.

One engineering helper failure is preserved in
[notice-copy-attempt1.log](evidence/mit-licensing/notice-copy-attempt1.log): the
leading-comment scanner encountered LLVM `DebugCounter.cpp` / `Locale.cpp`, which
begin with includes. The scan was corrected to record those under the upstream
umbrella terms without inventing file notices. The successful extraction log is
retained separately. There were no failed component-install check groups.

[Preservation/provenance verification](evidence/mit-licensing/preservation-and-provenance.json)
reconstructed all **10 upstream notice files** byte-for-byte from their recorded
tagged sources/extraction ranges and confirmed **2,898 pre-existing inputs** and
the four existing compiler/server/Runtime/fixture build artifacts unchanged.
[MIT text verification](evidence/mit-licensing/mit-text-verification.json) compares
the complete standard wording against SPDX after substituting the approved
copyright line and normalizing whitespace/display title. Both checks PASS.

## Remaining obligations and gate disposition

**MIT application and the audit's known notice-installation omissions are
implemented and locally verified.** The audit and planning files remain historical
evidence, not rewritten success claims. The current blocker register records the
new results and these limits:

- Rights to initial imports/unrecorded or employer/contractor contributions and
  any borrowed fragments are not independently established by Git. The owner's
  grant is applied to original code; no assignment from another party is claimed.
- Additional PSF/Perl-derived Unicode attribution and reference-vector provenance
  remain unresolved. Relevant generated material is explicitly separated.
- GNU source/rebuild usability, compliant replacement with editable resource
  hashes, distribution terms and conditional installation-information obligations
  remain open. Adding license texts/use notice does not resolve LGPL compliance.
- Final Windows/macOS linked-source closure, Microsoft `/MT`/DIA redistribution
  conditions and Apple SDK-derived terms remain UNKNOWN. No platform clearance
  follows from the Windows file-rule checks. SDKs/system libraries remain external.
- Optional VSIX/npm/native-server package notice duties and public-release
  packaging/portability remain outside this component-install scope.

**Release HOLD remains. LOG-PERF is OPEN:** heavy framing/console PASS, file200
FAIL at 20.091 ms against unchanged <10 ms; allocator experiment REJECTED.
Historical optimizer failures, accepted installation/HTTP evidence and protected
root README remain intact. Stop for Golden Wrench review before Git actions.
