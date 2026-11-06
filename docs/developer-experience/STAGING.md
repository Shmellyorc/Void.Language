# Developer-experience staged candidate — Golden Wrench review

6 November 2026. **RETAIN; staged for review. No commit, push, branch, CI dispatch,
Wiki publication or website deployment.** HEAD remains `d517459d7ad89843f1b5a09bf6b53a782a68a9e1`
on `main`. The accepted starting compiler is `175c08c8`, seven-job green run
`42231556437`, followed by the `.gitignore`-only commit.

## Exact staged scope

**113 exact paths: 15 modified, 98 added.** The [literal staged-path allowlist](staging/STAGED_PATHS.txt)
is authoritative; every path was added individually through a literal NUL-delimited
pathspec file. No directory, wildcard, blanket add, forced ignored-file add or
unrelated index entry was selected.

| Group | Paths |
| --- | ---: |
| Compiler/build/install/test source and root README | 12 |
| Canonical documentation and accepted implementation report | 16 |
| Approved Wiki drafts/maintenance files | 16 |
| Canonical Hello World/learning programs | 7 |
| Original qualification evidence | 47 |
| This staging report and staging evidence | 15 |
| **Total** | **113** |

The [accepted engineering report](REPORT.md) and its original failure records are
preserved byte-for-byte. Website audit records in that report are documentation,
not a staged website publication. The prior 108-path handoff inventory intentionally
continues to describe the whole prior working tree, including ten now-excluded
website proposal files; it is not the staging allowlist.

## Required CLI header correction and qualified-source comparison

The retained implementation's help started with `VOID source compiler`, which did
not satisfy Boss's version-first requirement. This preparation makes the narrow
required adjustment: help begins with `VOID Language Compiler v` plus the
authoritative CMake compiler version, with LLVM directly underneath, followed by
usage, commands and the existing options section. All command help forms and
`--version` share that full-name header. `--version` returns before standard-source
configuration. Its target/build identity fields and other output lines remain intact.

Only **three** of the original 108 handoff files changed during preparation:
`src/main.cpp`, `tests/cli/contract.py` and `docs/CLI.md`. These implement/test/document
the requested header and source-independent metadata reporting. The other **105**
files, including the verified installer, Runtime/native driver diagnostics, README,
Wiki drafts, examples and original failure evidence, retain their handoff hashes.
[Complete before/after SHA-256 comparison](staging/QUALIFIED_COMPARISON.json).

Key installer/CMake/native/test hashes were checked against the retained final
Ubuntu receipt. The stopped qualified container also supplied independent byte
comparisons for the original driver sources, all CLI/learning tests and all seven
canonical programs before this correction. Original native-workflow repairs remain
unchanged. Backend, frontend, Runtime, headers, stdlib, workflows and `.gitignore`
remain untouched. No new optimization or performance qualification occurred.

## Actual compiler output

Captured from the rebuilt local Release compiler, status 0 and empty stderr for
both commands. The [full help capture](staging/cli-help.txt) is:

```text
VOID Language Compiler v0.1.0 (development)
LLVM 23.1.4

Usage: voidc check <file-or-directory> [--no-opt]
Usage: voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--no-cache | --cache]
Usage: voidc build [file-or-directory] [-o executable] [--no-opt]
Usage: voidc emit <file-or-directory> -o <object> [--no-opt]

Commands:
  check   Analyze source and imports without executing startup code.
  run     Execute through the JIT.
  build   Compile and link a native executable with the matching Runtime.
  emit    Write a native object for manual linking.

Options:
  -h, --help  Show help; use <command> --help for examples.
  --version   Show compiler, LLVM, target and build identity.
  --no-opt    Disable O2 (accepted as a no-op by check).
  --cache     Select Debug code and report cache status (run only).
  --no-cache  Disable disk caching (run only).
  -o PATH     Set output path (build/emit only; required by emit).
  --          End options before a source path.

A directory selects main.void. run/build default to the current directory.
Application entry selection is automatic (main or executable top-level code);
--app has been removed. Use -- before a source path beginning with '-'.
Quote paths containing spaces. Options may precede or follow operands.
Exit status: 0 success; 1 compilation/toolchain failure; 2 invocation error.
run propagates an application's process status.
Documentation: https://voidlang.com/docs/compiler-workflow/
```

