# Troubleshooting the development loop

| Symptom | Check / action |
| --- | --- |
| `voidc` or `voidls` not found | Add the installed `bin` to PATH; reopen the shell. Use `command -v` (Unix) or `where.exe` (Windows), then `voidc --version` to identify the selected copy. |
| Missing Python/CMake/Make/Ninja/Perl | Follow the native [platform prerequisites](install/README.md). Python 3.12+ is needed for ordinary dependency configuration too. |
| LLVM configuration/ORC probe fails | Use the complete pinned 23.1.4 SDK with matching Clang/LLD and real exported development dependencies. Read CMake's configure log; a compiler-only installer is insufficient. |
| SDK checksum/identity mismatch | Stop using that SDK cache. Preserve the failed evidence, move the mismatched cache aside, then rerun the existing SDK helper/source installer. No unverified fallback is supported. |
| HTTP prefix/archive digest mismatch | Preserve the log; run `python3 tools/setup_http_deps.py --profile Release` (or the matching Debug profile), then `tools/verify_http_dependencies.py <prefix> Release`. Do not reuse a different instrumentation/platform prefix. |
| Missing `main.void` | Directory discovery requires exact-case `main.void`. Pass an explicit source file to select another name. |
| `--app` rejected | Remove it. Entry selection is automatic; see [CLI commands](CLI.md). |
| Application rejects numeric input | Numeric operands belong to legacy `entry(i32) -> i32`; synchronous `main` takes none. |
| Nothing printed after returning a number | Application returns are process statuses. Use `use console` and `print(...)` for output. |
| `use console` / installed stdlib cannot resolve | Keep `share/void/stdlib` with the installed tools. Remove stale `VOID_STDLIB_ROOT`, or set an absolute matching source root. Run `tools/verify_install.py <prefix>`. |
| Native manifest/inventory/archive missing or mismatched | Reinstall the matching CMake `compiler` component. Keep `bin/void-native*.txt` and `lib/void` together. Do not copy only `voidc`. |
| Native Clang/LLD missing or wrong version | Keep the original SDK at its configured location; rebuild/reinstall if moving it. Native manifests are source-install resources, not a portable toolchain abstraction. |
| Native linker cannot find C/C++ libraries | Linux: development headers/CRT/libstdc++; Windows: x64 MSVC/Windows SDK; macOS: selected Xcode SDK/libc++. Read the driver diagnostics above the failure. |
| Unsafe output/case/collision error | Choose a new ordinary-file destination with exact physical spelling and without a `.void` extension. A failed build preserves the previous output. |
| Editor cannot start server | Set the adapter's absolute `voidls` path; use a trusted local workspace and inspect its output log. Follow [editor integration](EDITOR_INTEGRATION.md). |
| Async code checks but ordinary run rejects | Async execution needs the documented host adapter; see [async](ASYNC.md). No automatic async application host is promised. |

Installer logs and exact command/status receipts live beside its CMake build in
`source-install.json` and `.log` files. Include the command, `voidc --version`, OS,
architecture, source revision and relevant diagnostic when reporting a failure.
Do not infer native Windows/macOS behavior from Linux tests or a portable release
from a successful local build. See the [qualification report](developer-experience/REPORT.md).
