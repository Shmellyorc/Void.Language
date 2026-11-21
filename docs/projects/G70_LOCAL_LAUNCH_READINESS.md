# G70 local installation and launch readiness

> Storage correction, 2026-11-21: the historical `/tmp` evidence locations in this report have been cleaned. Retained Markdown and original failure records now use the [verified migration inventory](/home/shmellyorc/Documents/Void.Language-archives/storage-correction-2026-11-21/MIGRATION_INVENTORY.md). Redundant generated outputs and disposable fixtures were deleted under the Boss’s updated retention instruction. Historical commands and measurements below are unchanged.

Accepted source is G68 `92432510ebfe0844aae4e6285b8c4eff579ad137`. G69 remains
on hold. The owner's Linux installation and the Star Harbor walkthrough
passed. Public binary and production VOIDUP distribution are on hold.

## Installed tools and recovery

The shell resolves both tools through `~/.local/bin`. Before G70 they were
regular files there, with compiler resources under `~/.local/lib/void` and
`~/.local/share/void`. The alternate `~/.local/opt/void/bin` entries already
pointed back to that same pair. A later PATH entry under
`~/Documents/Void.Language/build/ci-official-debug` contains older binaries.
That compiler does not implement `--version`. The default VS Code profile
initially had neither the VOID extension nor LLDB DAP. There was no VSCodium
command, installed VOID extension in its default extension directory, or
VSCodium user settings. The portable VSCodium copy used for G69 is a test
artifact.

| Tool | Previous identity | Selected after G70 |
| --- | --- | --- |
| `voidc` | `~/.local/bin/voidc`, v0.1.0 development, LLVM 23.1.4, x86-64 GNU, build `0edb4318452b305df6de84a24a945e3d5b3bdb2516350fbc13d525210a7393be` | `~/.local/opt/void-g68-92432510/bin/voidc`, same version and target, build `ce93cbb3d457f012b5217f55a7e84bf061cb75b7b473823bcf3b08c38002bcd9` |
| `voidls` | `~/.local/bin/voidls`, LSP `VOID 0.2.0`, SHA-256 `099823629e014e347314844d622cf72bd671d9fbcab21207ab77fd32783cdef8` | `~/.local/opt/void-g68-92432510/bin/voidls`, LSP `VOID 0.2.0`, SHA-256 `ecbf3ac3db29eb0a19ebc9e0effbf039e1fe65beae7c10e3bdfb35d94bb7181a` |
| VS Code | 1.144.0, default extensions directory `~/.vscode/extensions` | `void-language.void-language@0.1.0` built from G68 and `llvm-vs-code-extensions.lldb-dap@0.7.20261117` from the qualified LLVM build |
| `voidup` | No command on PATH and no default root, active release or installation records at `~/.local/share/voidup`. Historical bootstraps exist under the checkout's `build` tree. | Native G68-built `0.1.0 engineering` bootstrap in the G70 evidence directory. Its explicit fixture root is described below. |

The G68 compiler binary SHA-256 is
`a2a2e486d3aa403f5d01ee7f4396141e2564bb10da71006ef28918d2c203faed`.
The generic G68 VSIX SHA-256 is
`58cee8b836dc7b80a5f4f8177ba113ffd66242a8f54f5d21fa53ca796d9da2f1`.
The installed manifest and bundled code match that package, and its MIT
license matches the root license. The VSIX includes the dependency notices.

The new prefix was installed side by side with the compiler and tooling CMake
components. The maintained verifier passed standard library discovery,
optimized and debug JIT, independent AOT, native resource checks and the LSP
handshake. It passed again in a mount namespace with the checkout and build
trees hidden and the pinned SDK bound back at its configured location. This
local prefix still needs that LLVM 23.1.4 SDK for native linking and preparation.

