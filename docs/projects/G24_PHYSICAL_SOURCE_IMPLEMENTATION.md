# G24 — physical raylib source and standalone distribution proposal

Baseline: `6a7cb50b2d35023324053694275596c9a4a7b328`.
This is an uncommitted conversion of the integrated implementation. Canonical
development remains `verified-packages/raylib/`. No public repository was created
or pushed, and no main-worktree changes were staged or committed. Hosted CI was
not used.

## Physical source and preserved API

The library manifest selects `src/raylib.void`. Its only native import is the
ordinary relative import `use "./raylib_native.void" as native`. Both files exist
on disk. There are no VOID `.inc` files in the package, and the package no longer
imports or synthesizes `/raylib_checked.void`.

The former `src/api.void` implementation is preserved byte for byte after its
banner/import replacement. Its callback implementation was already incorporated;
the redundant `src/callbacks.void.inc` maintenance fragment is removed. There are
757 public-module functions, including helpers, and 772 checked native
declarations plus 24 record/resource declarations. These are not inflated into
the upstream count. The unchanged [600-entry inventory](../../verified-packages/raylib/inventory.json)
accounts for exactly:

| Exposure | Upstream declarations |
|---|---:|
| Direct C ABI mappings | 236 |
| Ownership adapters | 352 |
| G5 callback lifecycle adapters | 10 |
| Typed safe equivalents | 2 |
| Total | 600 |

The inventory's `complete_binding: false` continues to distinguish unrestricted
C ABI coverage from complete usable VOID coverage. `TraceLog` and `TextFormat`
are typed equivalents, not arbitrary C varargs. The ten callback adapters,
ownership restrictions and actual C declarations remain unchanged; see
[ownership contracts](../../verified-packages/raylib/OWNERSHIP.md) and the
[integrated implementation](G23_G24_IMPLEMENTATION.md).

The package owns `native/raylib_provider.c/.h` and the base descriptor, in addition
to the existing native API, callback C fragment, full descriptor declarations,
prototype/layout assertions, pinned upstream header and license. The old example
C files are compatibility includes of the canonical package files. The package's
own `prepare.py` uses approved local dependencies; the old example helper forwards
to it. The legacy gallery has its own real, descriptor-checked native module.

`generate.py` now maintains C/descriptor/inventory parity and checks public API
names. It does not emit or rewrite VOID implementation bodies. Callback and owner
maintenance code no longer generates VOID fragments. Normal builds never run a
generator. The standalone package is self-contained; building an application still
requires the existing compiler and an independently approved native catalog.

## Minimal generic native extension

An approved conventional C descriptor can select
`source-file /src/raylib_native.void` rather than legacy `source /file.void`.
The real module declares `native "org.voidlang.raylib"`. This generic provider
marker does not authorize native code. Admission still requires the existing
native descriptor/catalog and exact dynamic closure.

The frontend reads physical source through the existing project/package capture,
overlays, source identities and lock verification. It checks provider/path,
complete function/record/resource inventory, sealed resources, exact layouts and
all signatures, including unused functions. Native declaration bodies must be
`pass`; executable initializers/bodies, default arguments, generics, extra or
duplicate declarations, and multiple matching physical modules reject. The
admission is contextualized to the existing locked package source identity.

The existing typed IR, checked C transport, loader, ABI 3/revision 4, G5 ownership,
generation retention and retirement are reused. There is no raylib-specific code
or name in the compiler/Runtime and no new source registry. Legacy virtual-source
descriptors retain their previous behavior. The explicit maintenance command
`voidc native-bind descriptor header.h -o NEW.void` can export initial declarations
and refuses an existing destination; it is not part of normal compilation.
A final explicit-export check reproduced the checked-in native declaration bytes,
then rejected export over a deliberately edited file and preserved that file.
`build/g24-physical/handoff/preservation.json` records this and byte equality of
the moved implementation/native inputs against baseline.

The new compiler capability is required to use this converted package. A compiler
built only from baseline `6a7cb50b` cannot consume `source-file` declarations. This
does not change the native ABI number or pinned LLVM/Clang/LLD 23.1.4.

## Editable bytes, locks and failure behavior

