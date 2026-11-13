# Source compiler and engineering build guide

The compiler-library [manifest parser](../include/void/project.hpp) and
`analyze_manifest_project(root, entry_override, sources, manifests)` in
[frontend.hpp](../include/void/frontend.hpp) provide owning immutable project
analysis, discovery/execution snapshots and entry selection. Applications use the
existing ValidatedAnalysis/IR/LLVM pipeline. Libraries check their source union
with a declarationless root and have no executable application entry.
`package.entry` selects a library's import surface; it does not fabricate main.

SourceGraph separates its ordinary Parser pass from import expansion. Discovery
units move into execution as needed, so unimported helpers are not semantically
checked, emitted or initialized. The manifest fixes the physical VFS root; the
selected entry fixes application declaration ownership. Disk membership, boundary
observations and bytes are recaptured before successful acceptance. This detects
changes and requests retry; it is not an OS filesystem transaction or sandbox.
Legacy entry-only APIs remain supported adapters through the same frontend.

Project sessions/protocol, embedded metadata and OS conveniences, init/new,
checked numeric enums, D3 dependency resolution, typed C ABI/native admission and
D5 packaging are implemented. G23 adds trusted editor packaging; G24 supplies
physical raylib source; G25 imports headers with pinned Clang; G26 supplies
add/tidy and installed source views. G27 connects generated library entries and
library consumers to that existing maintenance path. See the
[current milestone register](roadmap/MILESTONE_REGISTER.md),
[G27 qualification](projects/G27_LIBRARY_AUTHORING.md) and
[project ownership contract](PROJECT_ARCHITECTURE_ROADMAP.md#phase-1-ownership-and-data-flow).
No second parser, editor resolver, package mechanism or LLVM path is introduced.

Manifest applications select one valid synchronous main regardless of filename,
or report ambiguity. Manifestless directory selection retains exact `main.void`;
explicit-file compatibility remains available. Ordinary run/build async roots
still require an explicit host adapter; [async contracts](ASYNC.md) remain intact.
Dynamic native linking is the default; supported static linking is optional.
Library source acquisition grants no native-provider authority.

For onboarding, start with [source installation](install/README.md),
[your first program](FIRST_PROGRAM.md), [creating and composing libraries](LOCAL_LIBRARIES.md#creating-and-composing-libraries)
and the [CLI reference](CLI.md). The dated implementation records below retain
their original qualification limits; current feature guides supersede historical
exclusions. The [8 November roadmap](PROJECT_ARCHITECTURE_ROADMAP.md) remains design
history, not a list of unimplemented prerequisites.

## Stage 11 shared runtime globals

The frontend registers outermost variables separately from executable script
statements and resolves one canonical binding across root visibility and imports.
Ordinary typed initializer functions and explicit global metadata lower through
the shared IR validator and LLVM backend. Guarded Runtime cell lookup preserves
readiness, owner access, GC and value-place semantics. JIT and linked AOT initialize
before selected entry; checking and tooling never execute startup. See the
[global contract](RUNTIME_GLOBALS.md) and [engineering record](stage11/IMPLEMENTATION.md).


The [Stage 8 ergonomics contract](STAGE8_ERGONOMICS.md) and
[local acceptance](stage8/SPRINT6_ACCEPTANCE.md) describe the current bounded
has/if/multiline-call/string-interpolation extensions. Historical phase-only
exclusions below are superseded by those delivered contracts.

The current [Stage 8 application contract](STAGE8_APPLICATION_NATIVE.md) records
automatic application hosting and real native executable builds; see
[Stage 9](STAGE9_APPLICATION_WORKFLOW.md).

Current status/hosted evidence: [current implementation status](DEVELOPMENT_STRATEGY.md#current-implementation-status). Dated phase-only exclusions below are historical where superseded; this guide retains its feature contract.

Stage 3 extends this same pipeline with native `{K: V}` dictionaries; see DICTIONARIES.md for the implemented source/runtime contract, supported keys and deferred result-based get.

Phase 22 adds the owner-approved UTF-8 string foundation through this same pipeline.
See [STRINGS.md](STRINGS.md) for immutable content, escapes, empty/default templates,
managed/value storage, exact equality/concatenation, roots, failures, reload and
native copying boundaries. Future indexing/length/nullability/interpolation APIs
remain outside this implementation. WORK_LOG.md records actual local verification
and the pending seven-job hosted gate.

Phase 20 adds managed single inheritance, identity-preserving ancestor conversions, explicit virtual/override/super dispatch and staged base initialization. See [INHERITANCE.md](INHERITANCE.md) for the current boundary and WORK_LOG.md for actual verification.

Implemented locally on 10 October 2026, using LLVM 19.1.7. LANGUAGE_BIBLE.md
remains the language-design source of truth; this document describes the working
subset and explicitly provisional choices. Multi-file compilation is now implemented;
see [MODULE_SYSTEM.md](MODULE_SYSTEM.md) for current import and ownership rules.
Phase 6 updates the numeric subset on 11 October 2026; see NUMERIC_TYPES.md.
Phase 12 adds numeric/bool/nested value structs; see [VALUE_STRUCTS.md](VALUE_STRUCTS.md)
for approved defaults, source syntax, shared aggregate IR and private ABI boundaries.
Earlier foundation-only status in historical documents predates source compilation.

Phase 16 adds ordinary managed source structs, nullable references, deep readonly
views and runtime-backed JIT/linked AOT; see [MANAGED_STRUCTS.md](MANAGED_STRUCTS.md).
Phase 17 adds custom managed initialization with checked construction views,
required-reference readiness, safe helper contexts and complete-object commit; see
[MANAGED_INITIALIZATION.md](MANAGED_INITIALIZATION.md).

## Active toolchain

VOID.Language uses **LLVM/Clang/LLD 23.1.4 exclusively**. Use the complete SDK
from the [existing verified archive recipe](../tools/ci/llvm_sdk.py); no ambient
LLVM selection or older-version fallback is permitted. The platform prerequisites
and commands below remain authoritative. Historical results retain their actual
LLVM versions and cannot serve as current build recipes.

For Windows host providers, derive MSBuild's `LLVMToolsVersion` from the pinned
`clang-cl.exe /clang:-print-resource-dir` result. Its directory leaf can differ
from the full release version. Pass the verified `LLVMInstallDir` and explicit
compiler/linker/librarian paths together; preserve `/MT` for Debug and Release.
Missing or mismatched components fail before compilation. [The integration record](voidup/INTEGRATION_REPORT.md#llvm-2314-ci-repair)
separates the source correction from unverified native execution.

## Building the compiler

VOID requires the complete **LLVM/Clang/LLD 23.1.4** development SDK: headers,
LLVM libraries, CMake exports and matching host tools. CMake rejects other LLVM
versions. Python **3.12+** is required for dependency preparation; Python is also
used during ordinary configuration, even when tests are disabled. A C++20 host
standard library and platform development files are required in addition to Clang.

The accepted [Linux qualification](release/LINUX_CLEAN_HOST_QUALIFICATION.md)
built source `97aa4e4334f1b9354ba651770b5cf13e74da03d0` in fresh Ubuntu 24.04
x64 container userspace. The [Windows qualification](release/WINDOWS_X64_ISOLATED_INSTALL_QUALIFICATION.md)
installed existing Release artifacts from `1afd881ecfa65c616261e7aeb76ecd2bf87770b6`
on a developer VM. Neither establishes independent-machine portability.
[The release handoff](release/PREVIEW_QUALIFICATION_HANDOFF.md) tracks the next
clean-host procedure and remaining review gates. These are private source-install
instructions, not a public binary package or redistribution authorization.

The [root README](../README.md) now introduces the consolidated source-install
entry point. These explicit commands remain available for engineering. Historical
LLVM19 feature and acceptance notes below remain history.
See the [migration record](llvm23/IMPLEMENTATION.md) for that transition.

### Linux dependencies and build

Use Linux x86-64 with the complete official Linux x64 SDK. The verified Ubuntu
userspace used CMake 3.28.3, Ninja 1.11.1, Python 3.12.3, Make 4.3, Perl 5.38.2,
glibc 2.39 and GCC/libstdc++ development headers 13.3.0. These are observed
versions, not a proven minimum ABI matrix. The project CMake minimum is 3.24;
Ninja has no separate repository-enforced minimum.

Run from the physical source-checkout root. On Ubuntu 24.04, prepare the host:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build python3 libffi-dev libedit-dev \
  libxml2-dev libzstd-dev zlib1g-dev libtinfo-dev \
  ca-certificates xz-utils curl git perl
```

`build-essential` supplies Make, host C/C++ headers and startup/link resources;
Perl is used by AWS-LC assembly generation. LLVM exports need real development
libraries, including zlib, zstd and LibXml2; `libedit-dev` provides optional
line-editing support. Ubuntu supplies `libtinfo-dev` through `libncurses-dev`.
Other distributions may need different package names. Distribution CA files must
be available both for downloads and curl's configuration-time trust discovery.

Install the SDK at **`.toolchains/llvm-23.1.4` inside this checkout**. HTTP setup
uses that path directly, regardless of a later custom `LLVM_DIR`. The Linux asset
is `LLVM-23.1.4-Linux-X64.tar.xz`, SHA-256
`25d814fda50d5fb0084b42be09605d8b1595d49d5e2e03a3345d7e112ff6f9f3`.
The [repository SDK helper](../tools/ci/llvm_sdk.py) pins its official URL and hash.
For a new download outside GitHub Actions, its install function can be called
without the CI entrypoint's `GITHUB_ENV`/`GITHUB_PATH` assumptions:

```sh
python3 - <<'PY'
from pathlib import Path
import runpy
sdk_setup = runpy.run_path("tools/ci/llvm_sdk.py")
sdk_setup["install"](Path(".toolchains/llvm-23.1.4").resolve())
PY
void_llvm23_sdk="$PWD/.toolchains/llvm-23.1.4"
test -f "$void_llvm23_sdk/lib/cmake/llvm/LLVMConfig.cmake"
"$void_llvm23_sdk/bin/clang" --version
"$void_llvm23_sdk/bin/ld.lld" --version
"$void_llvm23_sdk/bin/llvm-config" --version --build-mode --assertion-mode --has-rtti
```

Require 23.1.4 from the selected tools; the official qualified archive reports
Release, assertions off and RTTI off. Retain `void-sdk-identity.json`. The helper
rejects an existing SDK without the expected identity receipt. The first Linux
qualification copied and rehashed the official archive rather than downloading it
anew. The standalone download invocation above is source-inspected; its fresh-
network execution remains part of the next clean-host test, not a new claimed pass.

The Runtime includes the accepted HTTP provider, so prepare its pinned native
dependencies **before** configuring VOID:

```sh
python3 tools/setup_http_deps.py --profile Release --jobs 3
python3 tools/verify_http_dependencies.py \
  .toolchains/http/prefix/linux-aws-lc-5.12.0/Release Release
python3 tools/setup_host_deps.py --profile Release --jobs 3
python3 tools/verify_host_deps.py --prefix .toolchains/host/prefix/Release \
  --profile Release --platform Linux --architecture x86_64
```

[The pin set](../third_party/http-versions.json) covers nghttp2, c-ares, zlib,
AWS-LC, libidn2, libpsl and libunistring. Curl and public-suffix data are checked-in
source. The native prefix defaults to
`.toolchains/http/prefix/linux-aws-lc-5.12.0/Release`; `VOID_HTTP_DEPS_ROOT` can
select another verified matching prefix. `CMAKE_PREFIX_PATH` alone does not select
it. Keep the manifest, library/header files and verified corresponding source
archives in `.toolchains/http/downloads`: the verifier checks those archives too.
An ambient system curl/TLS installation is not a replacement. Setup writes local
build receipts under `docs/net/http/evidence/`; retain them privately for a
qualification rather than treating them as publication artifacts.

Host providers use that verified HTTP zlib and require their matching receipt
before CMake configuration. Keep `.toolchains/host` source archives, approved
patch inputs and profile-specific prefixes. The same builders and verifiers are
used by `tools/install.py`; do not substitute ambient sodium/archive libraries.

The following production-only Release configuration matches the verified recipe,
with source/build paths made checkout-relative. It builds the compiler, Runtime,
required native libraries and language server; it does not claim a CTest pass:

```sh
cmake -S . -B build/preview-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DLLVM_DIR="$void_llvm23_sdk/lib/cmake/llvm" \
  -DCMAKE_C_COMPILER="$void_llvm23_sdk/bin/clang" \
  -DCMAKE_CXX_COMPILER="$void_llvm23_sdk/bin/clang++" \
  -DVOID_CLANG="$void_llvm23_sdk/bin/clang" \
  -DVOID_LINKER="$void_llvm23_sdk/bin/ld.lld" \
  -DVOID_DWARFDUMP="$void_llvm23_sdk/bin/llvm-dwarfdump" \
  -DCMAKE_EXE_LINKER_FLAGS="--ld-path=$void_llvm23_sdk/bin/ld.lld"
cmake --build build/preview-release --target voidc voidls --parallel 3
```

CMake verifies SDK version/RTTI, the HTTP prefix, CA locations and a real ORC
link-and-run probe. Missing dependencies must diagnose; do not replace imported
libraries with stubs. `VOID_DWARFDUMP` is test-only and is reported unused with
`BUILD_TESTING=OFF`, as in the qualification receipt.

### Install and first program (Linux)

Choose an unused private prefix. From the same checkout and shell:

```sh
void_install_prefix="$HOME/.local/opt/void-2-preview"
cmake --install build/preview-release --config Release \
  --component compiler --prefix "$void_install_prefix"
cmake --install build/preview-release --config Release \
  --component tooling --prefix "$void_install_prefix"
```

The compiler component installs `bin/voidc`, both native manifest files,
`lib/void` Runtime/provider libraries, standard-library sources and notices.
The tooling component installs `bin/voidls` and Vim/Neovim adapters; both components
install matching `.void` sources under `share/void/stdlib`. Editor applications
and a VSIX are separate. Installation does not install Clang, MSVC/platform SDKs
or grant a redistribution license; the standalone LLVM notice gap remains in the
[blocker register](release/PREVIEW_BLOCKERS.md).

For the smoke check, remove source/library overrides and select installed tools.
Use a new temporary project outside the checkout:

```sh
unset VOID_STDLIB_ROOT LD_LIBRARY_PATH
export PATH="$void_install_prefix/bin:/usr/bin:/bin"
command -v voidc
command -v voidls
void_smoke_dir="$(mktemp -d "${TMPDIR:-/tmp}/void-first-install.XXXXXXXX")"
cat > "$void_smoke_dir/main.void" <<'VOID'
func main() -> i32:
    return 0
VOID
voidc check "$void_smoke_dir/main.void"
voidc run "$void_smoke_dir/main.void"
```

`check` prints `Source checked successfully`; both commands exit 0. This `main`
prints nothing. Then replace this disposable project's file with an installed
standard-module program:

```sh
cat > "$void_smoke_dir/main.void" <<'VOID'
use console

func main() -> i32:
    print("VOID installed stdlib OK")
    return 0
VOID
voidc check "$void_smoke_dir/main.void"
voidc run "$void_smoke_dir/main.void"
voidc build "$void_smoke_dir/main.void" -o "$void_smoke_dir/hello-native"
"$void_smoke_dir/hello-native"
voidls --discover "$void_smoke_dir" "$void_smoke_dir/main.void" ""
```

Both executions print exactly `VOID installed stdlib OK` followed by LF and exit
0. Discovery selects that project's `main.void`; it is not a protocol handshake.
The accepted [LSP wire receipt](release/evidence/lsp-handshake.json) separately
records initialize/initialized/shutdown/exit and tooling protocol 1. A new
qualification must perform that real exchange too; compilation/discovery alone
does not establish server or editor usability. See the
[language-server contract](LANGUAGE_SERVER.md) and
[editor setup](EDITOR_INTEGRATION.md) for client configuration.

Installed standard sources are found relative to the executable. An absolute
`VOID_STDLIB_ROOT` override takes precedence; the configured build-source root is
the fallback when no installed tree is available. To prove installed-source use,
the accepted container test relocated its prefix and moved its original source
stdlib out of reach. Merely moving a checkout is not an independent-host test.

Native manifests keep absolute configured Clang/LLD paths and hash-verified
prefix-relative Runtime/provider resources. Keep the matching SDK and host
C++/CRT development files available. Linux `voidc` uses an executable-relative
library RUNPATH; generated programs retain an absolute RUNPATH to the install's
shared IDN/Unicode libraries. Moving that prefix after linking applications has
not been qualified. The complete recorded loader closure is in the
[Linux report](release/LINUX_CLEAN_HOST_QUALIFICATION.md#observed-portability-and-notice-limits).

### Native execution and linking

`run` uses the native ORC JIT. `build` emits and links a host executable using the
configured toolchain and Runtime/provider closure. `emit` writes only a native
object with source line information; it does not run a linker. Prefer `build`
for an executable, especially for standard modules and managed features.
`--no-opt` is available for run/build/emit; ordinary use defaults to O2.

The older architecture example uses the still-supported legacy
`entry(i32) -> i32` adapter, rather than application `main`:

```sh
build/preview-release/voidc check examples/architecture.void
build/preview-release/voidc run examples/architecture.void 10
# 52
build/preview-release/voidc build examples/architecture.void \
  -o build/preview-release/architecture
build/preview-release/architecture 10
# 52
```

Only that legacy adapter accepts one optional decimal i32, defaulting to zero,
and prints its i32 result while exiting successfully. Application `main()` and
top-level execution accept no numeric operand; their i32 result is the exit status.
`check` also accepts libraries without an entry. For manual object-linking
contracts see [managed structs](MANAGED_STRUCTS.md) and the feature guides; an
object alone is not the full Runtime/provider link closure.

The older `--jit-proof` and `--emit-proof OBJECT` options are hand-built IR
architecture harnesses, not source compilation or source-install qualification.

### Windows developer-host requirements

**Accepted: isolated installation of existing Release artifacts on the named
Windows developer VM. Open: fresh Windows source build and independent clean-host
installation.** The [unaltered Windows-authored record](release/WINDOWS_X64_ISOLATED_INSTALL_QUALIFICATION.md)
is authoritative for its tested SHA, paths, hashes, controls and limitations.
It tested `1afd881ecfa65c616261e7aeb76ecd2bf87770b6`, not the later observed remote
`97aa4e43` or this documentation checkout.

| Requirement | Observed Windows developer host |
| --- | --- |
| OS / architecture | Windows 10 Home 22H2, build 19045.6466, AMD64 |
| Full LLVM SDK | LLVM/Clang/clang-cl/LLD 23.1.4, x86_64-pc-windows-msvc; archive SHA-256 `854bffcfc84528e0ee92490ff5c178920abd358a79c3e98f053e41e3e8e4c6d2` |
| Microsoft environment | VS Build Tools 2022 17.14.37806.7; MSVC toolset 14.44.35207, cl 19.44.35229; Windows SDK 10.0.19041.0 includes/UCRT/UM libraries |
| Other observed tools | CMake 4.4.3, Ninja 1.13.2, Python 3.14.8; these are observations, not certified minimum versions |
| Existing native inputs | `.toolchains/schannel-acceptance/{Debug,Release}` and matching `.toolchains/http/prefix/windows-schannel-x86_64/{Debug,Release}`; Release CMake prefix also included the SDK's `-deps` directory |

Source builds require the native x64 Visual Studio developer environment, the full
SDK with `clang-cl`, real zlib/static zstd/LibXml2 development targets, and the
matching Windows Schannel HTTP dependency profile, then its matching host
providers. `tools/setup_host_deps.py` delegates to the existing native builder;
`tools/verify_host_deps.py` checks the Windows x64 receipt before CMake. The
[SDK helper](../tools/ci/llvm_sdk.py),
[native dependency recipe](../tools/setup_http_deps_native.py) and
[ABI repair record](llvm23/CROSS_PLATFORM_REPAIR.md) describe those build inputs;
the isolated-install exercise did **not** freshly provision or rebuild them.
The standard HTTP recipe expects `.toolchains/llvm-23.1.4`; the report's existing
SDK was at its separately recorded developer path. Do not substitute the Linux
AWS-LC/GNU-library prefix or assume the developer VM's layout is a clean recipe.

Windows CMake matches the official SDK's release static MSVC runtime `/MT` and
iterator ABI in Debug and Release. Native `voidc build` uses configured `clang.exe`
with static runtime flags and the active/installed Microsoft libraries; its
manifest linker override may be empty. The Windows driver-plan inspection chose
SDK `lld-link`, static `libcmt` and MSVC/Windows SDK x64 libraries. That inspection
was a plan, not a trace of the successful build. DIA discovery may additionally
require `VSINSTALLDIR` or a real `VOID_DIA_GUIDS_LIBRARY`.

The accepted test installed compiler/tooling into a restricted new prefix from
private copies of generated CMake install scripts. Those copies redirected only
absolute manifest-write/nested-script paths into a shadow directory, preserving
artifact sources and component rules; all 562 original build-tree files retained
paths, sizes and modification times. The report does not supply a universal shadow
script generator, so no reconstructed script or untested clean-host command is
presented here as proven.

Installed check/run/native build, an EXE-only copied execution and a real `voidls`
handshake passed with restricted PATH and no explicit VOID/VS include/library
overrides. The SDK, MSVC, Windows SDK and installed VC redistributable remained on
disk. Direct PE imports were system DLLs; transitive/dynamic dependencies and a
machine without those developer/runtime installations were not established.
Independent provisioning, fresh compiler rebuild, installer/PATH registration and
uninstall remain unqualified.

### macOS build requirements

The hosted LLVM23 matrix includes macOS ARM64 Debug/Release. It is compiler
regression evidence, not a clean source-install qualification. Use the complete
official ARM64 SDK, Xcode 26+ vendor libc++ headers with Apple's system runtime,
a physical selected Xcode SDK, explicit architecture/deployment target, and the
matching macOS HTTP prefix followed by the matching host-provider profile.
The existing host setup/verifier requires the macOS ARM64 receipt before CMake.
Configuration checks the actual C++20 stop-token and
ORC link/run paths. The [cross-platform record](llvm23/CROSS_PLATFORM_REPAIR.md)
describes the ABI setup; the later
[HTTP acceptance](net/http/ACCEPTANCE.md) records the successful seven-job run.
Linux/Windows install results do not establish macOS installation or editor use.

### Tests, sanitizers and optional research

The production-only install recipe above deliberately does not run CTest. For a
separate developer regression build, prepare the corresponding HTTP and host
profiles (`Debug`, `Release`, or `strict`), then the
[HTTP test fixtures](../tools/setup_http_tests.py), and configure a separate build
with `BUILD_TESTING=ON`. Build all targets and run its unfiltered CTest suite;
building only `voidc`/`voidls` does not build every test executable. The fixture
setup also supplies certificate tooling and pinned Python test modules.

Use `RelWithDebInfo` plus `VOID_SANITIZERS=ON` for the strict profile. LeakSanitizer
needs a host that permits process inspection and cannot run under ptrace.
CMake matches SDK RTTI across linked C++ targets; the official Linux no-RTTI SDK
cannot provide UBSan vptr coverage. ASan/other supported UBSan checks remain
available. See [sanitizer coverage limits](CI_SANITIZER_ABI.md). No instrumented
coverage is claimed for this Release installation exercise.

Machine-code ownership certification is optional research: configure
`VOID_LLVM_RESEARCH=ON`, build `void_arrays_storage_audit` and run the research
label separately. Ordinary source JIT/AOT uses typed IR validation, LLVM
verification and shared lowering without a production machine-code certificate
requirement. The [private fixed-call experiment](../tests/research/call_envelope/README.md)
remains separate from source arrays. None of these experiments was run as part of
this documentation sprint.

## Working pipeline

`SourceFile` validates UTF-8 and retains original byte offsets. The indentation
lexer emits tokens, newlines and block boundaries. The recursive-descent parser
builds an AST per file. The module graph resolves all function signatures first, then
semantic analysis checks
lexical bindings and exact types, annotates expressions and determines possible
control-flow exits. Only validated ASTs lower into the shared VOID IR. The IR
validator checks its independent invariants before LLVM lowering.

The LLVM backend compiles that same IR for both ORC JIT and TargetMachine object
emission. There is no architecture-example recognizer or second source code
generator. Changing a source literal or function name changes both native outputs.
Compiler responsibilities stop at code generation; the existing experimental
heap and thread coordinator serve managed source through explicit runtime ownership
and AOT bootstrap; reload leases and the C bridge remain runtime/host responsibilities.

Source `private` function definitions now use LLVM internal linkage, except when
explicitly selected as the host/JIT or AOT entry. All source files in an import
graph lower into one LLVM module, so their private implementation calls resolve
locally. Public/project-visible source functions, legacy native definitions,
trusted C imports, Runtime ABI declarations and externally consumed metadata
retain their existing linkage. Virtual/interface helpers were already internal.
Names, generation suffixes, calling conventions and source visibility are
unchanged. Debug subprogram metadata reflects local linkage. This is a function
construction rule, not an internalization pass or a new native FFI contract.

`pass` is supported as a standalone no-op in every ordinary statement block,
including methods and initializers. Semantic analysis preserves fallthrough and
required-return/field-readiness checks; lowering emits no IR operation or runtime
call. Empty blocks remain invalid, and interface requirements remain bodyless.
The canonical syntax and examples are in LANGUAGE_BIBLE.md Section 27.2;
`examples/pass.void` exercises nested control flow and subsequent execution through
normal/O2 JIT and separately linked AOT in the existing frontend/source_cli suites.
The editor grammar assigns `pass` the same control-keyword scope as `return`.

## Syntax and supported semantics

```swift
func sum(left: i32, right: i32) -> i32:
    return left + right

func entry(value: i32) -> i32:
    var total: i32
    var index = 0
    for index < value:
        index++
        if index % 2 == 0:
            continue
        total += sum(index, 1)
    for:
        if total >= 20:
            break
        total += 3
    return total
```

- Functions use `func name(parameter: type, ...) -> type:`. Module-level
  `private`, `public` and `internal` modifiers are supported; see MODULE_SYSTEM.md. Parameters require
  types. Omit the return annotation for no result. Empty `return` or fallthrough
  is valid only for a no-result function. Recursive and forward calls work.
- All ten exact-width numeric types plus bool and the value structs described in
  VALUE_STRUCTS.md work. Contextual literals and
  explicit checked numeric `as` follow [NUMERIC_TYPES.md](NUMERIC_TYPES.md).
  Calls check positional argument counts and exact types after literal adaptation.
- `var x = expression`, `var x: i32 = expression`, and `var x: i32` work.
  Annotated variables without an initializer receive safe defaults (including
  declared/recursive value struct defaults). Parameters
  and var locals are mutable. Local readonly const and module compile-time
  constants are now implemented; see VALUE_SEMANTICS.md. Local bindings are visible
  after declaration and only within their lexical scope. Shadowing visible locals, parameters or function names is
  rejected; sibling scopes can reuse names.
- Assignment `=`, arithmetic compounds `+= -= *= /= %=`, and standalone postfix
  `++`/`--` work. Increment/decrement cannot be used as expression values.
- Arithmetic uses unary `+`/`-`, `+ - * / %`; comparisons use `< <= > >= == !=`.
  Equality accepts matching numeric or bool operands; ordering requires matching numeric types.
- Boolean operators are `not`, `and`, `or`. `and`/`or` short-circuit at runtime.
  Arguments and binary operands evaluate left to right. From weakest to strongest:
  `or`, `and`, `not`, comparisons, addition/subtraction, multiplication/division/
  remainder, numeric `as`, unary arithmetic. Parentheses override precedence. Comparisons do
  not implement chained-comparison semantics; write `a < b and b < c`.
- `if condition:`, `else if condition:`, and `else:` nest normally. Conditions
  require bool. Every reachable end of a value-returning function needs a return.
  Analysis is conservative for conditional branches and conditional loops;
  unbounded `for:` and literal `for true:` need no return after the loop unless a
  reachable break can exit it. Unreachable statements still receive semantic
  checks, and are omitted from lowering.
- All loops use `for`: conditional `for condition:` or unbounded `for:`.
  `continue` reevaluates the nearest loop condition; `break` exits the nearest
  loop. Collection/range iteration is rejected until its types exist.
- Function calls can be statements, including no-result calls. Other standalone
  value expressions are rejected.

## Source and diagnostics

Files must be valid UTF-8; a leading UTF-8 BOM is accepted. LF, CRLF and CR line
endings work, as does a missing final newline. Comments start with `#` (including
`##` documentation-style lines), and may contain Unicode. Indent each block
with tabs or spaces consistently through its nested levels. Tabs count as four
columns. Space indentation may use other widths when dedents match earlier
levels. Mixing tabs and spaces in a structural prefix or switching style
within an enclosing block is diagnosed. Blank/comment lines do not open blocks.
Expressions and declarations are single-line in this subset.

Names currently accept lowercase ASCII letters, digits after the first character,
and underscores. This does not settle future Unicode identifiers or the full
language naming policy. Unsupported characters and types are diagnosed.
Locations are one-based file/line/Unicode-scalar column positions, with the actual
source line and a caret. Scalar columns are not terminal display widths for wide
or combining characters. Diagnostics now collect independent errors with stable codes, ranges, related
declarations and bounded recovery; see [DIAGNOSTICS.md](DIAGNOSTICS.md).

For example, `while value > 0:` produces a VOID diagnostic at that keyword:

```text
example.void:2:5: error: VOID uses 'for' for all loops.
    code: VOID1104

 2 |     while value > 0:
   |     ^^^^^
    Hint: Replace 'while condition:' with 'for condition:'.
```

Invalid indentation/tokens, unresolved names, duplicate declarations, argument
counts, type mismatches, invalid returns and missing return paths are rejected
before LLVM. Malformed UTF-8 is rejected even in comments.

## Approved numeric behavior

Phase 6 extends the existing i32 wrapping/division contract to the exact-width
integer family and adds IEEE f32/f64 arithmetic, contextual literals and checked
numeric casts. See [NUMERIC_TYPES.md](NUMERIC_TYPES.md) for the approved rules,
shared software arithmetic, LLVM guards, diagnostics and precise remaining limits.
Integer division guards zero and MIN/-1 before dangerous LLVM operations;
float division by zero follows IEEE. No Language Bible text was changed.

## CLI and native executable boundary

```text
voidc check <file-or-directory>
voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--no-cache | --cache]
voidc build [file-or-directory] [-o executable] [--no-opt]
voidc emit <file-or-directory> -o <object> [--no-opt]
```

Run/build without a path select cwd using exact-case `main.void` discovery.
`check` accepts libraries with no entry. Executable programs use synchronous
zero-argument `main` (no result or i32), executable top-level statements, or the
legacy `entry(i32) -> i32` adapter. The application forms take no numeric operand;
fallthrough/no-result main exits 0 and an i32 result supplies the exit status.
Only legacy entry takes the optional decimal input and prints its numeric result.
Competing application entry forms reject; the legacy adapter retains its defined
precedence. See [application hosting](STAGE9_APPLICATION_WORKFLOW.md).

O2 is the default. `--no-opt` disables LLVM IR optimization for run/build/emit.
Run's `--no-cache` bypasses persistence; explicit `--cache` selects the existing
Debug-cache behavior and disables optimization. The flags cannot be combined.
The removed `--app` option rejects. Invalid invocation exits 2; source/backend/
Runtime/toolchain failures exit 1. An application's deliberate nonzero result is
its status, not an invented compiler diagnostic.

`build` performs native linking with configured resources and preserves the
output on failure. `emit` remains object-only; manual linking must supply the
matching driver, Runtime/provider archives and platform flags.

Source symbols encode their declaring module and function identity before the C
adapter is added, allowing same-named functions in different modules and helpers
named `main`, `printf` or `strtoll` without native adapter collisions. Stable
module identity is implemented; public library/export ABI remains deferred. Generated line
tables refer to the real input file. Variable debug records are not implemented.

## Validation and next phase

The existing six architectural suites remain, with `frontend` and `source_cli`
added to the same test executable/build. Frontend checks run three real files
and thirteen independent programs through normal/O2 ORC, plus over fifty negative
and boundary cases. Integration tests check, emit, link, verify DWARF and execute
three actual files with normal/O2 JIT and AOT, using paths with spaces. A modified
architecture source changes `42` to `13` and renames its helper to `main`; both
engines then return 23 for input 10. Tests retain the native `printf` proof.
See WORK_LOG.md for actual configuration commands and results.

The user confirmed this original eight-suite frontend phase passed Linux,
Windows and macOS Actions. The next module phase adds two suites and is documented
in MODULE_SYSTEM.md and WORK_LOG.md. The user reports that ten-suite phase also
passed all three hosted platforms. The user reports Phase 4 value semantics passed Actions run #6 across all three
platforms. Phase 5 diagnostic recovery requires another hosted run.

Current constants, arithmetic and runtime errors are documented in
[VALUE_SEMANTICS.md](VALUE_SEMANTICS.md). Structured diagnostic recovery is now implemented in [DIAGNOSTICS.md](DIAGNOSTICS.md).
Phases 6–11 subsequently completed numeric types, validated analysis, tooling
sessions, LSP and editor integration. Phase 11 run #16 passed all three hosted
platforms according to the owner. Phase 12 adds VALUE_STRUCTS.md and the
value_structs_cli suite, preserving all 21 previous suites. The subsequent
receiver/initialization and GC phases completed that gate. Current managed source
and the next deferred initialization decisions are documented in MANAGED_STRUCTS.md.

## Phase 13 receiver methods

Supported value structs now have owner-module receiver declarations and ordinary
value.method calls, including imported/nested receivers, inferred readonly effects,
normal/O2 JIT/AOT and owned tooling references. See RECEIVER_METHODS.md. Custom
init execution was deferred under Phase 13 I1; Phase 14 approved C1 now implements
it as documented in INITIALIZATION.md. No-init named construction and the CLI
check/run/emit i32 entry adapter remain available.


## Phase 14 custom value initialization

INITIALIZATION.md documents approved C1 custom value constructors, typed
positional/named arguments, pure defaults, recursive availability/access checks and
lifecycle diagnostics. Fresh construction uses shared receiver Places and explicit IR
invocation metadata; source execution and optimized/unoptimized JIT/AOT share the same
lowering. Required-child wrappers, pure contexts, readonly publication and conservative
initializer/helper reload contracts remain explicit boundaries.

Phase 18 implements owner-approved reference-containing value structs, per-leaf
readonly capability/effect checking, complete construction and precise roots through
normal/O2 JIT and native AOT. See [REFERENCE_VALUE_STRUCTS.md](REFERENCE_VALUE_STRUCTS.md);
earlier phase-only type exclusions in this historical guide are superseded there.


## Phase 19 composition

Owner-approved stored `using name: type` and `using type` composition, one-hop
member promotion and original embedded receiver mutation are implemented through
the existing paths. See [COMPOSITION.md](COMPOSITION.md) for exact lookup, access,
readonly, initialization, tooling and reload behavior. This adds neither subtyping
nor stateless reuse and does not change C1/S1/P3/P4/RV1–RV7. Phase 20 adds managed
inheritance and component virtual dispatch as described in INHERITANCE.md. Phase 21 adds managed-backed structural interfaces as documented in INTERFACES.md.
Transitive promotion, stateless reuse and has remain deferred.


## Phase 21 structural interfaces

Owner-approved IF1–IF8 implement bodyless interface requirements, implicit exact
structural conformance, managed-backed identity views, nullable values, readonly
capabilities and original-storage receiver dispatch. Locals, fields, copied values,
arguments and returns share the same frontend/IR/LLVM pipeline and precise roots.
Private factory exposure retains conversion-scope access proof; calls navigate to
requirements. Independent IR validation recomputes complete nominal/binding universes
and effects. See [INTERFACES.md](INTERFACES.md) for the source contract, budgets,
publication, reload policy, deferred value erasure/boxing and private ABI boundaries.

## Application hosting (Stage 9 supersedes Sprint 4 CLI selection)

[Stage 9](STAGE9_APPLICATION_WORKFLOW.md) selects application/legacy mode from
validated compiler analysis. `voidc run [file-or-directory]` and `voidc build
[file-or-directory] [-o executable]` default to cwd discovery; `voidc emit
<file-or-directory> -o <object>` keeps object emission. Options retain `--no-opt`
and Debug run caching. The old application flag is removed and rejected. Applications accept no numeric operand and return their status
without printing it. The linked executable likewise accepts no arguments.
Invalid invocation exits 2; compile/runtime failure exits 1. Platform exit-code
representation applies to explicit i32 results.

The selected source may contain executable top-level statements or synchronous
zero-argument main, with no result or i32. These competing entry forms reject
when combined. Top-level fallthrough/bare return yields 0; an i32 return supplies
status. Explicitly typed main requires a return on every reachable path. Imported
executable statements reject. Stage 11 makes outermost `var` declarations runtime
globals; bindings inside top-level blocks remain body locals. Declaration-only
legacy entry programs retain their printed numeric adapter and precedence
over an ordinary main helper. Numeric input remains exclusive to legacy entry.

Implementation and local acceptance status are maintained in the linked contract;
this does not grant editor execution, native import trust or hosted acceptance.


## Stage 8 Sprint 6 — 21 October 2026

[The bounded ergonomics contract](STAGE8_ERGONOMICS.md) supersedes historical
blanket exclusions above for compile-time type `has`, typed if-expressions,
multiline calls and string-only interpolation. Existing nominal storage,
structural conformance, generic specialization, capabilities and checked failure
rules remain authoritative. Both conditional branches type-check. Formatting and
numeric/bool/custom string conversion remain deferred. Approval and implementation
are separate from local acceptance, Wrench review and the final hosted lock.
