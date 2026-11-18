# G66 — VS Code project template creation

Baseline `a1dd73d8cbabf92522c74ec933aec2c975667a25` (G65).
Candidate branch `codex/g66-vscode-project-creation`.
Recommendation **INTEGRATE**.

In a trusted VS Code window with one local folder open, run **VOID: New
Project**, pick **Console Application** (`app`), enter `MyGame`, and select
the parent folder. **Open Project** offers to open the result in a new
window. Choose **VOID Library** (`classlib`, also listed as `lib`) for a
library. The list includes installed templates with their tags and all short
names. For example, after `voidc new install --dir PATH`, choose the installed
template by its name or type one of its aliases in the picker.

Both built-ins support an explicitly selected empty folder. An in-place
library uses that folder's base name for the package, following G62's CLI
starter behavior. Installed templates create a new named subfolder through
their existing atomic directory publication. Project names accepted by the
picker follow the compiler's portable template identifier rule. **VOID:
Create Project** and `voidc init` still adopt existing source; standalone
execution needs no manifest.

## Compiler and editor contract

- `voidc new list --json` verifies the existing store and returns schema 1.
  Each template supplies its name, short names, tags, and in-place support.
  A broken registration fails discovery before any JSON list is emitted. The
  existing human-readable list remains available.
- `voidc new --template SHORT-NAME --name NAME [--in-place]` reuses the
  built-in creation plan or G62's registered-template instantiation and
  publication. It selects the current folder as the parent, or the exact
  empty folder in built-in in-place mode. Successful publication returns
  schema 1 with the absolute project root. A removed template fails
  explicitly, without falling through into legacy path creation.
- The extension uses VS Code Quick Pick, Input Box, the folder picker and
  the open-folder command. It resolves the compiler through the existing
  settings, checks workspace trust, and verifies the selected workspace
  and executable settings again before starting creation. It passes
  literal arguments and reads only the compiler's structured results.
  Manifest and identity generation, path admission, case and collision
  checks, store protection and staged publication stay in the compiler.
- The existing app starter still provides the qualified F5 configuration.
  A classlib has no application launch files. Custom templates retain
  their authored structure and binary assets. Templates still cannot
  install dependencies or opt into native execution as part of creation.
  The extension invokes neither template-authored scripts nor approval
  commands.

The editor guide describes the command step by step. The UI design follows
the [VS Code Quick Pick and file-picker API](https://code.visualstudio.com/api/extension-capabilities/common-capabilities),
[Workspace Trust](https://code.visualstudio.com/api/extension-guides/workspace-trust),
and the [open-folder command](https://code.visualstudio.com/api/references/commands).

## Qualification

The focused CLI and compiler suites cover project creation, creation
processes, profiles, native debug information, templates, and packages and
native approval. The Linux suites include project sessions. The template
suite adds schema, strict selection, disappearance, alias, empty folder,
substitution, binary asset, collision and tamper checks. A separate Linux
permission check denied publication in a read-only destination and left it
empty. A separate in-place rejection kept the original bytes of a
developer-owned binary file and added no files. Linux Release and fatal
ASan, UBSan and LeakSanitizer passed 7/7 each. Windows 10 x64 build
19045 passed 6/6 in both Debug and Release, with 1 MiB stack checks.
Both hosts used pinned LLVM/Clang/LLD 23.1.4. The VS Code configuration
tests passed 8/8.

Actual VS Code 1.141.0 on Linux and native Windows drove Quick Pick,
Input Box, the simple folder dialog, notifications and the generated
launch configuration.
The trusted case listed built-in and installed metadata, cancelled at the
folder dialog without creating files, created and ran an app, created a
library in an explicitly selected empty folder while the earlier Open
Project offer was pending, and substituted an installed template.
It checked fresh IDs, source paths, binary assets, collision preservation,
invalid names, tampered registration, and the absence of native approval.
The Restricted Mode case checked that even direct command invocation
produced no compiler launch and no filesystem writes.

F5 built and launched the same app produced by the editor command with
LLVM LLDB DAP on Linux and Microsoft C/C++ 1.35.6 with GDB 17.2 on
Windows. Both hosts verified that a failed build blocked launch and
exercised a source breakpoint, a step, and scalar inspection. The
Windows adapter reported exit 21 for the updated starter, which
independently returned 17, matching the G64/G65 limitation. The separate
G65 raylib replay with the final Linux editor bundle also passed Run,
Build and F5 and blocked missing, changed and revoked approval.

The first compiler test caught a regression in the exact human help line
for `voidc new`. The protocol usage now has a separate help line and the
legacy line is preserved. Early desktop test runs also showed that the
folder widget needs time to refresh after navigating into an empty
folder. The final driver waits for that refresh and clicks the widget's
Select Folder button. The final editor code releases the active creation
state before the user responds to a success or error notification, so the
next project can be created while the earlier offer remains available.

The test verifies that the Open Project action is offered and that the
original window stays on its workspace while the offer is pending.
Opening a generated folder uses VS Code's ordinary Workspace Trust
rules. G66 executes no template-authored script during creation.
