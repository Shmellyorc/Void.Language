# Install the tools

_Draft for Boss approval; repository documentation is canonical._

Follow the canonical platform guide and its prerequisites:

- [Linux x64](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/linux.md): Ubuntu 24.04 is the userspace baseline.
- [Windows x64](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/windows.md): Visual Studio x64 developer environment, Windows and DIA SDKs.
- [macOS ARM64](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/macos.md): selected Xcode 26+ with matching Apple libc++/SDK.

The requirements include LLVM/Clang/LLD **23.1.4**, Python **3.12+**, CMake **3.24+**
and host C/C++ development files. From the checkout on Linux:

```sh
python3 tools/install.py --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
voidc --version
python3 tools/verify_install.py "$HOME/.local"
```

The helper verifies the SDK/dependencies, builds compiler/tooling, installs existing
CMake components and exercises an external Hello World. It does not install OS
packages or edit your shell profile. Keep the original SDK for native builds.
Linux testing, prior Windows developer-host evidence and prepared-only native
wrapper paths are distinguished in the [qualification report](https://github.com/Shmellyorc/Void.Language/blob/main/docs/developer-experience/REPORT.md).
A source installation is not an independently portable binary release.
Next: [First program](First-program.md). Full contract: [installation](https://github.com/Shmellyorc/Void.Language/blob/main/docs/install/README.md).
