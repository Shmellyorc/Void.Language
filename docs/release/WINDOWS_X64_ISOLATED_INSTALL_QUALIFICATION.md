# Windows x64 private installation qualification

4 November 2026. **PASS for a developer-host isolated install. Independent Windows clean-host qualification remains open.** The compiler and language server were installed into a new private prefix. From a project outside the repository, the installed compiler checked and ran an installed-console program, built a native x64 executable, and the installed language server completed an LSP handshake.

## Tested host and inputs

| Item | Observation before installation |
| --- | --- |
| Host | Windows 10 Home 22H2, build 19045.6466, AMD64 |
| Checkout | `C:\Users\a\Documents\ChatGPT\Void TEMP\Void-native`, branch `main`, HEAD `1afd881ecfa65c616261e7aeb76ecd2bf87770b6`. The two existing unstaged files were `docs/json/evidence/implementation/readonly-Debug.json` and `readonly-Release.json`. |
| GitHub comparison | Remote main was observed at `97aa4e4334f1b9354ba651770b5cf13e74da03d0`. The tested input was the existing local build. This record does not qualify binaries built from that newer ref. |
| LLVM | The SDK bin directory was `C:\Users\a\Documents\ChatGPT\Void TEMP\Void-native\.toolchains\native-schannel\sdk\clang+llvm-23.1.4-x86_64-pc-windows-msvc\bin`. Direct invocations of `llvm-config.exe`, `clang.exe`, `clang-cl.exe` and `lld-link.exe` there reported 23.1.4. Clang targeted `x86_64-pc-windows-msvc`. The SDK receipt records archive SHA-256 `854bffcfc84528e0ee92490ff5c178920abd358a79c3e98f053e41e3e8e4c6d2`. |
| Microsoft tools | Visual Studio Build Tools 2022 17.14.37806.7 at `C:\BuildTools\VS2022`. Toolset directory `VC\Tools\MSVC\14.44.35207`. Its `bin\Hostx64\x64\cl.exe` reported 19.44.35229. Windows SDK includes and libraries were 10.0.19041.0 under `C:\Program Files (x86)\Windows Kits\10`. |
| Other tools | CMake 4.4.3 at `C:\gcc\bin\cmake.exe`, Ninja 1.13.2 at `C:\gcc\bin\ninja.exe`, Python 3.14.8 at `C:\Users\a\AppData\Local\Programs\Python\Python314\python.exe`. |
| Available artifacts | Debug and Release compiler/tooling builds in `.toolchains/schannel-acceptance/{Debug,Release}`. Matching Windows x64 Schannel prefixes in `.toolchains/http/prefix/windows-schannel-x86_64/{Debug,Release}`. The Release cache also names the SDK's `-deps` directory as its CMake prefix. |
| Prior VOID discovery | Neither VOID executable resolved on the initial PATH. Neither standard Program Files location checked contained a VoidLanguage installation. An older Desktop checkout had Debug and Release build-tree `voidc.exe` files. This was scoped discovery, not an exhaustive disk scan. |

## Isolation and installation method

The input was the accepted CMake Release build. This exercise installs existing artifacts and does not attest a fresh compiler rebuild. Both installed tool hashes matched their build-tree inputs.

The harness used a fresh directory restricted to the current user and SYSTEM, a private temporary directory, and an external project. The compiler/tooling child environment removed VOID overrides and explicit VS include/library environment variables. Its PATH contained only the private `bin`, Windows System32 and Windows. The SDK and MSVC installations remained available on disk.

CMake's generated install scripts contain absolute paths back to the existing build for manifest writes. Private copies redirected those writes and nested install-script references to a shadow directory. Artifact source paths and component install rules were preserved. The successful run compared the original Release tree before and after installation and testing. All 562 file paths, sizes and modification times matched.

## Acceptance results

| Check | Result and evidence |
| --- | --- |
| Private install | PASS. `cmake --install <shadow> --config Release --prefix <prefix> --component compiler` and the corresponding `tooling` command both exited 0. All 76 installed files were hashed and reverified. All seven archive hashes in `bin/void-native-resources.txt` matched files in the prefix. |
| Discovery | PASS. `where.exe voidc` and `where.exe voidls` each exited 0 and returned only the matching private `bin` executable. |
| Installed console control | PASS. Temporarily withholding only the installed `share/void/stdlib/console.void` made `voidc check main.void` exit 1 with `VOID3002` for `/@stdlib/console.void`. Restoring it reproduced the checkout module hash. |
| Source check | PASS. Installed `voidc check main.void` exited 0 and reported `Source checked successfully`. |
| Source run | PASS. Installed `voidc run main.void` exited 0. Standard output was exactly `private Windows install OK\n` (LF). Standard error was empty. |
| Native build and execution | PASS. Installed `voidc build main.void -o qualified.exe` exited 0 and reported the created executable. It was AMD64 PE and exited 0 with the same exact output and empty stderr. A copy ran from a directory containing only the EXE, with only Windows directories on PATH, with the same result. |
| Language server | PASS. Installed `voidls --entry <external main.void>` exchanged byte-counted `Content-Length` frames. Initialize ID 1 returned server `VOID` version `0.2.0`, UTF-16 positions and definition support. After `initialized`, shutdown ID 2 returned null. The client sent `exit` and the process exited 0. |