`voidc --version` ([capture](staging/cli-version.txt)):

```text
VOID Language Compiler v0.1.0 (development)
LLVM 23.1.4
Target: x86_64-unknown-linux-gnu
Build identity: cbc95fce599e38f5bda210e257c1b70869d3f745c63e184c8551d623657a7def
```

`VOID_COMPILER_VERSION` comes from CMake `PROJECT_VERSION` (`0.1.0`); LLVM version
and default target come from the selected LLVM SDK configuration headers. Build
identity is the existing generated constant in `cmake/CacheBuildIdentity.cmake`,
not a runtime source/Git lookup. Version and help were exercised using a copied
executable without installed stdlib/native manifests, an invalid source override,
an empty profile and a source-free cwd. Both succeed without creating source,
cache or output files; the empty profile stays empty. The regression compares
standalone version bytes to normal invocation. `--app` still rejects, including
alongside help. Normal rebuilt-compiler cache fingerprint changes remain active.

## Installer and CTest verification

The installer and verifier are byte-identical to the accepted final Ubuntu-tested
versions. Before the final full-name presentation correction, a warm source-build/install
using the unchanged entry point passed into `/tmp/VOID DX staged install é`. It installed both existing
CMake components, then verified installed stdlib, optimized/Debug run, native
build/run, resource hashes and LSP protocol 1. [Command log](staging/installer.txt)
and [exact installer receipt](staging/source-install.json).
The earlier fresh-download Ubuntu onboarding record remains unchanged in the
accepted report; this header correction is not relabeled as a new fresh-host run.

Focused requalification after the final full-name correction (including real
CMake installation of the updated compiler through `cli_install`):

| Native local configuration | Result |
| --- | --- |
| Release | 4/4 passed — [CTest](staging/release-ctest.txt) |
| Debug | 4/4 passed — [CTest](staging/debug-ctest.txt) |
| Strict RelWithDebInfo, ASan/UBSan/leaks | 4/4 passed — [CTest](staging/strict-ctest.txt) |

The four selected tests are `cli_contract`, `cli_install`, `onboarding_examples`
and the existing `application_workflow_stage9`. The expanded CLI contract now
exercises **76 real processes** per configuration, including the new metadata
controls. No broad compiler/Runtime suite or benchmark was rerun.

All three required entries are inside the ordinary root `BUILD_TESTING` block,
with commands/timeouts, no OS guard and **no disabled/skip properties**. The
[actual CTest JSON registration](staging/ctest-registration.json) confirms their
presence in all three local configurations. The existing standard and seven-job
`ci-link-experiment.yml` compiler workflows build tests and invoke CTest for Linux
Debug/Release/Sanitizers, Windows Debug/Release and macOS ARM64 Debug/Release.
`onboarding_examples` uses the existing built async adapter on every native host;
missing/unsupported execution fails rather than being silently skipped.
The separate source-install qualification workflow deliberately uses tests-off
production builds and its existing installed smoke path; it is not substituted
for compiler CTest registration.

No new native Windows/macOS execution is claimed. Those wrapper/platform cases
remain for Boss-controlled cross-platform qualification. No workflow was changed
and no CI was dispatched. Existing source-install, portable-package, async-host,
platform/ABI and licensing boundaries are preserved in README/canonical guides.
The Wiki remains repository drafts with its explicit publication review notes.

## Excluded website publication proposals

Deployment linkage cannot be established from this checkout, so **all ten**
website source/generated/checker proposals remain unchanged and unstaged. The
[exact exclusion list](staging/EXCLUDED_WEBSITE_PATHS.txt) is:

