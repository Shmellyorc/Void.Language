# LLVM 23.1.4 compatibility

LLVM/Clang/LLD 23.1.4 is the migrated development and native-build toolchain.
Consolidated local acceptance is tracked separately. Official source authority:
[LLVM 23.1.4 release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.4).
The complete development archives are required, not compiler-only installers.

No changes to VOID language syntax, source/VFS identities, Runtime ABI/GC,
readonly/ownership, generation leases or platform path protections are introduced.
Shared pre/post `verifyModule` and standard O2 remain required.

Native platform verification must distinguish local Linux from unexecuted native
Windows and macOS. The seven-job workflow matrix is preserved without dispatch.
Historical LLVM 19 reports and unresolved incidents remain historical evidence.

## Preserved architecture and API adaptations

| Boundary | LLVM 23 migration | Verification state |
|---|---|---|
| Target identity | Explicit `llvm::Triple` at registry, machine and module APIs | Release compiler/server build and focused JIT/AOT pass |
| String globals | `CreateGlobalString`, retaining NUL termination and opaque-pointer address | Full Debug/Release/strict source/JIT/AOT coverage passes after documented focused corrections |
| Intrinsics | `getOrInsertDeclaration` with existing overload types | Numeric/math normal/O2 JIT and linked AOT pass in all local profiles |
| Optimizer | Same no-TM PassBuilder recipe and standard O2, both verifiers retained | Actual pipeline captured; DSE remains enabled |
| Codegen/ORC | Default target codegen, existing LLJIT/resource trackers/leases | Full lifetime/GC/async/callback/reload selections pass in all local profiles |
| Persistent objects | Existing identity includes LLVM version and library bytes | Distinct 19/23 cold/hit objects verified; existing format and identity retained |
| Native build | Manifest producer/consumer pinned to 23.1.4; same staging/publication | Linux Unicode/percent/deep-path suites pass; native Windows pending |
| Optional machine research | MC register/subtarget reference APIs, TargetMachine consolidation, optional alignment and iterator insertion | Four source syntax checks and all 13 Debug native research cases pass |
| C++ ABI | SDK `LLVM_ENABLE_RTTI=OFF`, `LLVM_ENABLE_EH=OFF`, CXX11 libstdc++ ABI 1; project exceptions retained | CMake linked ORC probe and consolidated local profile selections pass |
| Apple cancellation | libc++ 23 exposes standard stop-token support without libc++19 opt-in; actual compile/link probe remains mandatory | Pinned source inspected; native macOS execution pending |
| Windows ABI/DIA | Same dynamic release CRT, iterator level zero and real dependency resolution | No local native Windows execution claimed |
| SDK supply chain | Official platform SHA-256 before extraction; cached provenance identity | Linux actual hash/install, cache and corrupt-download checks pass |

Official versioned API references:
[Module](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/include/llvm/IR/Module.h),
[IRBuilder](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/include/llvm/IR/IRBuilder.h),
[Intrinsics](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/include/llvm/IR/Intrinsics.h),
[TargetRegistry](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/include/llvm/MC/TargetRegistry.h),
[TargetMachine](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/include/llvm/Target/TargetMachine.h),
[PassBuilder](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/llvm/include/llvm/Passes/PassBuilder.h),
[libc++ stop token](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/libcxx/include/__stop_token/stop_token.h).

The official SDK's native ELF member observation describes an inspected member,
not a claim that every SDK archive member or platform was exhaustively classified.
No LLVM23-to-LLVM19 cross-loading of cached native objects is authorized.

## ABI, native execution and cache invariants

The Release, Debug/research and strict builds use distinct fresh directories.
Their CMake ORC probes actually link and run; their recorded compiler/link commands
reference only the new SDK. The SDK ships static no-RTTI/no-EH libraries built with
libstdc++ CXX11 ABI 1. VOID retains C++20, project exception handling and matching
no-RTTI across SDK-facing targets. The no-RTTI SDK cannot supply UBSan vptr coverage;
this is explicit and unchanged, not replaced by a mismatched RTTI mode. Strict
acceptance requires ASan, ordinary UBSan and leak checking to halt on findings.