Tested `main.void`:

```void
use console
func main() -> i32:
    if console.stdout.write("private Windows install OK\n") is Err(var failure):
        return 9
    return 0
```

## Native dependencies and distribution boundary

The installed `bin/void-native.txt` names the **absolute developer SDK clang.exe** and prefix-relative runtime, fixture and dependency archives. Its linker override is empty. A separate `clang.exe --driver-mode=g++ -### <installed void_runtime.lib> -fms-runtime-lib=static -o planned.exe` inspection exited 0. The plan selected the SDK's `bin\lld-link`, static `libcmt`, MSVC x64 libraries and the Windows SDK UCRT/UM x64 directories. This is a driver plan, not a process trace of the preceding successful build.

Direct PE import inspection used the SDK's `llvm-readobj.exe --file-headers --coff-imports`. Every tool inspection exited 0 and the named DLLs existed in Windows System32.

| Binary | Direct DLL imports |
| --- | --- |
| Installed voidc | ADVAPI32, CRYPT32, IPHLPAPI, KERNEL32, Normaliz, SHELL32, Secur32, WS2_32, bcrypt, ntdll, ole32 |
| Installed voidls | ADVAPI32, KERNEL32, SHELL32, ntdll, ole32 |
| qualified.exe | KERNEL32 |

Clang and LLD likewise listed system DLLs. The x64 VC redistributable registry entry reported `v14.44.35211.00`, installed. No separate VC runtime DLL appeared in the inspected direct imports. Transitive/dynamically loaded dependencies and behavior on a machine without the redistributable, SDK or MSVC were not established.

## Artifact identities

| Artifact | SHA-256 |
| --- | --- |
| Installed voidc.exe | `9e7a9d283dc9ba51bdb3bed00aeb680af687bdbfa86d1743ad63ee130d662993` |
| Installed voidls.exe | `2b9ee648cbc67976363c93a0413e48de9065ad943fd38c2d2795688286faf2ca` |
| void_runtime.lib | `e004b01b0a48b4663356f949065d3da571dff12cff5b4e8b0d036b05ca6dc7b4` |
| Installed console.void | `34b5d317e5f536c3e4b43317019b4e713b4f3e7423b99ef699fc256729772055` |
| main.void | `fa5c8013a56d1694ffc727b766d2b7bc0455daafeec64abbda4a5de4e77df439` |
| qualified.exe | `68423ed09a68e2436fc6253a047ae5f07ce306734e5bc1348e2530f4ca3483df` |

## Private evidence and handoff for Boss

The private run is `C:\Users\a\Documents\ChatGPT\Void TEMP\Private-Windows-Qualification\20261104-065436-bc47b4`. It contains `host.json`, `inventory.json`, `commands.json`, command stdout/stderr, LSP wire bytes, PE reports, installed-file hashes, preservation and verification receipts. The qualification harness is `qualify-20261104.py` in its parent. Raw logs, the prefix and binaries stay outside the checkout.

Two harness failures remain recorded. An earlier private run (`20261104-065419-4a61f8`) installed both components, then stopped on a case-sensitive Python dictionary lookup for Windows' system-root variable. The corrected harness used a fresh prefix. A supplemental Clang driver-plan inspection named an absent dummy object and exited 1. The successful inspection used an existing installed archive. Neither failure occurred in the compiler or tooling acceptance commands.

| Disposition for the release blocker register | Windows finding |
| --- | --- |
| Bounded developer-host isolated install | PASS on the named Windows 10 x64 host for the Release artifacts above. |
| Independent clean-host installation | OPEN. A fresh Windows installation and machine-independent provisioning of the compiler driver, MSVC and SDK libraries have not been qualified. |
| Public package or installer | Unassessed. This private run did not exercise an installer, system PATH registration, redistribution packaging or uninstall. |
| Other platform and release gates | Carry existing dispositions forward. Linux records are owned by Boss on the Linux checkout. HTTP acceptance and historical optimizer campaigns were outside this run. |

This Windows-only record supplies the handoff and blocker entries for Boss-controlled synchronization. **Stop for Golden Wrench review.**