Before switching the two shell entries to symlinks, the old binaries, manifests,
installed native resources and standard library were copied into
`~/.local/opt/void-recovery-before-g70`. The full installed verifier passed
against the recovery copy. Either tool can be invoked there by absolute path.
To roll back the shell, close VOID editor sessions and place verified copies
of the recovery `bin/voidc` and `bin/voidls` at `~/.local/bin/voidc` and
`~/.local/bin/voidls`, using temporary files and renaming them over the two
G70 symlinks. The legacy resource directories are still in place.

The desktop-launched VS Code process did not have `~/.local/bin` on PATH.
Two settings were added to the user JSONC file. `void.server.path` points to
`~/.local/bin/voidls` using its absolute path, so the extension finds the
compiler beside it. `lldb-dap.executable-path` points to the existing
LLVM 23.1.4 adapter. The original settings file is backed up as
`Code-settings.before.jsonc` in the recovery prefix. To undo the editor
setup, remove the two added keys and uninstall the two extensions from the
user profile. Remove the keys individually if other settings have been
edited since the backup.

## Private VOIDUP replay

The native application used an explicit private root
`/tmp/voidup-g70-jjflvevx/root`, a localhost HTTPS endpoint and its test CA.
The catalog used the existing engineering keys and their required 2-of-3
Ed25519 threshold. It pointed to the original signed-inventory
`engineering-a` artifact and a new `g68-development` artifact made from the
real G68 compiler and server, with an SDK subset and installed resources.
The two artifacts report different compiler identities. The G68 payload was
built on the owner's host even though the fixture uses the existing Linux
catalog host identifier. The fixture does not qualify that payload on Ubuntu.

| Actual command or test | Outcome |
| --- | --- |
| `list --available` with a single endorsement | Rejected. The same catalog with both endorsements passed over HTTPS. |
| `install engineering-a --activate`, `verify engineering-a`, `which voidc`, `which voidls` | Passed. The managed compiler printed its identity and ran a source program. The managed server completed LSP initialize and shutdown. |
| `install g68-development --activate`, interrupted during transfer | Killed the isolated installer after 1 MiB. Concurrent `doctor` refused the held lock. The old default remained selected. |
| `doctor` and authenticated retry | Passed after interruption. The partial download was retained and the retry installed and selected G68. `verify`, compiler execution and LSP passed. |
| `default engineering-a`, `default g68-development`, `rollback` | Each switch succeeded. Default returned to `engineering-a`, and both tools ran through `run default`. |
| Damaged a file in the inactive fixture | `verify`, `default` and same-ID `install` refused the damaged release. The working default stayed active. Restoring the exact original bytes in the fixture let verification and `doctor` pass. |

For future private updates, produce a complete toolchain from a known build
under a new release ID, verify it, authenticate its catalog with the
engineering keys and serve it over HTTPS with an explicit CA. Verify the
installed release before selecting it, then invoke tools with
`voidup run default voidc -- ...` and `voidup run default voidls -- ...`.
`voidup which` gives the verified paths for explicit editor configuration.
The manager maintains its own selection file. Its activation does not switch
normal shell entries or the VS Code setting. Use a new signed ID when an
installed release is damaged and keep the previous working default.
The local owner setup above remains a source-component install.

## Star Harbor walkthrough

The exact [game source](../../examples/star-harbor/app.void) and original
[PNG sprite](../../examples/star-harbor/assets/ship.png) are included. With
the G68 tools and pinned LLVM available, run these from a parent directory.
Set `demo` to the absolute path of the supplied example directory.

```sh
voidc new app --name StarHarbor
cd StarHarbor
voidc add raylib
cp "$demo/app.void" src/app.void
mkdir -p assets
cp "$demo/assets/ship.png" assets/
voidc tidy --check
voidc check .
voidc run .
voidc build . --reuse-native-approval
./build/app
code .
```

The first bundled add needs Python 3.12+ on PATH and HTTPS access to the
pinned raylib release. The game uses VOID, raylib and its PNG. In the terminal,
review the native provider and type `approve`. Arrow keys move the ship and
Escape closes the game. Open the game folder as a trusted workspace. In
`src/app.void`, put a breakpoint at `var x: i32 = 296`, press F5, then step
and continue. The generated launch uses LLDB DAP and builds the debug program.

