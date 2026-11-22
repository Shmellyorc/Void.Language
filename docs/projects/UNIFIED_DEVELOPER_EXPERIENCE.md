# Unified developer experience correction

21 November 2026. Ordinary corrective engineering; no new milestone.

Filesystem compatibility and live editor package discovery are reviewed together.
The actual Linux development prefix is updated, with matching standard-library,
Runtime, native-link receipts and bundled package preparation resources. At
engineering acceptance no Git staging, commit, push, hosted CI or publication
was authorized or performed. Subsequent scoped Git authorization is recorded below.

## Root causes and corrections

Ordinary project/storage operations incorrectly reused confidentiality admission
rules (owner identity and restrictive permission bits). Existing permission work
replaces those arbitrary checks with OS access and held path/type identities.
New ordinary directories/files use 0777/0666 through umask and inherited ACLs;
existing directories are not chmod-repaired. Explicit private capabilities,
release authentication, native consent and signing-key confidentiality remain.
Project-local `.void` storage, G95 authenticated compilation caching, native
staging leases, verified contents and bounded recovery are retained. Cache HMAC
credentials retain their separate private per-user leaf and fallback; no secret
is included in review evidence.

Editor failures had three causes: cached standalone selection survived manifest
creation/package installation; incomplete `for` conditions lacked the existing
`if` member-hole recovery; manifest graph validation omitted qualified type-probe
context. The existing correction watches manifest/lock changes and invokes the
compiler-owned discovery/reselect path, shares condition recovery between `if`
and `for`, and attaches the editor context at common graph validation. Ordinary
compiler parsing still rejects incomplete code. No editor-owned type system or
second project resolver is added.

Independent review found and corrected additional integration defects:

- Installed executables had been updated without matching installed `io.void`
  and `libvoid_runtime.a`. The new API failed with an unknown-function error.
  Complete compiler/tooling component installation now updates the actual
  prefix, native-resource hashes and raylib preparer together.
- Atomic replacement preserved existing named ACLs but failed to remove an ACL
  inherited by a replacement stage when the original had no extended ACL.
  The Linux reproduction succeeded in saving while changing access permissions.
  The correction explicitly removes the stage ACL in that case and applies the
  original mode after ACL transfer. macOS uses an empty ACL for absent input ACLs.
- Windows native output staging had been changed to inherit ordinary shared
  access. Invocation staging is private again. Final files derive ordinary access
  from their real output parent and retain a handle through ACL publication and
  rename. New POSIX native leaves check ownership/mode/identity; macOS clears
  inherited ACLs only on those exclusively created leaves.

