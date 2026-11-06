# Install VOID from source

Start with your native platform: [Linux x64](linux.md), [Windows x64](windows.md),
[macOS ARM64](macos.md). The complete LLVM **23.1.4** development SDK and Python
**3.12+** are required. A Clang-only installer is insufficient.

After platform prerequisites, run `python3 tools/install.py --prefix <directory>`
from the checkout (`python` on Windows). The helper reuses:

1. `tools/ci/llvm_sdk.py`: pinned official archive SHA-256, safe extraction,
   cached identity, development headers/libraries and matching Clang/LLD checks.
2. `tools/setup_http_deps.py`: the existing native pinned dependency recipes;
   absent prefixes are built. Existing prefixes must pass
   `tools/verify_http_dependencies.py`, including source/archive and installed-file hashes.
3. CMake: a separate Release build (or `--configuration Debug`), compiler/tooling
   targets, then the existing `compiler` and `tooling` install components.
4. `tools/verify_install.py`: an external temporary Hello World project, installed
   stdlib, optimized and Debug JIT, native build/run, resource hashes and LSP handshake.

Ninja is selected when available; Linux/macOS otherwise use Unix Makefiles.
Windows requires Ninja. `--jobs 3` is the default; use fewer jobs on a memory-limited
host. `--build-dir <directory>` selects a separate CMake tree. Existing build and
install directories may be reused, but must match their toolchain/configuration.
Use a fresh build directory after changing generator, compiler or configuration.
The helper does not install OS packages or change PATH/profile files.

The install prefix contains:

```text
bin/voidc, voidls                # .exe on Windows
bin/void-native.txt              # host compiler/linker and link flags
bin/void-native-resources.txt    # hashes of matching native libraries
lib/void/                       # Runtime, native support and HTTP dependencies
share/void/stdlib/               # matching .void sources
share/void/licenses/             # VOID and dependency notices/source inputs
share/void/vim/, neovim/         # editor adapters
```

Keep the prefix together. Installed stdlib discovery is relative to the actual
executable; no `VOID_STDLIB_ROOT` is needed. An explicit absolute override wins and
must name matching sources. The native manifest still names the original SDK's
Clang/LLD. Keep that SDK and the platform's development libraries in place.
Native executables have their own platform/dependency requirements; successful
source installation does not make either the tools or their output universally portable.

After adding `bin` to PATH, run `voidc --version` and
`python3 tools/verify_install.py <prefix>`. Logs and the per-command receipt are in
`<build-directory>/source-install.json` and adjacent `.log` files. A failed setup
stops before later stages; a failed installation may leave a partial prefix.
CMake installation is not transactional. Re-run after fixing the reported cause.

For explicit manual setup and CMake commands, see the
[engineering build guide](../SOURCE_COMPILER.md#building-the-compiler).
For first use, see [Hello World](../FIRST_PROGRAM.md), [CLI](../CLI.md) and
[editor setup](../EDITOR_INTEGRATION.md).

## Evidence and distribution boundary

The [milestone report](../developer-experience/REPORT.md) records this wrapper's
actual tests and original failures. Earlier [Ubuntu 24.04 container qualification](../release/LINUX_CLEAN_HOST_QUALIFICATION.md)
and [Windows developer-host installation](../release/WINDOWS_X64_ISOLATED_INSTALL_QUALIFICATION.md)
remain dated evidence for their own source revisions. Seven-job compiler CI is
separate from native installation qualification. The new wrapper's Windows/macOS
paths need native execution before being advertised as verified onboarding.
No independent portable package, public binary release, package manager or stable
one-command installer is claimed. Preserve [license obligations](../release/REDISTRIBUTION_AUDIT.md)
when copying or distributing installed material.
