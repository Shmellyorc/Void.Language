# Slice 1 closeout and local continuation handoff

8 November 2026. Baseline/unchanged HEAD:
`2f667ea338127b8f91c3136804377ef3fa5218d6`.

**Slice 1 is locally qualified for independent review.** Linux Release, Debug and
fatal strict each pass 25/25 selected CTests. Windows **10** x64 Release/Debug
standalone parser/header consumers also pass, with the narrower scope below.
Golden Wrench review is pending; S2 has not started. No staging, commit, push,
hosted CI, dependency-pin change, security relaxation or binary publication.

This record supplements the preserved [original engineering report](SLICE1.md)
and [original receipt](evidence/slice1-local.json). Those two files, the original
malformed fixture and the already corrected parser/vendor patch are byte-for-byte
unchanged. All fifteen Phase 1 documentation changes remain: thirteen unchanged
from the closeout checkpoint; the central roadmap and decision ledger receive
the new local-continuation/hosted-gate direction.

Review artifacts:

- [Closeout source delta](evidence/slice1-closeout-source.patch): exact changes
  from the captured starting working tree, including newly changed notice files.
- [Complete Slice 1 source/architecture diff](evidence/slice1-baseline-source.patch):
  exact source and architecture documentation changes against the baseline.
- [Machine-readable closeout receipt](evidence/slice1-closeout.json): hashes,
  commands/results, platform limits, preserved failures and checkpoint inventory.
- [Exact expanded Git inventory](evidence/slice1-closeout-inventory.txt): every
  modified/untracked path, including reports, receipts and raw evidence.

Both patches exclude `docs/projects/`, whose reports/evidence are listed separately
in the inventory and receipt. This avoids recursive patches containing themselves.
Neither patch uses or changes the Git index. Reverse checks and reconstruction
checks confirm that they reproduce their stated source scope exactly.

## Header, parser and provenance audit

`project.hpp` directly uses `std::size_t`, `std::shared_ptr`, `std::string` and
`std::vector`; it now directly includes `<cstddef>`, `<memory>`, `<string>` and
`<vector>`. Its existing `<array>`, `<compare>`, `<map>`, `<optional>` and
`<string_view>` includes cover the other standard declarations. It still exposes
no toml++ types. The new `void_project_header` executable includes only that public
header and exercises its declarations/inline operations. It links only
`void_options`; precompiled headers and unity builds are explicitly disabled.
All three Linux profiles and both native Windows consumer profiles compile/run it.

A fresh GitHub tag lookup resolves toml++ `v3.4.0` to unchanged pinned commit
`30172438cee64926dc41fdd9c11fb3ba5b2ba9de`. Fresh header/license downloads at that
immutable commit match the pin. Applying the complete existing private patch with
system `patch --batch --fuzz=0 -p1` produces exactly the vendored header. Offline
reverse reconstruction independently obtains the same original bytes.

| Material | SHA-256 |
| --- | --- |
| Upstream `toml.hpp` | `6b5172ad4dd6519aec67b919181fa7a38a2234131e5b2afa232dfe444819783e` |
| Upstream MIT license | `529bc3900a9571e49db285b0df432397e70b881cc3bf48de6667ae74ff4b06d8` |
| Complete private patch | `d131e7ff7daa6373904406403b8f23def1b0dc02127de8230c1a09cdace3330f` |
| Patched header | `7cd54af1a3497465eb26507f8d8a2ccfe661fccc27dac7a5121b885bee88d5e7` |

This verifies the retrieved source/tag and byte identity; it does not claim signed
publisher authentication. No dependency source, patch, configuration or pin changes
in closeout. The private guard still rejects invalid key starters before the
upstream assertion/optimizer assumption; key bounds and duplicate-source handling
remain intact.

Each compiler/tooling component now also installs the exact patched header and
README under `share/void/licenses/tomlplusplus/corresponding-source`, beside the
existing license, provenance and complete patch. All five files are in the shared
notice inventory, and the redistribution guide identifies this compiler/tooling
dependency. Separate real component installs match all five source files and pass
offline upstream reconstruction from the installed layout. The existing notice
checker passes all five groups, including Windows **file-rule selection only**;
that check is separate from the native execution below.

## Expanded regression evidence

The existing suite remains enabled. New coverage adds thirty malformed key/header
fixtures, both raw and after BOM/astral-Unicode/CRLF prefixes, plus all 128 ASCII
bytes in four header/inline-key positions. Repeated parses must reject the
incomplete manifests deterministically and keep diagnostic ranges within the
captured input. Existing application mutation tests remain.

Exact half-open byte ranges include `[=roject]` at `[1,1)`, the prefixed form at
`[12,12)` (line 2, column 2), duplicate second declarations at `[11,11)` and BOM/CRLF
`[15,15)`, related first keys at `[0,6)` and `[3,9)`, an unknown key at `[9,14)`,
the full invalid quoted version after astral Unicode, EOF missing-field ranges,
escaped quoted keys and multiline string tokens.

Boundary pairs cover 256 KiB/256 KiB+1 input, 32/33 roots, 256/257 exclusions,
128/129 errors, 255/256-byte core/prerelease/build versions and 128/129-scalar
Unicode display names. Existing deep-value/key rejection remains. Independently
specified SemVer order exercises all pairs of 21 versions, antisymmetry,
transitivity, unbounded decimal widths, ASCII identifier ordering and build
identity versus precedence.

Ownership checks destroy/replace the original bytes, SourceFile, parser result
and Manifest, churn allocations, and then use retained identities, version
components, roots, source snapshots, copied fields and copied diagnostics.
Standalone SemanticVersion also survives input replacement. No returned view
depends on the TOML parse tree or caller-owned string storage.