`tests/project/raylib_physical.py` uses a disposable local path dependency. The
unmodified wrapper returns 41. Editing the existing `get_random_value` wrapper to
add one makes ordinary normal/O2 JIT and native AOT return 42, after explicit
local lock refresh. Adding an ordinary wrapper returns 43. No generator runs.
The stale local lock rejects before refresh. Separate immutable Git consumers
reject changed fetched source, wrong revision, stale lock and changed native
provider. Source edits do not bypass dependency integrity.

The physical-source fixture also proves rejection of a changed native return
type, record layout, executable declaration body, provider identity and missing
native admission. An independent `org.voidlang.mixed` fixture exercises the same
physical mode through normal/O2 JIT, AOT, relocation and closed-owner failure,
alongside unchanged legacy C ABI/G5 fixtures.

## Native qualification

Evidence is retained under `build/g24-physical/` and
`/home/shmellyorc/VM/g24-physical-20261113/`; machine receipts identify the
disposable origins and exact source/native lock identities. These are local smart
HTTPS Git origins, not public GitHub results.

All toolchains are pinned to LLVM/Clang/LLD 23.1.4. Linux is the established
PikaOS x64 host. The Windows VM's name is “Windows 11”, but the native qualified
OS is Windows 10 build 19045. Native Windows commands run from the VS 2022 x64
Native Tools environment (`VSCMD_VER=17.14.42`, host/target x64, VCTools
14.44.35207, Windows SDK 10.0.19041.0). Native Clang and LLD version output is
retained with the Windows identity receipt.

| Configuration | Focused regressions | Physical source / 600 symbols / callbacks | Root Git / JIT / AOT / D5 | Additional native devices |
|---|---:|---|---|---|
| Linux Debug | 13/13 | PASS | PASS, normal and O2 | Real JIT graphics/input; real callback audio PASS |
| Linux Release | 13/13 | PASS | PASS, normal and O2 | Real relocated D5 graphics/input; real callback audio PASS |
| Linux strict | 13/13 | PASS, fatal ASan/UBSan/leak detection | PASS, normal and O2 | Bounded real callback audio PASS; full-device leak findings retained |
| Windows Debug | 9/9 configured native suites | PASS | PASS, normal and O2 | Real JIT and relocated D5 graphics/input; full audio PASS |
| Windows Release | 9/9 configured native suites | PASS | PASS, normal and O2 | Real JIT and relocated D5 graphics/input; full audio PASS |

The independent mixed-provider **physical** fixture additionally passes normal/O2
JIT, AOT, relocation and checked closed-owner failure on all five configurations.
Linux retains all 13 configured focused suites; Windows retains its nine
applicable configured suites (the regex is not counted as 13 Windows tests).
The legacy gallery/native preparation compatibility fixture passes on all three
Linux builds. Package layout, native C prototype/layout parity and all 600 native
symbols pass on both operating systems. This does not claim individual runtime
invocation of every upstream function.

Native VS Code 1.141.0 Debug/Release results are recorded separately from CLI:

| Native editor build | Raylib checks | G23 project checks |
|---|---:|---:|
| Linux Debug / Release | 7/7 each | 37/37 each |
| Windows Debug / Release | 7/7 each | 37/37 each |

Raylib checks cover record hover, real public/native definition paths, callback
hover, pinned package completion, no source errors, Check success and native
Package consent refusal. Receipts are
`build/g24-physical/qualified-editor-final-{debug,release}.json` and
`/home/shmellyorc/VM/g24-physical-20261113/{Debug,Release}-editor-final.json`.
The pinned editor's background service/tokenization warnings remain in raw logs;
they did not fail these extension-host assertions or process exit status.

The G23 Check/Build/Run/Package fixture passes **37 checks each** on Linux and
Windows Debug/Release. It includes dirty-buffer refusal, exact profile/arguments,
compiler failure diagnostics/recovery, successful Package Project and compiler
verification of its ordinary VOID application package. No production extension
behavior changed. For the raylib native consumer, Check succeeds; Package Project
preserves the existing compiler refusal without explicit `--allow-native`
(exit 2). Native packages are successfully created/verified/relocated by the
explicitly authorized CLI fixture. This is an existing editor consent boundary,
not a new successful native Package Project flow.

