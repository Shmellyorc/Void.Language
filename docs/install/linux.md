# Linux x64 source installation

The established userspace baseline is Ubuntu 24.04 x86-64/glibc. Other Linux
distributions require equivalent development packages; no lower glibc floor or
musl/ARM64 support is established by this recipe.

On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build python3 libffi-dev libedit-dev \
  libxml2-dev libzstd-dev zlib1g-dev libtinfo-dev \
  ca-certificates xz-utils git perl
```

Python must be **3.12+**, CMake **3.24+**. Make, Perl, C/C++ headers, CRT startup
objects and libstdc++ development files are needed as well as LLVM. LLVM's exports
require real zlib/zstd/LibXml2 development packages. Distribution CA roots are
needed for downloads and HTTPS; the recipe does not replace them with a bundled
trust store.

From a source checkout obtained through the project's GitHub repository:

```sh
python3 tools/install.py --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
command -v voidc
command -v voidls
voidc --version
python3 tools/verify_install.py "$HOME/.local"
```

Add `export PATH="$HOME/.local/bin:$PATH"` to your shell's startup file for future
shells. Bash login shells use `~/.profile` (or your existing login configuration);
interactive Bash configuration may use `~/.bashrc`. Reopen the shell and confirm
`command -v` resolves the intended installation.

The SDK is installed under `.toolchains/llvm-23.1.4`; HTTP source downloads and
prefixes remain under `.toolchains/http`. Existing valid caches are verified and
reused. A missing cache can require several GB of downloads/storage and a substantial
source build. Keep the SDK for subsequent native builds.

Follow [your first program](../FIRST_PROGRAM.md). See the
[qualification boundary](README.md#evidence-and-distribution-boundary) and
[troubleshooting](../TROUBLESHOOTING.md) before treating the result as a package.