| Executed local check | Release | Debug | Fatal strict |
| --- | --- | --- | --- |
| Selected Linux CTests | 25/25 PASS | 25/25 PASS | 25/25 PASS |
| Ordinary parser fixture | 8,787 checks | 8,787 | 8,787 |
| Syscall-denied parser fixture | 8,788 checks | 8,788 | 8,788 |
| Live file/process/network/mutation denial controls | four PASS | four PASS | four PASS |
| Parser fixture with 1 MiB main stack | 8,787 PASS | 8,787 PASS | 8,787 PASS |
| Legacy explicit-file/directory/JIT/AOT fixture | 15 PASS | 15 PASS | 15 PASS |

The matrix is 75/75 selected configuration/test outcomes, not the complete
repository suite. Check counts include assertions and deterministic reparses.
The new fifth project CTest is the header consumer; twenty existing CTests retain
CLI, frontend, numeric/module, tooling/session, JIT/AOT, OS, LSP, workflow and cache
coverage. Release `cli_install` additionally passes 1/1, including relocated
installed execution and missing-resource/output-preservation controls.

Host remains PikaOS 4 x86_64, LLVM/Clang/LLD 23.1.4. Strict remains RelWithDebInfo
with `VOID_SANITIZERS=ON`, with exactly:

```text
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86
```

Ordinary strict runs retain leak checking. The existing filtered-child `_Exit`
behavior remains because LSan's own exit-time procfs/helper activity violates the
denial filter; the ordinary strict test separately checks exit-time leaks.

## Windows 10 native execution and limits

Actual guest: Windows 10 x64, build 19045; native LLVM/Clang/LLD 23.1.4, VS 2022
MSVC 14.44/Windows SDK environment. The VM's misleading display label does not
identify its OS. No VM settings, ACLs, SDK/provider pins or original native source
files were changed.

The exact current parser/header/test inputs were copied and hashed in an isolated
native directory. Release compiles with `/O2 /DNDEBUG`; Debug uses `/Od /Zi` with
assertions retained. Both use the existing matching `/MT`, iterator and RTTI ABI.
Both header executables pass; each parser executable passes **8,788 checks**. The
extra check versus ordinary Linux verifies that Linux-only syscall-denial mode
was not requested. Native offline provenance passes. Existing HTTP and host
provider verifiers pass separately for both profiles.

These executables link newly compiled parser/test objects against the **preexisting
native `void_core.lib` and its LLVM/dependency closure**. Core/build and current
input hashes and exact compiler/link commands are retained. This establishes
native parser/header-consumer execution, not a full current-tree CMake/compiler,
CLI, editor, packaging or sanitizer qualification. Three deprecated literal-
operator warnings originate in the unchanged upstream header in each manual
consumer compilation; no warning suppression or vendor edit was added.

A fresh complete native configure was attempted and retained: incomplete source
transfer, omitted approved curl review patches, omitted HTTP/host source archives,
then the isolated SDK layout required by the unchanged provider verifier. The
transfer/source omissions were corrected; the full configure remains failed at
SDK layout, and its result is not relabelled PASS. Focused native consumers use
the original verified layout. Windows Server 2022, Windows fatal strict, macOS,
actual editor UI and hosted acceptance remain **UNQUALIFIED**.

## Preserved failures and source boundaries

The original `[=roject]` Debug assertion and strict UBSan-assumption failures remain
in the unchanged original receipt. Their raw failed XML/logs are additionally
copied into `evidence/closeout/`, with matching original hashes. The original
reproducer and the corrected parser/vendor source are unchanged. Expanded tests
now pass both the original case and the wider grammar corpus in every executed
parser profile; no test, assertion or sanitizer was disabled.

Closeout also retains its initial Make invocation failure (new target requested
before reconfiguration), native transfer/configure failures, and the first notice
check's stale redistribution-document hash. Explicit reconfiguration fixes target
discovery; copying the missing source inputs fixes transfer/archive checks; the
notice inventory now records the intentionally updated redistribution document.
Corrected results remain alongside original failures. These setup failures are
distinct from the original parser UB and from the still-unqualified full native
build.

All original 35 working-tree files are checkpointed. The parser implementation,
portable-path extraction, diagnostic registry, all existing negative tests,
toml++ source/license/pin/patch, original receipts and protected inputs are
preserved. Closeout changes eleven source/documentation paths plus this report,
receipts and evidence. The exact inventory names every path; no unrelated file
was staged, discarded or rewritten. HEAD and the empty index remain unchanged.

## Next architectural boundary

Stop for Golden Wrench's Slice 1 review. The next prepared scope is central-roadmap
S2: compiler-owned project-root versus root-scope separation, bounded physical
admission, reusable captured source inventory, parse-only D versus semantic E
selection, deterministic main selection, nested boundaries and library checking.
Its gates are P01–P19 and P24–P28, explicit-file/manifestless compatibility,
containment/case/reparse/ownership negatives and disk/overlay parity. Reuse the
existing compiler/VFS/frontend parser; introduce no new LLVM path. Confirm any
material discrepancy with Boss/Golden Wrench before dependent changes.

S3 protocol/editor inputs, S4 Runtime metadata/OS adapters, S5 exclusive creation
and editor offers, and S6 project qualification remain subsequent review slices.
Numeric enum syntax/results, general C ABI/loading, dependency constraints/locks,
raylib revision/static-mode support and remaining editor/native packaging choices
retain their explicit decision gates. None is silently implemented here.

Boss's [continuing authorization and final hosted gate](../PROJECT_ARCHITECTURE_ROADMAP.md#continuing-local-authorization-and-final-hosted-gate-8-november-2026)
govern further work: no hosted CI before the full project/raylib demonstration,
then one integrated seven-job acceptance run under Boss's authority. Optional
static linkage cannot replace dynamic native linkage.