Exact disposable source pins and combined source/native lock SHA-256 values:

| Build | Fixture Git commit | `void.lock` SHA-256 |
|---|---|---|
| Linux debug | `b39c1d0d4408e12a1a76bb430f17b346375c2ade` | `ddb3d0c09025126ca0d5cf764075495ec358bbacc0df15bed3a47e86527303d6` |
| Linux release | `019f4d13b9a9080329a8c66919b7d3e547527f80` | `f6ccbfcc39cba24af187e237cd6a2dcef3a580888c16b62737a61fbf20a9e051` |
| Linux strict | `1bb5c71664015761f0f0eeea96d951454205e6c0` | `7ed1b93cf99174f44fc20399cd0ea06162257b9963a05e329ee69a13467e7d75` |
| Windows Debug | `3cbad27daa6e7f253727c03e791807e589cce897` | `a8073206c79cfba594526a68c5c1b7feed17c6c5ac3047a02e88b6c3b154b5cc` |
| Windows Release | `05f0bf0482145795f1094e371bba5a3a99647e89` | `14c62c8c4895dc4bea63aa227ee82531f47538bf50dd29a5b68c1cbebf1736f8` |

All five locks select source ID/version `org.voidlang.raylib`/`0.1.0`, no subdir,
and physical library manifest SHA-256
`1cb023ad9c7fab8e474143a75be2b4e55d4f11b488297f40ca08c56f68601bb1`.
Native ID/version/ABI/revision are `org.voidlang.raylib`/`6.0.0`/`3`/
`raylib-6.0-api-v4-physical`.
The exact descriptor identities are Linux
`76248:d403e6ea247eaf161d73ccb095512325489cdad36b84ba5d93f4c674c3e4e936`
and Windows
`76246:fffcf79020a9c3e6a6e23e209630027815fb2a7aa2643e7a1da8625d4acbb56a`.
Linux upstream runtime SHA-256 is
`1041653dd5c1cb8c67494fa398a296520d3ffec20cf1bf71b1eaa4b96ac61aee`;
Windows runtime/import library SHA-256 values are
`0dbf11a538786b6d35ea521bc86962ad8bd1c9bde0e4f23165fbdfa4dd1f2868` and
`59778ec137ffebe87a6c099944330f20b7eecdf1d49a5a93de7000a8992339df`.

Full source tree hashes, fixture origins, compiled adapter/runtime/link/notices
identities and exact locks are retained in
`build/g24-physical/handoff/linux-identities.json`,
`windows-identities.json` and the accompanying five `*void.lock` files. Debug,
Release and strict native adapter identities are recorded independently. All
executed package public/native sources, native C/descriptor inputs, manifest and
600-entry inventory match final worktree bytes on both systems. Later
native-only maintenance cleanup and documentation changes preserve generated C
parity; final `generate.py --check --require-complete` passes.



The callback fixture invokes all ten interfaces, tests replacement/unregistration,
captured state and GC, implicit shutdown, failed installation/cleanup, G5 reload
and retained generations, and 1,000 mixer replacement/detach cycles. Normal/O2
JIT/AOT and real audio are exercised. No callback framework or adapter lifecycle
was changed by this source conversion.

The Git consumer uses the package manifest at the repository root, without
`subdir`. Existing `deps fetch`, resolve/explain/locked check, normal/O2 JIT,
native AOT, D5 creation/verification/relocation and tamper rejection run through
the existing D3/D4/D5 paths. The source package never acquires native approval by
itself. Compiler-native and package/native lock identities remain distinct.

## Strict leaks remain external, unsuppressed and unfixed

