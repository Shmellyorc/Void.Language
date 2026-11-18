# G67 — VS Code library management

Accepted baseline: `890494b032f23da1b451ba98db5f739384480315` (G66).
Candidate: `codex/g67-vscode-library-management`. Recommendation: **INTEGRATE**.

In a trusted local folder, create and open a VOID application with
**VOID: New Project**, save your work, and run **VOID: Add Library**.
The picker lists compiler bundled and verified registered packages,
plus **From local folder** and **From GitHub**. A local library's
admitted package name supplies the alias when it is portable. The
GitHub picker accepts a repository or subtree URL and asks which alias
to use. Write `use pkg/ALIAS` explicitly. **VOID: Remove Library** shows
direct dependencies and asks for confirmation. For the complete
workflow, see the [editor guide](../EDITOR_INTEGRATION.md#adding-and-removing-libraries).

## Compiler and editor contract

- Schema 1 `voidc package list --json` provides catalog identity and source
  kind. `voidc add --suggest-alias --dir DIR` returns a schema 1 alias or null
  from compiler admitted manifest metadata. Existing human listing and
  package registration remain separate from application dependency edits.
- `voidc deps explain --json --for-edit --project DIR` verifies the saved
  manifest and lock, lists direct relationships and returns an edit token.
  The token binds the physical directory, exact manifest and optional
  lock. The compiler checks it on the existing `voidc add` and `voidc remove`
  paths. Registered selections also bind the verified registration receipt;
  the bundled choice is explicit even if a registration has the same alias.
- The editor checks trust, the selected local workspace/project and
  compiler settings, and rejects dirty project buffers. The language server
  distinguishes an unavailable pinned root from source errors, including
  unresolved imports. This lets the compiler reject a replaced project
  without blocking a valid fix for a missing import.
- The local picker uses the compiler's opt-in `--copy` so the installed view
  is under `libs/ALIAS` even if the selected source is inside the project.
  The normal CLI can still link a local library in place. Removal retains
  editable local source under the existing rules; verified registered and
  Git views use the existing removal checks.
- The editor invokes the compiler with literal arguments and no consent
  input. Only the compiler handles acquisition, package identity, native
  preparation and filesystem publication. The editor reselects the new
  dependency context in the existing language server, preserving the
  entry override, and runs read-only `voidc tidy --check`. It never edits
  developer source or creates or changes a native approval.

## Qualification

The focused Linux Release and fatal ASan, UBSan and LeakSanitizer suites
passed 10/10 each. A separate replay of project creation, root recovery,
editor recovery and quick fixes on the final language server passed 4/4
under both profiles. The configuration suite passed 8/8.

Real VS Code 1.141.0 on Linux created and debugged the starter, rejected a
failed prelaunch build, and verified breakpoint, step and scalar
inspection. In that editor-created project the library suite checked all
four add sources, exact lock and installed views, direct removal,
diagnostics before and after a dependency change, completion, hover,
definition, Run and Build. It also checked cancellation, dirty files,
saved manifest changes while a picker was open, invalid input, failed
acquisition and Restricted Mode. Bundled raylib installation left the
approval store unchanged and refused approval reuse. The separate G65
desktop replay with the ordinary G67 compiler exercised Run, Build and
F5 before and after explicit terminal approval, catalog tamper and
revocation. The approved app wrote the expected raylib pixel value 241.

Native Windows 10 x64 build 19045 passed the final focused suite 10/10
in Debug and Release. Both compiler and server retained the 1 MiB stack
reserve. Real VS Code 1.141.0 with Microsoft C/C++ 1.35.6 and GDB 17.2
passed creation, the trusted library workflow, Restricted Mode and
F5, including the failing prelaunch build, a source breakpoint, a
step, and scalar inspection. The Windows editor used the ordinary
compiler and checked GitHub input rejection; the positive GitHub URL
was exercised in the Linux fixture. As in G66, Windows' adapter
reported exit 21 for the starter that independently returned 17.

The G60 warm startup comparison reused the prepared raylib workload,
alternated the candidate and baseline on CPU 7, and measured eight
pairs per profile. Median paired first output was +1.4% in the default
profile and −2.0% with O2; wall time was +1.2% and −1.9%, respectively.
The ranges overlapped. G67 introduces no new work in hot Run or Build.

## Coverage limits

The positive editor GitHub test used the existing smart HTTPS fixture
and a test compiler linked with one origin routed to loopback. The
compiler processed the GitHub slash-ref subtree URL and locked the exact
fixture commit. A missing remote ref left the manifest, lock and views
unchanged. This is not a positive fetch from a public GitHub repository.
The ordinary compiler separately passed the HTTPS transport suite.

## Patch inventory

- Editor commands and workflow: `editors/vscode/package.json`,
  `editors/vscode/src/extension.ts`, `editors/vscode/src/libraryManagement.ts`.
- Compiler and server: `include/void/project_resolution.hpp`,
  `src/frontend/project_resolution.cpp`, `src/main.cpp`, `src/lsp/main.cpp`.
- Qualification tests: `editors/vscode/src/test/libraryHost.ts`,
  `tests/lsp/project_root_recovery.py`, `tests/project/git_https.py`,
  `tests/project/simple_packages.py`.
- Documentation: this report, `docs/EDITOR_INTEGRATION.md` and
  `editors/vscode/README.md`.
