# Install the tools

Install `voidc` to check, run and build programs. The matching `voidls` supplies
language assistance to [your editor](https://github.com/Shmellyorc/Void.Language/wiki/Editor-setup). Both come from the same source build.

## Choose your platform

| Host | Prerequisites and canonical instructions |
| --- | --- |
| Linux x64 | [Linux source installation](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/linux.md); Ubuntu 24.04 is the established userspace baseline. |
| Windows x64 | [Windows source installation](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/windows.md); use the x64 Visual Studio developer environment with the Windows and DIA SDKs. |
| macOS ARM64 | [macOS source installation](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/macos.md); selected Xcode 26+ and matching Apple SDK/libc++ are required. |

Prepare your host prerequisites before running the helper. All platforms require
the complete LLVM/Clang/LLD **23.1.4** development SDK, Python **3.12+** and CMake
**3.24+**. A compiler-only LLVM installer is insufficient.

## Linux quick path

From a source checkout, after following the Linux prerequisite guide:

```sh
python3 tools/install.py --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
command -v voidc
command -v voidls
voidc --version
python3 tools/verify_install.py "$HOME/.local"
```

The helper verifies dependencies, builds the tools and installs them under your
chosen prefix. Missing dependencies can require substantial downloads, disk space
and build time. It does not install OS packages or edit your shell profile.
The verifier checks the resulting installation with an external application and
server handshake. Keep its logs if a step fails; see [Troubleshooting](https://github.com/Shmellyorc/Void.Language/wiki/Troubleshooting).

For Windows or macOS, follow the corresponding native platform guide;
prerequisites and commands differ. See [platform limits](https://github.com/Shmellyorc/Void.Language/wiki/Resources-and-status#platform-limits)
before choosing tools or distributing an application.

## Keep the installation together

Keep `bin/`, `lib/void/` and `share/void/` together. Standard sources are discovered
relative to the executable; ordinary use needs no `VOID_STDLIB_ROOT` override.
Keep the original LLVM SDK and host development libraries for
[native builds](https://github.com/Shmellyorc/Void.Language/wiki/Native-compilation). Source installation does not establish
portable binary distribution or minimum-OS compatibility.

Next: [Create your first application](https://github.com/Shmellyorc/Void.Language/wiki/First-program).
Full installation contract: [Install from source](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/README.md).