The native manifest is deliberately version-incompatible with LLVM19. A controlled
private CLI copy rejects the old header while preserving a pre-existing destination.
Existing cache build identity includes compiler flags, LLVM version and linked
compiler/LLVM library contents. Cross-version cold/hit experiments show distinct
objects, preserving old bytes and allowing each version to hit its own object.
The cache format, source/VFS identity and publication machinery are unchanged.

No Windows workspace/staging/security helper is changed. The existing private
Windows link workspace, checked transfer to output-adjacent staging, same-parent
atomic publication, Unicode/% handling, exact case and ownership checks remain.
Linux focused application workflows and independently linked AOT execute locally.
The existing seven hosted jobs and native-only checks remain configured; neither
Linux execution nor a Linux-hosted LLD flavor version check establishes native
Windows/macOS behavior. macOS still uses the existing system linker selection
when no explicit ELF LLD is configured; its matching SDK ld64.lld is version-checked.

## Optional conservative storage proof

LLVM23's stock legacy codegen explicitly installs target-aware library analyses.
The optional manual emitters now install those same analyses. Production codegen
and ORC use stock interfaces that already do so. This is distinct from the shared
no-target-machine IR O2 recipe, which remains byte-identical to the baseline.

One managed O2 fixture now creates four cloned homes under LLVM23's inliner.
The conservative research certificate cannot prove their original ownership and
still fails closed. Its exact negative expectations now require those four
rejections and unchanged destination bytes; no guessed lineage, subtraction
credit, disabled inlining or permissive success/rejection assertion is allowed.
This records a narrower successful certification domain, not loss of valid source
execution support. Normal managed certification, both handoff profiles and
independently linked execution remain separate required checks. Golden Wrench
should review this changed research expectation with the retained reports.

## Tooling and distribution limits

Private native-server archives audit host dependencies, retain the LLVM notice and
complete an extracted-server protocol handshake without SDK PATH. The Linux VSIX
is tested through actual isolated installation and host startup. These artifacts
are owner-local acceptance outputs, not a portable/public release. The host's newer
libstdc++ requirements are listed in the dependency audit. VS Code 1.143.0, Vim 9.2
and Neovim 0.13 prerelease are the actual local versions; no VSCodium/native-target
pass or official minimum-version claim is inferred from those local executions.

The installer accepts only real published full SDK assets for the supported
Windows x64/Linux x64/macOS ARM64 matrix. It hashes before extraction and requires
a matching provenance receipt on cache reuse. A missing/mismatched receipt or bad
archive is rejected. The original LLVM19 SDK is retained separately.

## Observed artifact-size cost

The final local Release `voidc` grows from 54,614,824 to 69,872,600 bytes
(**+27.94%**), and `voidls` from 6,632,960 to 7,647,808 bytes (**+15.30%**).
The compiler/Runtime static archives themselves are slightly smaller. These
compare the preserved and migrated builds as configured; they do not isolate an
LLVM version, native-versus-bitcode SDK packaging or Clang codegen contribution.
No extra stripping, altered ABI or size optimization is introduced to mask the
observed distribution cost. Exact sizes are in `evidence/artifact-sizes.json`.

The protected root README is intentionally unchanged and still contains historical
LLVM19 setup examples. Use the updated [compiler build guide](../SOURCE_COMPILER.md)
and [migration commands](IMPLEMENTATION.md#local-build-reproduction) for the active
SDK. Updating the protected README remains an owner action. Dated proof/benchmark
records retain their original version labels and are not current setup instructions.

Local ELF version inspection finds SDK Clang/LLD/llvm-jitlink require at most
`GLIBC_2.34`, `GLIBCXX_3.4.30` and `CXXABI_1.3.13` among their versioned imports.
This is a binary dependency observation, not execution on every supported Linux
runner. The local compiler artifacts use this host's newer libc/libstdc++:
both preserved LLVM19 and migrated LLVM23 `voidc` require `GLIBC_2.44` and
`GLIBCXX_3.4.35`; the local `voidls` requires `GLIBC_2.38`/`GLIBCXX_3.4.35`.
Thus these private binaries are not portable Ubuntu24 packages. Build natively
on the intended deployment baseline before any owner-authorized distribution.
Exact host identity and imports are in `local-platform.json` and
`sdk-elf-runtime-versions.json`. No host library upgrade or ABI substitution was
performed for this migration.
