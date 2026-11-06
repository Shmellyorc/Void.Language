# macOS ARM64 source installation

Use native Apple Silicon with **Xcode 26+** selected and its physical macOS SDK.
The selected vendor libc++ headers must supply `stop_token`; CMake compiles, links
and runs a cancellation probe. The LLVM archive's bundled libc++ headers cannot
simply replace Apple's matching headers/runtime. Intel macOS and cross compilation
are not qualified by this recipe.

Install Python **3.12+**, CMake **3.24+**, Make and Perl; Ninja is preferred.
Select your installed Xcode through `xcode-select`, then confirm:

```sh
xcodebuild -version
xcrun --show-sdk-path
python3 --version
cmake --version
```

Prepared source-install invocation:

```sh
export MACOSX_DEPLOYMENT_TARGET=14.0
python3 tools/install.py --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
command -v voidc
command -v voidls
voidc --version
python3 tools/verify_install.py "$HOME/.local"
```

Add the PATH export to your shell's startup file (typically `~/.zprofile` for a
login zsh), reopen it and verify discovery. The wrapper selects the physical SDK,
ARM64 and the deployment target. It uses the existing install-only HTTP recipe,
which omits AWS-LC auxiliary executables while retaining static SSL/crypto libraries.
Clang/LLVM/LLD must be **23.1.4**, supplied by the pinned complete SDK.

**Evidence:** the accepted seven-job compiler run at `175c08c8` includes macOS
ARM64 Debug and Release. This milestone has no native macOS install execution;
the new wrapper invocation is prepared, not verified onboarding or a portable
package claim. CMake's configured deployment target is not evidence of execution
on that minimum OS version. See the [milestone report](../developer-experience/REPORT.md),
[manual platform build guide](../SOURCE_COMPILER.md#building-the-compiler) and
[installation boundaries](README.md).
