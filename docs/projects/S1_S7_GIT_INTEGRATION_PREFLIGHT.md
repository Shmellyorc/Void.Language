# S1–S7 Git integration preflight

**Golden Wrench accepted Slice 7. This preflight is ready for Boss review.**

## Commit proposal

Use one integrated commit after authorization. S1–S7 share files across the frontend, Runtime, CLI, editor tooling and CMake. The accepted qualification covers their combined tree. One commit preserves those qualified bytes and gives the integration a single review and recovery point.

Suggested title

```text
Integrate S1–S7 compiler, project tooling, and native FFI
```

Stage exactly the 198 paths listed below. This is the accepted manifest with its paths grouped by role. It contains 82 tracked modifications and 116 additions, with zero deletions or renames. The manifest SHA-256 is `e53aed35cea448a77c242fdf3ccc76fc1e8a1fc73073929f169ff8f11e155fad`. [Accepted integration manifest](evidence/slice7-final-link-repair/integration-paths.txt)

| Classification | Paths |
| --- | ---: |
| Production source | 62 |
| Build configuration | 3 |
| Standard library | 1 |
| Vendored build input | 5 |
| Test | 91 |
| Example | 6 |
| Public documentation | 21 |
| Essential integration report | 9 |
| **Total** | **198** |

The three build configuration paths include the VS Code extension manifest. CMake files under `tests/ffi/` are test inputs. The vendored class includes all five toml++ files because the patched header, upstream patch, provenance, license and reconstruction notes travel together. The nine gate and closeout documents are essential integration reports. This preflight is a separate review handoff.

## S1–S6 preservation

The proposed set includes all 111 product and documentation paths from the pre–Slice 7 checkpoint. The other 1,004 paths at that checkpoint are reports or evidence. The prior reverse/replay audit covered 86 distinct Slice 7 implementation paths across nine deltas. Its reverse pass recovered 17 exact S1–S6 working-tree postimages and four original HEAD files. Its forward pass reproduced all 86 Slice 7 postimages, including the 65 new implementation and fixture files.

This preflight checked the nine delta hashes and file sets, the 17 checkpoint hashes, and the four HEAD blob hashes. The other 1,097 checkpoint hashes match the current working tree directly. The checkpoint inventory has one self row. All 86 replay paths appear in the 198-path manifest. The final link repair adds the nine legacy test drivers to the earlier 189-path manifest. Its reverse application check passed in check-only mode. The 189 proposed paths outside the nine reports match the final qualification source receipt. The retained 3,440-entry source packet also matches its recorded hash. [S1–S6 checkpoint](evidence/slice7-inventory.txt) · [Reverse/replay receipt](evidence/slice7-final-integrated/layer-roundtrip.json) · [Final source receipt](evidence/slice7-final-link-repair/test-source-inputs.json) · [Link repair patch](evidence/slice7-final-link-repair/source.patch)

## Build inputs and redistribution

