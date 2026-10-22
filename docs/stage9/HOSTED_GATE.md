# Stage 9 owner-controlled native gate

> Windows update, 21 October 2026: **CI #95 accepted Debug 160/160 and Release
> 160/160 (320/320)** at `e25816bdb80e3f32613edf77c9cf3258232af966`.
> The blocking Windows investigation is **RESOLVED**; see the
> [permanent Windows record](WINDOWS_ENGINEERING_RECORD.md). The original snapshot/
> broader gate below remains historical; no complete seven-job matrix or actual
> second-volume pass is claimed here. Further CI remains Boss-controlled.

21 October 2026. This is a requirement, not authorization to start hosted CI.
Linux local results cannot establish Windows/macOS success or a hosted lock.
The existing seven-job workflow and CI #87 path override remain intact.

Require all seven complete builds and unfiltered CTest passes on the exact
reviewed commit: Ubuntu Debug/Release/Sanitizers, Windows Debug/Release and
macOS ARM64 Debug/Release. The new `application_workflow_stage9` suite runs real
native executables in every configuration. Preserve strict ASan/UBSan/leaks and
Linux Debug/Release 1 MiB frontend gates, prior local-library/discovery/alias
regressions, extension units and actual editor integration.

Windows must execute automatic application/legacy modes, default `.exe` spelling,
private install relocation, Unicode/spaces/#/% and >260-character physical paths,
source/import/hard-link collisions, failed compile/link preservation, and
replacement while the old executable runs (failure must preserve it; successful
OS-supported replacement must execute the new artifact). Retain SDK/MSVC/UCRT
link environment, native UTF-8 conversions, drive-letter equivalence and existing
reparse/8.3/long-name descendant rejection. Report whether optional symlink
creation is available; other source/output safety assertions remain mandatory.

macOS must execute the same linked workflow with the official 19.1.7 host driver,
configured SDK/architecture and CRT stderr symbol. Verify ordinary and
case-sensitive APFS semantics: wrong-case sources reject on either filesystem;
wrong-case existing output aliases reject on insensitive volumes, while distinct
spellings can create distinct outputs on sensitive volumes. Retain /var versus
/private/var workspace ancestor equivalence and descendant alias rejection.
The default macOS runner alone does not prove case-sensitive APFS execution;
Boss's native verification must include an actual case-sensitive volume.

The real staging observation must show intermediates under the selected physical
output parent containing `%`, and the produced executable must run. Do not replace
this with command inspection or an unrelated clean output directory. Verify the
private installed bin/config/lib layout after relocation, retaining the configured
host LLVM/Clang/platform SDK dependency; no clean-machine SDK bundle is claimed.

Review trusted/untrusted/multi-root real VS Code profiles, supported Neovim/Vim
adapters, local libraries, unsaved overlays, UTF-16, recovery, navigation,
completion/documentation and clean shutdown. VSCodium/new client versions remain
unverified until actually executed. Record native results, versions, filesystem
properties, log hashes and failures without counting cancelled/unexecuted jobs.

Only Boss controls commits, pushes, branches and hosted runs. Wrench reviews the
complete local handoff before Boss decides those actions. No root README change,
publication, package manager, downloads, generalized FFI or cross compilation is
part of this gate.
