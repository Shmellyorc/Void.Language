# G68 raylib community reference package

Accepted parent `6e1035d06e1021bc07de00ff5125e687bdcca3c6` (G67).
Candidate `codex/g68-raylib-community-package`.

The [raylib README](../../verified-packages/raylib/README.md) is the developer
entry point. It covers the manifest, CLI and VS Code installation, an image
example, native approval, local edits and publication by immutable Git commit.
The collection README and native library guide lead to that workflow.

## Package audit

The tracked library tree changed from 22 files to 17. The checksum inventories
in the evidence archive enumerate each file before and after.

| G67 library inventory | G68 inventory and classification |
| --- | --- |
| `.gitattributes` | Distribution metadata. Retained to preserve the exact upstream header and license during checkout. |
| `.gitignore` | Development metadata. Retained for local preparation outputs and temporary files. |
| `README.md` | Developer documentation. Rewritten around current app, package and approval commands. Removed the speculative standalone URL and obsolete deployment instructions. |
| `OWNERSHIP.md` | Developer reference. Retained the ownership, borrowing, callback and device limits and updated historical terminology. |
| `void.project.toml` | Required source metadata. Unchanged project and package IDs, version and entry. |
| `src/raylib.void` | Required public source. Updated comments only. |
| `src/raylib_native.void` | Required physical native declarations. Exact bytes retained. |
| `native/raylib.descriptor.in` | Required native descriptor template. Exact bytes retained. |
| `native/raylib_api.descriptor.inc` | Required checked signature and layout input. Exact bytes retained. |
| `native/raylib_api.c` and `native/raylib_api.h` | Required C adapters. Exact bytes retained. |
| `native/raylib_provider.c` and `native/raylib_provider.h` | Required native provider. Updated an example trace message. Header and provider operations retained. |
| `native/raylib_callbacks.inc` | Required callback routing. Updated historical comments. |
| `native/raylib_parity.h` | Required C prototype and structure assertions. Exact bytes retained. |
| `upstream/raylib.h` and `upstream/LICENSE` | Required pinned upstream input and notice. Exact bytes retained. |
| `prepare.py` | Required compiler preparation tool. Moved to `verified-packages/raylib-prepare.py` and installed beside the package as a separate compiler resource. |
| `generate.py`, `callback_bindings.py` and `owned_bindings.py` | Development tooling. Moved to `tools/raylib/` and excluded from compiler and consumer package inventories. |
| `inventory.json` | Development verification record. Moved to `tools/raylib/` with the generator. The 600 tuples of native declaration, VOID name and category match G67. |

The generated `SOURCE_ORIGIN.txt` still accompanies the shipped library.
The native catalog still carries the prepared adapter, official runtime
library, Windows import library where required, and the upstream license.
The maintained gallery example and test still use explicit preparation via
the relocated tool. Historical records under `docs/` were retained.

## Compatibility and trust boundary

CMake records the exact shipped file inventory and hashes the source,
provenance receipt and separate preparer. Bundled addition admits that
inventory and checks the digest of the captured bytes. Newly prepared catalogs
use a private copy of the verified recipe and sources. Stale tools left in
an upgraded compiler prefix are excluded by the inventory. Git and local
package acquisition do not select or execute that recipe. The existing
package transaction publishes the verified source and catalog and writes
the lock. Python runs in isolated mode during preparation. The recipe
still checks both official platform releases against their exact sizes and
SHA-256 hashes before reading the pinned archive members.

Existing G67 installations keep their locked source and catalog, including
the earlier inventory. A copied G67 project resolved, checked, ran, and
reused its catalog with its original lock bytes. A fresh G68 addition has a
different source and artifact identity. Exact restoration of a missing view
requires the compiler resources and toolchain that produced that lock.
Use the existing terminal review when a replacement changes the approved
lock or native identity. The package identities, 600 interfaces, wire ABI,
native approval system and package resolution rules remain compatible.

## Python scope

Python 3.12 or newer is still required for the first bundled
`voidc add raylib` and when recreating its native catalog. Normal
imports, checks, JIT and AOT reuse the installed provider without it.
The existing declarative C recipe accepts a self-contained provider. It
cannot fetch and verify two pinned external release archives, extract their
named binaries or link an extra library. Replacing that pipeline requires
a separately qualified compiler-owned path for the two bounded HTTPS
assets and their native artifacts. Keep that follow-up specific to compiler
resources and require exact catalog restoration on both hosts.

## Qualification

A native x86-64 Linux Release build and a fatal ASan/UBSan build with leak
detection each passed all seven focused tests, including the smart HTTPS
fixture and generated binding parity. The bundled workflow runs the exact
README sample in a new app, checks JIT and
AOT without Python on PATH, and exercises approval, revocation, lock and
descriptor tampering, catalog reuse and missing-view restoration. The
generated bindings compile against the target header and the symbol test
links and resolves all 600 native interfaces. Separate tests cover
typed callbacks, resource cleanup, relocation and physical declaration and
provider errors.

The Linux VS Code 1.141.0 host verified project creation, Run, Build and F5,
G67 add/remove and import diagnostics, and rejection in Restricted Mode.
Bundled raylib installation preserved the source files and native approval
store. A raylib project ran under F5 with LLDB; the debuggee wrote a pixel
result of 241 and exited 17. Run, Build and F5 all required terminal approval
and refused missing, changed and revoked approvals.

Native Windows x64 Debug and Release builds each passed seven focused
tests and the separate smart HTTPS fixture. These checks include the
generated binding inventory, package transactions, project templates and
creation, build profiles and native debug information. Both builds checked
the README example, restored the original lock and catalog, and compiled,
linked and resolved all 600 native interfaces. Release also passed the
typed callback and resource cleanup checks.

The Windows VS Code 1.141.0 host exercised project creation, Run, Build,
F5 and G67 library management, including diagnostics, cancellation and
Restricted Mode. The native raylib project reused exact terminal approval
and rejected missing, changed and revoked approvals. The debuggee wrote
241. The Windows C/C++ 1.35.6 / GDB 17.2 adapter reported exit 0 in the
final native replay and 21 in an earlier replay. Independent execution
returned 17 in both. The final starter app replay also reported adapter
exit 21 for independent exit 17.

A clean CMake install ran in a Linux mount namespace with both the source
checkout and configured build tree hidden. Changed preparer, source and
receipt bytes failed before publication. A stray package-side script was
ignored. The compiler ran the README sample through JIT and AOT after
discovery, addition, lock verification and tidy. That isolated run used
the explicit CLI native allowance. Terminal approval and VS Code approval
reuse were exercised separately. The library starter, local add and import
commands in the README also passed.

The independent Linux native C controls reproduced all three unsuppressed
device leaks after terminal release. Gallium graphics reported 256 bytes,
the WAV music context 408 bytes and sound-alias conversion 64 bytes.
These device results are outside the passing headless sanitizer suite.

Both targets fetched and verified the pinned official raylib release asset.
Git package acquisition qualification uses the smart HTTPS fixture. A
successful fetch of a public VOID package remains unqualified.

## Recommendation

**INTEGRATE.** Keep first-install Python as the documented prerequisite
until the compiler can prepare and restore the pinned catalogs on both
targets without it.