```text
website/dist/docs/compiler-workflow/index.html
website/dist/docs/modules/index.html
website/dist/getting-started/index.html
website/dist/roadmap/index.html
website/public/docs/compiler-workflow/index.html
website/public/docs/modules/index.html
website/public/getting-started/index.html
website/public/roadmap/index.html
website/scripts/check.py
website/scripts/test_check.py
```

No website path is present in the staged allowlist. These files remain available
locally for a separate approved publication change. Website evidence retained in
repository documentation does not publish the site. GitHub Wiki drafts are staged
as repository documentation only; no Wiki repository was written.

## Changed-source and fixture hashes

SHA-256, actual staged bytes. [Machine-readable source/fixture hashes](staging/HASHES.json)
also include every one of the seven `.void` examples.

| Source / regression path | SHA-256 |
| --- | --- |
| `CMakeLists.txt` | `7fdceafec4681e99968c3f1ef7e877d7572e6a137fd5ac13bdca00b99f521ec8` |
| `cmake/CurlProvider.cmake` | `7a0fd3f14097dc22794059d48261d6dd00ec7856e353a0499d907808c742be73` |
| `src/main.cpp` | `756e50bee5173e9a48cf04c9e0aae420a8de531f020906937e65579c5f5a9348` |
| `src/native_build.cpp` | `939bea01e8fa52457591d704cec5a54f65b64a12413274b43ae9499f437779d0` |
| `tools/ci/llvm_sdk.py` | `a043f5c82a80a747b1417d1635804b2195f002d92617188513b79a846659b355` |
| `tools/install.py` | `9f755a9cd5b50a9ca6536373a55c8135086ae7e1029a9d15906222428bab8ffd` |
| `tools/verify_install.py` | `f02f418623278a7c741c2037daafcd28c5d05e39434cc988e4b92be2b6bc5e3a` |
| `tests/cli/contract.py` | `a760dfb536308390ac4c480491b373347b3a676c0ba791a95b822a9b1db92afc` |
| `tests/cli/install.py` | `ed9c3ed4af5fb44423f4ee43af9cd29642e2711ae7a5bc17bda3fae2e7ca8301` |
| `tests/cli/learning.py` | `7dc311019289e749f6a83b70d0e4fee5f84a83db8a11863797ea30f8b63586f6` |
| `tests/stage9/workflow.py` | `6a54c5d690796781ee4030a969c38ecd9bc77915b7f80c0b5316bdb3a7b2c202` |

[Final validation](staging/VALIDATION.json) binds exact index paths to the allowlist,
checks index bytes against the working files and source/fixture hashes, verifies
staged Markdown targets/anchors, and checks source/fixture/document whitespace.
All 426 staged Markdown file/directory targets and 17 fragments resolve in the
index, including 33 prospective main-branch links. Source/fixture/canonical-document
whitespace passes. The full-index check reports 11 original trailing-space notices
in `evidence/ubuntu-fresh-install.txt` (captured CMake output); those original bytes
are preserved rather than normalized. No source/fixture whitespace failure exists.

## Preservation, blockers and next action

[Preservation receipt](staging/PRESERVATION.json): all 47 original evidence files
are byte-identical; all ten excluded website files are byte-identical; **18,930**
ignored experimental/evidence paths retain their sizes and mtimes. The ignored
inventory is local and hashed; no broad deletion, reset or cleanup was performed.
Build outputs/cache files were regenerated only as needed for this focused check.
No unrelated worktree/index changes were selected.

**Staging blockers: none.** Native Windows/macOS qualification and independently
portable distribution remain explicit pending work, not invented local passes or
new claims of package/release approval. Original failure receipts remain intact.
All links to new repository content refer to the eventual approved candidate;
Wiki/publication notes retain that boundary.

Suggested commit title:

**Improve source installation and compiler CLI onboarding [skip ci]**

After Boss review and separately authorized commit/push, the existing seven-job
compiler workflow can qualify this immutable candidate. This preparation has not
performed any of those actions. **Stop for Boss/Golden Wrench review.**