- **Native link inputs.** The nine Linux CTest recipes append `VOID_HOST_LINK_FILES` at registration. All 17 direct link calls place the resulting archive list after `libvoid_runtime.a` and `libvoid_native_fixture.a`. The recorded Release, Debug and strict commands use the profile's pinned `libarchive.a` followed by `libsodium.a`. Both archive hashes match the final qualification in each profile. The non-Linux list is empty. The existing Windows Release and Debug qualification remains applicable to those identical arguments. [Host dependency contract](../../cmake/HostDependencies.cmake) · [Exact recipes and qualification](SLICE7_FINAL_CLOSEOUT.md#final-integration-repair)
- **Baseline inputs.** HEAD already carries the root MIT license, licensing scope, native and host dependency CMake modules, host and HTTP pin files, provider source patches, setup and verification tools, SDK selector, and editor npm lockfile. The two curl configure-time corrections under `docs/net/http/evidence/` are already tracked inputs. Keep them in the source closure. LLVM/Clang/LLD 23.1.4 and the verified native dependency prefixes are external prerequisites. Provider attestation binds the local SDK path as well as its hash. A new checkout needs its own correctly provisioned or attested inputs. [Build guide](../SOURCE_COMPILER.md#building-the-compiler) · [Qualified dependency inputs](evidence/slice7-final-integrated/dependency-inputs.json)
- **toml++ 3.4.0.** Include all five vendor paths and the modified install rules, notice inventory and redistribution text. Both compiler and tooling install the upstream MIT license, exact patch, provenance and corresponding source. The five notice inventory entries match the local files. The offline provenance check recovered the pinned upstream header hash in memory. [Vendor reconstruction](../../third_party/tomlplusplus/README.md) · [Redistribution notices](../release/REDISTRIBUTION_NOTICES.md)
- **Raylib 6.0.** The six sample paths and the integration report carry the project, provider, descriptor template and preparation instructions. `prepare.py` takes the local header, shared library and upstream license, plus the matching Windows import library when applicable. Keep `RAYLIB-LICENSE.txt` with every gallery package that ships the vendor library. Keep the upstream copyright and license intact and mark altered upstream source as the license directs. The retained Linux header, library and license and the three original vendor archives match the recorded hashes. Windows package identity comes from the retained qualified receipts. [Sample and packaging](SLICE7_RAYLIB_INTEGRATION.md#build-and-run-the-sample) · [Vendor hashes and license](SLICE7_RAYLIB_INTEGRATION.md#raylib-60-and-runtime-dependencies)
- **Release boundary.** Preserve the applicable LLVM, HTTP and host-provider notices and source materials already recorded for each distributed component. The existing redistribution report records unresolved GNU replacement and corresponding-source procedures, SDK conditions and the public-release hold. This is a source integration proposal for review. [Release scope and open items](../release/REDISTRIBUTION_NOTICES.md)

All 198 candidates are regular text files. There are no SDK or Runtime binaries, binary archives, screenshots, caches, generated native adapters, build output or standalone logs in the proposal. The vendored TOML amalgamation and the notice inventory are retained source and provenance inputs. No candidate collides with another path under case-insensitive comparison against HEAD.

The path scan found example paths and intentional test literals, including Windows drive and manifest-rejection cases. The two host-specific Windows locations are historical receipt paths in `SLICE7_FINAL_CLOSEOUT.md` and `SLICE7_G4_CONVENTIONAL_FFI.md`. Build examples use placeholders or checkout-relative variables. The accepted build and production code contains no hardcoded location for this checkout, the Windows qualification tree or temporary build trees. A targeted scan for private-key headers, common service-token formats and literal credential assignments returned zero matches.

## Explicit exclusions

Use the final layered inventory minus the exact 198 paths below as the exclusion list for the 2,949 inventoried working-tree changes. That set difference contains exactly 2,751 paths. It consists of 2,732 under `docs/projects/evidence/`, three under `docs/log/evidence/`, and the 16 standalone reports listed here. The 240-path review manifest remains separate. Its only intersection with the product list is `docs/projects/SLICE7_FINAL_CLOSEOUT.md`. [Exact layered inventory](evidence/slice7-final-link-repair/inventory.txt) · [Review evidence manifest](evidence/slice7-final-link-repair/review-evidence-paths.txt)

```text
docs/projects/C_CALLBACK_RESEARCH.md
docs/projects/SLICE1.md
docs/projects/SLICE1_CLOSEOUT.md
docs/projects/SLICE2.md
docs/projects/SLICE2_FINAL_CLOSEOUT.md
docs/projects/SLICE3.md
docs/projects/SLICE3_FINAL_CLOSEOUT.md
docs/projects/SLICE4.md
docs/projects/SLICE5.md
docs/projects/SLICE6.md
docs/projects/SLICE6_FINAL_CLOSEOUT.md
docs/projects/SLICE7_ARCHITECTURE_REVIEW.md
docs/projects/SLICE7_G4.md
docs/projects/SLICE7_G4_WINDOWS_ARCHITECTURE.md
docs/projects/SLICE7_G4_WINDOWS_FFI_RESEARCH.md
docs/projects/SLICE7_G4_WINDOWS_IMAGE_SELECTION.md
```

Keep this preflight, all ignored files, and all local qualification outputs outside the product commit. This includes `build/`, `.toolchains/`, `.vscode/`, `-.lib`, `nvim.log`, package outputs, provider binaries, source packets, dependency downloads, logs, screenshots and caches. Leave HEAD's tracked fixtures and dependency inputs in place. The nine proposed reports contain 296 distinct relative evidence targets or directories outside the proposed tree. All 296 resolve in the retained workspace. Review the reports with the retained evidence if reviewing from a fresh clone. [Ignored local inventory](evidence/slice7-final-integrated/ignored-local-inventory.txt)

## Read-only Git receipt

| Check | Result |
| --- | --- |
| Branch and HEAD | `main` at `2f667ea338127b8f91c3136804377ef3fa5218d6` |
| Index | 68,996 tuples, SHA-256 `c9b3a3ca23a36c7c11fac5839d50c9f3d07311f5ec61f8577182f1abb0715fc0` |
| Staged diff against HEAD | 0 bytes |
| Before this report | 82 unstaged tracked modifications, 2,867 untracked files |
| After this report | 82 unstaged tracked modifications, 2,868 untracked files |
| Tracked diff against HEAD | 82 files, 3,981 insertions and 607 deletions |
| `git diff --check HEAD --` | Pass |
| `git apply --reverse --check` on the link repair | Pass |
| Individual `git diff --no-index --check` for the 116 proposed additions | 115 clean. The pinned toml++ patch has 54 context-line whitespace diagnostics. |

Each toml++ diagnostic occurs on a unified-diff context line. The patch bytes match both provenance and installed notice records. Retain them exactly. The preflight matched all 2,949 existing status entries and all 2,948 recorded content hashes. The inventory's self row has no recorded self-hash. The preflight report is the only new path. Git commands used `--no-optional-locks` to keep the index untouched. [Accepted Git receipt](evidence/slice7-final-link-repair/git-state.txt)

## Recovery plan

1. Before authorized integration, take a hash-verified backup outside the working tree of the 198 proposed files and the separate review material. Record the current HEAD, the index checksum and the exact manifests. Retain the qualified source packet and build trees.
2. If staging stops before a commit, compare the index with the recorded baseline and identify the affected paths. After separate authorization, restore only those index entries from the recorded baseline. Leave the working-tree copies and all untracked artifacts in place.
3. If a commit needs investigation, retain its ID and inspect the recorded baseline in a separate worktree after authorization. Recover any damaged working-tree file from its verified accepted bytes. Keep the original workspace available for Golden Wrench and Boss.

## Exact proposed staging paths

### Production source (62)

```text
editors/vscode/src/config.ts
editors/vscode/src/creation.ts
editors/vscode/src/creationWorkflow.ts
editors/vscode/src/extension.ts
editors/vscode/syntaxes/void.tmLanguage.json
include/void/application_metadata.hpp
include/void/backend.hpp
include/void/c_dynamic.hpp
include/void/c_primitive.hpp
include/void/diagnostic.hpp
include/void/execution.hpp
include/void/frontend.hpp
include/void/ir.hpp
include/void/native_build.hpp
include/void/native_resource.hpp
include/void/numeric.hpp
include/void/os.hpp
include/void/path.hpp
include/void/project.hpp
include/void/project_creation.hpp
include/void/runtime.hpp
include/void/runtime_abi.hpp
include/void/tooling.hpp
src/application_metadata.cpp
src/backend/llvm/backend.cpp
src/dynamic_closure.cpp
src/frontend/compiler.cpp
src/frontend/construction.inc
src/frontend/generic_declarations.inc
src/frontend/generic_inference.inc
src/frontend/generic_tooling.inc
src/frontend/generics.inc
src/frontend/pattern_parser.inc
src/frontend/pattern_semantics.inc
src/frontend/project.cpp
src/frontend/project_creation.cpp
src/frontend/project_selection.inc
src/frontend/source.cpp
src/frontend/tooling.cpp
src/ir/c_primitive.cpp
src/ir/module.cpp
src/ir/numeric_enum_storage.inc
src/ir/os.inc
src/ir/reference_capabilities.inc
src/lsp/main.cpp
src/main.cpp
src/native_build.cpp
src/numeric.cpp
src/project_identity.cpp
src/runtime/abi.cpp
src/runtime/application.cpp
src/runtime/array.cpp
src/runtime/c_dynamic.cpp
src/runtime/c_resource.cpp
src/runtime/c_span.cpp
src/runtime/coordination.cpp
src/runtime/fixed_abi.cpp
src/runtime/native_resource.cpp
src/runtime/os.cpp
src/runtime/os_abi.cpp
src/runtime/reload.cpp
tools/lsp_client.py
```

### Build configuration (3)

```text
CMakeLists.txt
cmake/InstallLicenses.cmake
editors/vscode/package.json
```

### Standard library (1)

```text
stdlib/os.void
```

### Vendored build input (5)

```text
third_party/tomlplusplus/LICENSE
third_party/tomlplusplus/README.md
third_party/tomlplusplus/toml.hpp
third_party/tomlplusplus/upstream.json
third_party/tomlplusplus/void-bounds-and-locations.patch
```

### Test (91)

```text
editors/vscode/src/test/config.test.ts
editors/vscode/src/test/creationHost.ts
editors/vscode/src/test/creationWorkbench.ts
editors/vscode/src/test/numericHost.ts
editors/vscode/src/test/projectHost.ts
editors/vscode/src/test/runCreationHost.ts
editors/vscode/src/test/runNumericHost.ts
editors/vscode/src/test/runProjectHost.ts
tests/cli/install.py
tests/ffi/callback.descriptor.in
tests/ffi/callback.void
tests/ffi/callback_fixture.c
tests/ffi/callback_fixture.h
tests/ffi/callback_process.py
tests/ffi/callback_reload.cpp
tests/ffi/callback_reload.void
tests/ffi/conventional.descriptor.in
tests/ffi/conventional.void.in
tests/ffi/conventional_dependency.cpp
tests/ffi/conventional_fixture.c
tests/ffi/conventional_fixture.h
tests/ffi/conventional_process.py
tests/ffi/conventional_rejection.c
tests/ffi/dynamic.cmake
tests/ffi/dynamic.cpp
tests/ffi/dynamic.descriptor.in
tests/ffi/dynamic.void
tests/ffi/dynamic_constructor.c
tests/ffi/dynamic_dep.c
tests/ffi/dynamic_fixture.c
tests/ffi/dynamic_fixture.h
tests/ffi/dynamic_plan.py
tests/ffi/dynamic_process.py
tests/ffi/dynamic_startup.void
tests/ffi/dynamic_thunk.c
tests/ffi/g1_descriptor.h.in
tests/ffi/g2_descriptor.h.in
tests/ffi/g3_descriptor.h.in
tests/ffi/g4_descriptor.h.in
tests/ffi/missing.py
tests/ffi/primitives.cmake
tests/ffi/primitives.cpp
tests/ffi/primitives.descriptor.in
tests/ffi/primitives.void.in
tests/ffi/primitives_fixture.c
tests/ffi/primitives_thunk.c
tests/ffi/records.cmake
tests/ffi/records.cpp
tests/ffi/records.descriptor.in
tests/ffi/records.void
tests/ffi/records_fixture.c
tests/ffi/records_process.py
tests/ffi/records_thunk.c
tests/ffi/resources.cmake
tests/ffi/resources.cpp
tests/ffi/resources.descriptor.in
tests/ffi/resources.void
tests/ffi/resources_exit.cpp
tests/ffi/resources_fixture.c
tests/ffi/resources_process.py
tests/ffi/resources_thunk.c
tests/numeric/enum_storage.inc
tests/numeric/enums.py
tests/numeric/enums_ir.cpp
tests/numeric/enums_lsp.py
tests/numeric/tests.cpp
tests/os/frontend.py
tests/project/application.cpp
tests/project/application.py
tests/project/authority.py
tests/project/creation.cpp
tests/project/creation.py
tests/project/discovery.py
tests/project/header.cpp
tests/project/legacy.py
tests/project/manifest.cpp
tests/project/nested.py
tests/project/profiles.py
tests/project/protocol.py
tests/project/provenance.py
tests/project/selection.cpp
tests/project/sessions.cpp
tests/stage10/console.py
tests/stage10/conversion.py
tests/stage10/fields.py
tests/stage10/operators.py
tests/stage10/temporal.py
tests/stage11/globals.py
tests/stage8/sprint3.py
tests/stage8/sprint4.py
tests/stage8/sprint6.py
```

### Example (6)

```text
examples/raylib/native/raylib.descriptor.in
examples/raylib/native/raylib_provider.c
examples/raylib/native/raylib_provider.h
examples/raylib/prepare.py
examples/raylib/src/gallery.void
examples/raylib/void.project.toml
```

### Public documentation (21)

```text
docs/CLI.md
docs/DECISIONS.md
docs/DIAGNOSTICS.md
docs/EDITOR_INTEGRATION.md
docs/LANGUAGE_BIBLE.md
docs/LANGUAGE_SERVER.md
docs/LOCAL_LIBRARIES.md
docs/MODULE_SYSTEM.md
docs/NATIVE_LIBRARIES.md
docs/NUMERIC_TYPES.md
docs/PROJECT_ARCHITECTURE_ROADMAP.md
docs/README.md
docs/RUNTIME_GLOBALS.md
docs/SOURCE_COMPILER.md
docs/STAGE8_DISCOVERY.md
docs/STAGE9_APPLICATION_WORKFLOW.md
docs/TOOLING_SESSIONS.md
docs/os/API.md
docs/release/NOTICE_INVENTORY.json
docs/release/REDISTRIBUTION_NOTICES.md
editors/vscode/README.md
```

### Essential integration report (9)

```text
docs/projects/SLICE7_FINAL_CLOSEOUT.md
docs/projects/SLICE7_G1.md
docs/projects/SLICE7_G2.md
docs/projects/SLICE7_G3.md
docs/projects/SLICE7_G3_FINAL_CLOSEOUT.md
docs/projects/SLICE7_G4_CONVENTIONAL_FFI.md
docs/projects/SLICE7_G4_LINUX.md
docs/projects/SLICE7_G5_CALLBACKS.md
docs/projects/SLICE7_RAYLIB_INTEGRATION.md
```

**Preflight complete. Git mutation awaits Boss authorization.**