The ACL correction follows Linux [ACL inheritance semantics](https://man7.org/linux/man-pages/man5/acl.5.html)
and descriptor-based [attribute removal](https://man7.org/linux/man-pages/man2/removexattr.2.html).
Windows final output uses documented [security descriptor inheritance](https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-createprivateobjectsecurityex)
and [rename through a held file handle](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfileinformationbyhandle).
These references support implementation choices, not native platform qualification.

## Verification and installation

Current run evidence and exact commands are consolidated in the reviewer packet.
Focused checks cover filesystem creation/access, atomic save/ACLs, package
maintenance, managed native staging, compiler authority, automatic/debug cache,
retention/concurrency/native-content invalidation, and cold/warm loop/type
completion. Installed-tool checks use 0755, 0775 and setgid 02775 fixtures and the
real `/home/shmellyorc/Desktop/Ninja` with a temporary explicit headless entry.
The entry/output are removed; the test asserts unchanged source/manifest/lock
bytes and permission modes across its invocation.

Ninja's saved source changed during this task through Boss's editing; the initial
session snapshot is not claimed to match the final source. Tests never edit it.
The read-only editor test uses its current `for not rl.` member position; exact
incomplete `for rl.` is checked in the generated project and compiler regressions.

The installed-editor harness uses `/usr/bin/code`, the real default Code user
data/profile and the existing installed extension directory. The development-host
flag points at that actual installed directory, never the source extension or a
copied prefix. Trust is disabled and other extensions are disabled for the test
window through launch flags; user settings files are not changed. Boss closed
Code for the test because the launcher refuses extension tests while another
instance runs. Generated test windows close between live/reopen phases. These
are actual installations; the earlier isolated 1.141 editor-host run is historical
supplemental evidence only.

The harness tests manifest creation and `voidc add raylib` while a standalone
source is already open, `rl.`, `for rl.`, `as rl.`, normal incomplete-code errors,
repaired diagnostics, semantic tokens, CLI check/JIT/AOT and warm cache hits.
Removal refreshes an already-open manifest; explicit bundled re-add restores
completion without restarting the session. A second editor launch checks cold
project startup. Ninja is queried read-only in another installed-editor launch.

Original unsuccessful harness runs remain in the packet: a timing output path
was incorrectly placed inside `.void`; ordinary removal intentionally retained
`libs/raylib`, so bare re-add correctly refused an overwrite; a local-source
re-add correctly granted no native-provider authority. The final harness writes
fresh timing files outside the project, preserves the removed copy separately,
and explicitly selects the bundled native provider. None of those refusals was
weakened to make the test pass.

Actual paths:

- Compiler/server: `/home/shmellyorc/.local/bin/voidc` and `voidls`.
- Runtime: `/home/shmellyorc/.local/lib/void/libvoid_runtime.a`.
- Standard library and preparer: `/home/shmellyorc/.local/share/void/stdlib/`
  and `share/void/verified-packages/raylib-prepare.void`.
- Extension: `/home/shmellyorc/.vscode/extensions/void-language.void-language-0.1.2`.
- VS Code: 1.144.0, commit `2c33b50b16c472efe58357d1d2a8a5414bc23fbf`.
- Compiler: LLVM/Clang/LLD 23.1.4, Linux x86-64, Release.

`identities.json` records complete SHA-256 values, compiler version/build identity,
ELF dynamic sections and matching server/extension/Runtime identities. CMake
rewrites compiler RUNPATH from the build provider path to `$ORIGIN/../lib/void`;
therefore installed/compiler build SHA-256 values intentionally differ. Both
report the same compiler build identity. Activation logs identify the actual
extension and selected language-server path; tooling/project protocol checks
exercise pairing. `voidls --version` is not a supported command.

Before replacement, `recovery/` captured both tools, the extension, `lib/void`,
`share/void` and native inventories with a per-file SHA-256 manifest. Restore
those saved paths to their corresponding prefix/extension locations and reload
Code once to recover the pre-task installation. The recovery copy is local and
separate from the source/review ZIP. It contains no approval/cache credentials.
The permanent build/install rule is recorded in
[engineering instructions](../WRENCH_CLANKER.md#permanent-development-installation-rule--21-november-2026).

## Source inventory, evidence and independent review

The reviewer packet contains the exact union inventory, complete source copies,
HEAD-to-current `review.patch`, original permission report/patch, original failure
logs, current checks, installed identities, this report and independent findings.
`integration-only.patch` reconstructs the permission baseline from its saved
patch, verifies its SHA-256 against this task's initial inventory, then records
only this integration task's changes. Permission and editor inventories overlap
in CMake test registration; full HEAD diffs also contain explicitly marked
preexisting work. They are not a staging manifest.

The permission group has 37 original task-touched files. The editor group covers
common frontend validation, extension metadata watching, maintained regression
fixtures/tests, package test registration and editor documentation. Integration
adds ACL/native staging fixes, installed-host tests, permanent installation
instructions and this consolidated report. Unrelated G96/VOIDUP, F5/output-layout
and release-publishing changes remain separate and unstaged; full overlapping
file diffs must not be attributed entirely to this task.

Independent source/evidence review is performed by a separate reviewer agent in
the Sacred Wrench review role. Its findings and final disposition are retained
verbatim in `independent-review.txt`. This does not impersonate a separate
preexisting Sacred Wrench chat or Boss's later Git authorization. The portable ZIP
can be given to that reviewer without access to this workstation. The reviewer
agent has direct access to all packet sources/logs, not only a summary of paths.

Windows and macOS native execution remain **UNQUALIFIED**. Linux tests do not
qualify Windows DACL/handle rename or macOS inherited ACL behavior. No real
second-account cache test is claimed; existing simulated ownership/HMAC tests
remain scoped evidence. No release or whole-suite portability claim is made.

## Final Linux results

- **16/16** focused CTests pass on final source; **14/14** extension unit tests
  pass. The rebuilt frontend suite also passes (previous editor correction
  evidence is retained).
- **48 installed-tool filesystem/Ninja calls** pass, including the new absent-ACL
  regression, ordinary modes, named ACL preservation, symlink refusal and
  headless raylib check/JIT/build in Ninja. The installed ACL reproduction
  independently changes from FAIL to PASS.
- Actual installed Code **live**, **reopen** and **Ninja** phases all pass.
  The generated project returns **871** suggestions for `rl.` and `for rl.`,
  **69** types for `as rl.`, semantic tokens and correct diagnostics.
  JIT cold miss/warm hit and the emitted AOT executable pass. Removal/bundled
  re-add returns the same 871 loop suggestions in the existing session.
  Ninja returns 871/69/871 suggestions at `rl.`, `as rl.`, `for not rl.`.
- Activation logs observe the installed extension and actual installed server;
  the live phase switches from `--entry` to `--project` after manifest creation.
  Package changes afterward use live reselect and require no manual restart.
  The normal Ninja window is reopened with the installed extension afterward.
- Final compiler build identity:
  `b1e6c749a89bef22e16bf701f33a4ff7e4028dec8926538bcf365b61f351c722`.
  Installed compiler SHA-256:
  `3f99311acc5441868610219a4a002e3f20979d443d95d5bcb3e83d21989135ba`.
  Server SHA-256:
  `cf0a62dfde1be87eafc0f419c3bcfadcfa5becf04840d5c8642575486a729167`.
  Extension JavaScript SHA-256:
  `08177a48f03734fb068ebdb1d05f3e64d3381c846d51f790c315a2b384887c25`.

The raw pre-correction editor failure transcript was not present in the retained
editor evidence folder. Its root-cause account and regression patch are preserved;
no reconstructed transcript is presented as original evidence. Original Linux ACL
failure output and all unsuccessful current harness runs are retained.

Independent final disposition is in the packet's `independent-review.txt`.

## Authorized Git integration

Boss separately authorized scoped staging, commit and normal push on 21 November
2026. All 54 accepted source identities and the installed compiler, server,
extension, Runtime, standard library and preparer still matched the packet at
integration start. `main` and `origin/main` both pointed to
`1080440953847338807d5005d162ad34841de701`; G95 history is retained.

The reviewed 54-file union is an evidence inventory, not a blanket staging list.
The scoped candidate includes 52 of those files plus the storage-policy correction
above, for **53 committed files**. Only permission-related directory type changes
are selected in `apps/voidup/manager.void` and `support.void`; G96 behavior stays
local. The documentation index includes only this correction's link. The extension
includes metadata refresh but leaves its unrelated F5 output-path hunk local.
The preexisting untracked G96 `apps/voidup/launcher.cpp` and
`tests/voidup/first_run.py` remain intact and uncommitted, including their reviewed
permission updates. Neither file can be partially introduced as a standalone
correction before its owning installer change is integrated.

The Git integration receipts retain the exact candidate patch/inventory, scoped
independent review, focused checks, final commit/push identities and preservation
checks. Existing Linux qualification is reused for unchanged implementation;
the isolated VOIDUP and editor projections receive only focused consistency
checks. No release or manual hosted-CI dispatch is authorized by this push.