The [independent native C controls and attribution](G23_G24_IMPLEMENTATION.md#strict-leak-classification)
remain retained: 256 bytes in Gallium after window shutdown, 408 bytes in pinned
raylib's WAV music context and 64 bytes in its sound-alias converter residual.
The expanded strict device paths reached cleanup and then failed LeakSanitizer.
They are not clean. No sanitizer was disabled and no suppression was added.

Current strict headless Git/JIT/AOT/D5, physical-source mutation and bounded real
audio callback tests pass with fatal ASan/UBSan and leak detection enabled. These
boundaries verify VOID/G5/adapter ownership and do not erase the separate leaking
music, alias and graphics paths. Windows real-device and Linux non-strict graphics
results are functional qualification, not strict leak clearance.

## Standalone Git publication — proposal only

The full VOID.Language public origin exceeds existing whole-repository D3 limits;
the actual failure at baseline is retained in the
[public-origin report](G24_PUBLIC_GITHUB_QUALIFICATION.md). No limit or transport
implementation was changed. Publish only the canonical package subtree, using
existing `git subtree split`. There is no second development copy to maintain.
The standalone root layout is:

```text
.gitattributes, .gitignore
void.project.toml
README.md, OWNERSHIP.md, inventory.json
generate.py, owned_bindings.py, callback_bindings.py, prepare.py
src/raylib.void
src/raylib_native.void
native/raylib.descriptor.in
native/raylib_api.descriptor.inc
native/raylib_provider.c, native/raylib_provider.h
native/raylib_api.c, native/raylib_api.h
native/raylib_callbacks.inc, native/raylib_parity.h
upstream/raylib.h, upstream/LICENSE
```

A disposable repository proof split the package twice to the same commit and
verified exact equality of its root tree and the canonical subtree. That fixture
had 22 files/1,703,396 blob bytes, far below D3's 8,192-entry/64 MiB source limits.
The final working source has 22 files/1,701,773 blob bytes.
Its test split `b170c90eb4ffcfa047024cba25ad95347f08c4d8` is not a publication
commit. The final integration and standalone commits do not exist yet.

After Boss integrates the physical conversion and authorizes creation/publication
of an empty repository, the proposed exact commands are:

```bash
cd /home/shmellyorc/Documents/Void.Language
# Set to Boss's actual integrated physical-conversion commit, not baseline 6a7cb50b.
VOID_INTEGRATION_SHA='<full integrated conversion SHA>'
# Proposed repository name; Boss must authorize and create/choose it first.
PACKAGE_ORIGIN='https://github.com/Shmellyorc/VOID.raylib.git'
git cat-file -e "$VOID_INTEGRATION_SHA^{commit}"
PACKAGE_SHA=$(git subtree split --prefix=verified-packages/raylib "$VOID_INTEGRATION_SHA")
git diff --exit-code "$VOID_INTEGRATION_SHA:verified-packages/raylib" "$PACKAGE_SHA^{tree}"
git ls-tree -r --name-only "$PACKAGE_SHA"
git show "$PACKAGE_SHA:void.project.toml"
git push "$PACKAGE_ORIGIN" "$PACKAGE_SHA:refs/heads/main"
git ls-remote "$PACKAGE_ORIGIN" refs/heads/main
```

These commands are a proposal, not an executed authorization request. Do not use
`--rejoin`, force-push, copy generated artifacts or push the monorepo branch.
Repeated splits of the same integrated commit/prefix are reproducible. Future
updates are integrated in the canonical directory, split with the same prefix,
and pushed normally without force. Native binaries/catalogs remain independently
approved, outside the source repository. Package-local ignore rules keep native
preparation artifacts out of the source layout.

The exact consumer manifest generated by existing
`examples/raylib-consumer/configure.py --standalone` is below. Replace both
placeholders with the authorized URL and actual full split commit. There is no
`subdir` because the library manifest is at the standalone root.

```toml
schema = 1
[project]
id = "org.voidlang.examples.raylib-consumer.project"
name = "Raylib package consumer"
version = "0.1.0"
kind = "application"
[application]
id = "org.voidlang.examples.raylib-consumer"
[sources]
roots = ["src"]
[build.debug]
optimization = "none"
[build.release]
optimization = "O2"
[dependencies.raylib]
id = "org.voidlang.raylib"
git = "https://github.com/Shmellyorc/VOID.raylib.git"
rev = "<actual full lowercase standalone package commit SHA>"
version = "=0.1.0"
[native-requirements.raylib]
id = "org.voidlang.raylib"
version = "=6.0.0"
abi = 3
```

After authorized publication, copy the existing consumer fixture to a fresh
directory outside the source repository and run:

```bash
python3 configure.py --origin "$PACKAGE_ORIGIN" --rev "$PACKAGE_SHA" --standalone
voidc deps fetch --project .
voidc resolve --project .
voidc deps explain --project . --json
voidc resolve --locked --project .
voidc check --project .
voidc run --project . --allow-native --profile debug
voidc run --project . --allow-native --profile release
voidc build --project . --allow-native --profile release -o /new/output/app
/new/output/app
voidc package --project . --allow-native --profile release -o /new/package
voidc package --verify /new/package
mv /new/package '/new/relocated package'
'/new/relocated package/app'
```

Set `VOID_NATIVE_CATALOG` to the independently verified prepared catalog first.
Use the updated compiler and pinned toolchain; on Windows use `python`, `.exe`
outputs, native paths and the VS 2022 x64 Native Tools shell. Repeat all five
native profiles and native VS Code 1.141.0 completion/Check/Package qualification
against the actual public immutable split commit. Preserve any real GitHub
transport, source, native lock, application package or editor failure. Until that
authorized push and replay, public standalone GitHub qualification is pending.

## Exact worktree inventory and checks

Relative to baseline HEAD there are 40 changed paths
(28 tracked modifications/deletions and 12 untracked additions).
The pre-existing public-origin report, milestone edits and missing-snapshot editor
observation branch were preserved and extended only where this conversion requires.
Git reports the public source move as a deletion plus untracked addition until Boss
stages it. The exact inventory is:

```text
 M docs/NATIVE_LIBRARIES.md
 M docs/roadmap/MILESTONE_REGISTER.md
 M editors/vscode/src/test/raylibHost.ts
 M examples/raylib-consumer/configure.py
 M examples/raylib/native/raylib.descriptor.in
 M examples/raylib/native/raylib_provider.c
 M examples/raylib/native/raylib_provider.h
 M examples/raylib/prepare.py
 M examples/raylib/src/gallery.void
 M include/void/c_primitive.hpp
 M src/dynamic_closure.cpp
 M src/frontend/compiler.cpp
 M src/frontend/project_selection.inc
 M src/ir/c_primitive.cpp
 M src/main.cpp
 M tests/project/managed_raylib.py
 M tests/project/mixed_native.py
 M tests/project/raylib_callbacks.py
 M tests/project/raylib_guards.py
 M tests/project/verified_raylib.py
 M verified-packages/raylib/OWNERSHIP.md
 M verified-packages/raylib/README.md
 M verified-packages/raylib/callback_bindings.py
 M verified-packages/raylib/generate.py
 M verified-packages/raylib/owned_bindings.py
 D verified-packages/raylib/src/api.void
 D verified-packages/raylib/src/callbacks.void.inc
 M verified-packages/raylib/void.project.toml
?? docs/projects/G24_PHYSICAL_SOURCE_IMPLEMENTATION.md
?? docs/projects/G24_PUBLIC_GITHUB_QUALIFICATION.md
?? examples/raylib/src/gallery_native.void
?? tests/project/raylib_physical.py
?? verified-packages/raylib/.gitattributes
?? verified-packages/raylib/.gitignore
?? verified-packages/raylib/native/raylib.descriptor.in
?? verified-packages/raylib/native/raylib_provider.c
?? verified-packages/raylib/native/raylib_provider.h
?? verified-packages/raylib/prepare.py
?? verified-packages/raylib/src/raylib.void
?? verified-packages/raylib/src/raylib_native.void
```

`git diff --check` passes. HEAD remains
`6a7cb50b2d35023324053694275596c9a4a7b328`; `git diff --cached --quiet` passes
(the index has no staged changes). Untracked source whitespace is checked
separately in `build/g24-physical/handoff/receipt.json`. No D3 acquisition implementation/limits,
D4 catalog resolver, D5 implementation, Runtime, backend or production VS Code
extension file is changed by this conversion. Historical uncommitted qualification
work remains present. Disposable fixture repositories have test commits only;
there is no integration/publication commit or push.
