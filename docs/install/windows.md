# Windows x64 source installation

Use native x64 Windows with Visual Studio Build Tools 2022, C++ desktop tools,
MSVC x64 libraries, the Windows SDK/UCRT and the DIA SDK. Use a matching **x64
Developer PowerShell/Native Tools environment** (`VSCMD_ARG_TGT_ARCH=x64`), not an
ordinary shell without the SDK environment. Python **3.12+**, CMake **3.24+** and
Ninja must be on PATH. Long-path policy and application support are not equivalent;
start with a short local source/build path.

The official full LLVM **23.1.4** tar archive contains headers, static development
libraries and CMake exports; the compiler-only `.exe` installer is insufficient.
The existing SDK helper also builds the pinned zlib/zstd/LibXml2 development
libraries required by LLVM. VOID matches the SDK's static release `/MT` CRT in
Debug and Release. HTTP uses the pinned Windows Schannel/WinIDN recipe.

Prepared invocation in x64 Developer PowerShell:

```powershell
python tools/install.py --prefix "$env:LOCALAPPDATA\VOID"
$env:Path = "$env:LOCALAPPDATA\VOID\bin;$env:Path"
where.exe voidc
where.exe voidls
voidc --version
python tools/verify_install.py "$env:LOCALAPPDATA\VOID"
```

For future shells, add `%LOCALAPPDATA%\VOID\bin` to your user PATH through Windows
Environment Variables, then reopen the shell. [Hello World](../FIRST_PROGRAM.md)
uses `voidc build main.void -o hello.exe` and `.\hello.exe`.

**Evidence:** an [isolated installation on a Windows 10 developer host](../release/WINDOWS_X64_ISOLATED_INSTALL_QUALIFICATION.md)
checked, ran and built an installed-console application and initialized `voidls`.
That test installed pre-existing Release artifacts from its recorded revision;
it was not an independent clean-host source build. The accepted SIMD seven-job
CI covers Windows compiler tests at `175c08c8`. Neither qualifies this new wrapper:
the invocation above is prepared and needs native Windows execution. No Windows
command has been counted as tested through Linux simulation. The [manual build
guide](../SOURCE_COMPILER.md#building-the-compiler) retains the underlying commands.

Keep the original SDK and Microsoft development resources available. Direct DLL
inspection and one successful copied application do not establish general package
portability. See [installation boundaries](README.md) and [troubleshooting](../TROUBLESHOOTING.md).