The real Linux window drew a texture uploaded from the on-disk 48×48 PNG.
Keyboard input visibly moved it in the CLI, independent executable, VS Code Run,
F5 and relocated delivery tests. The image owner closes after upload. The
GPU texture closes before the window, and raylib logged the texture and
window cleanup. The desktop tests captured the installed compiler and server
processes. They also exercised project creation, add/remove library,
diagnostics, Run, Build, F5 and Restricted Mode with the installed pair.

CLI JIT and plain executables use the caller's working directory. From the
game folder `assets/ship.png` opened. A JIT launch from the parent, even with
`--project`, and a launch from `build/` both reported the missing asset and
exited 1. VS Code Run, Build and F5 use the selected project root. The
same terminal approval was reused by Run, Build and F5. In a separate test
G68 rejected reuse of a G67 compiler's approval for the same provider
and unchanged lock. A fresh terminal review is needed after that upgrade.

For delivery, package the program into an inner verified directory and ship
the `assets/` directory beside it. Run from the outer directory.

```sh
mkdir -p ../StarHarbor-delivery
voidc package --project . --profile release --allow-native -o ../StarHarbor-delivery/engine
voidc package --verify ../StarHarbor-delivery/engine
cp -R assets ../StarHarbor-delivery/
cd ../StarHarbor-delivery
./engine/app
```

`--allow-native` explicitly authorizes the reviewed provider for this
packaging invocation. Saved approval reuse applies to Run and Build.
The package includes the native provider, raylib and its license. Its
inventory does not include the adjacent sprite. The shipped sprite needs its
own delivery inventory if distribution requires asset integrity.

## Release gaps ranked by launch impact

This updates the [G29 release-readiness audit](../release/G29_FIRST_RELEASE_READINESS.md).
The G29 missing host dependency setup is already present in the current
installation smoke recipe. G70 used the qualified component install on the
owner's host. A cold cross-platform install replay remains unqualified.

| Priority | Area and evidence | Next concrete step |
| --- | --- | --- |
| 1 | **Verified binary compatibility failure.** The local compiler failed before entry in Ubuntu 24.04 on `GLIBCXX_3.4.35` and glibc 2.42–2.44 requirements. The packaged game also failed there on `GLIBCXX_3.4.35` and `GLIBC_2.42`. | Choose the Linux ABI floor and build the exact toolchain and reference game there. Verify the compiler, server, linker and relocated game in that userspace before advertising binaries. |
| 2 | **Production VOIDUP unqualified.** Private signed install, verification, activation and recovery passed. The current bootstrap embeds engineering trust roots. | Prepare a release bootstrap with owner-controlled production trust roots, key custody and rotation, a release endpoint, and signed payload and rollback qualification. Review redistribution for the exact runtime and SDK bytes. |
| 3 | **Verified public GitHub acquisition failure for the tested source.** Fetching the G68 public monorepo's raylib subtree exceeded the HTTPS Git transport's private storage limit. The project manifest stayed byte-identical and no lock or dependency was published. | Qualify a small public VOID library at an immutable revision. Keep the transport bound and document the rejected monorepo case. |
| 4 | **Editor and install coupling.** The local resource, notice and VSIX checks passed. The desktop initially lacked a usable tool path, and the generic VSIX needs a separate matching compiler/server. Existing G67 native approval was correctly refused after changing compiler builds. | Publish a matching source and VSIX version with tool-path, adapter and reapproval steps. Test fresh VS Code and VSCodium installs for the chosen release. |
| 5 | **Application assets.** The checked package did not copy the PNG. The sibling asset layout ran on the owner's host. | Document the portable folder contract for the first demo. Add a declared, verified asset inventory if the release requires one. |

No new Windows or macOS acceptance is claimed. The prior G68 Windows
qualification and debugger exit-status discrepancy still apply. The public
GitHub failure is for the specific monorepo revision tested. The existing
HTTPS fixture covers authenticated acquisition on a bounded repository.
The known upstream raylib device leaks remain visible in the G68 evidence.
