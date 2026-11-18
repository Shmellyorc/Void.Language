# G63 — Durable native package approval

Baseline: G62 library-template repair `9b93d0d839d7b362ff840731896142472138f39e`.
Candidate: `codex/g63-durable-native-approval`.

## CLI

On a supported host, from an application project in a terminal:

```sh
voidc add raylib
voidc run .
# Review the application, provider, target/ABI and approval ID.
# Type exactly: approve
voidc run .
voidc build .
voidc native approvals
voidc native revoke APPROVAL_ID
```

A first `build` in a terminal can also request consent. Any other response or
end of input declines. Later terminal runs and builds reuse the matching
approval, including across `voidc` restarts. A terminal can still decline a
changed identity. Earlier approvals remain separately inspectable and
revocable by their full SHA-256 ID; restoring the exact earlier identity can
use its approval until it is revoked. Revocation affects subsequent
invocations. It cannot stop an already authorized process or revoke a native
executable already built.

For deliberate automation, output redirection, or a conventional provider
without a managed catalog:

```sh
voidc run . --allow-native
voidc build . --allow-native
```

`--allow-native` authorizes that invocation through the existing path and
never creates a durable record. `emit` and deployment `package` still
require the explicit flag for native projects. `check`, package install,
dependency acquisition, and template registration cannot grant consent.
A real terminal on stdin and stderr is required to create **or use** a
durable approval. In CI (`CI` set), even an attached terminal cannot use it.
Sending `approve` through a pipe cannot grant consent.

## Identity and verification

- Approval is an application/provider pair, not a package name. A
  domain-separated SHA-256 over length-prefixed fields binds the physical
  project path and directory identity, project and application IDs, selected
  entry, exact verified `void.lock` bytes (by SHA-256), exact catalog bytes
  (by SHA-256), and the compiler build identity.
- The admitted native closure supplies the exact descriptor, native
  directory, runtime and notice hashes and sizes, library/adapter names,
  target, and transport. The ABI revision and exact physical binding
  identity and bytes participate in the digest. The lock also covers
  link inputs, the transitive source package graph, native source, and
  relevant manifest identities. Ordinary application-body edits can reuse
  approval if this identity is unchanged and the new code passes analysis.
- The compiler uses G61/G62's locked dependency resolver, managed catalog
  verifier, and physical binding/type checks before consulting consent.
  Its normal fresh capture verifies those inputs again before lowering;
  G63 compares the exact catalog digest and directory identity on that
  capture as well. A changed file while the question is open cannot
  authorize new bytes. A new record is published only once that capture
  and CLI entry validation pass.
- For a durable JIT run, the CLI copies the exact verified runtime payload
  into a private directory and passes that path to the existing loader.
  The Runtime still checks the payload; it loads the private copy rather
  than re-opening the project's native files after comparison. The CLI
  keeps the copy alive through JIT use and shutdown. Linux uses an
  exclusively created directory below a checked `/tmp`. Windows reuses
  the existing private native staging directory and directory pins.
  The existing AOT path already stages and verifies its managed payload
  and link inputs. Explicit `--allow-native` keeps the established
  execution and build path.
- A record is one versioned, bounded file inside an ID-named directory.
  The existing project filesystem admission, writer lock and atomic
  directory publication/removal are reused. Approval paths reject aliases,
  unsafe ownership/permissions and project/provider overlap; normal CLI
  project and template creation cannot publish over the store. An approval
  file must be a regular, singly linked file whose bytes match the expected
  record. Malformed or unavailable approval data cannot authorize execution.
- Locations are `%LOCALAPPDATA%\VOID\native-approvals`,
  `$XDG_DATA_HOME/void/native-approvals` (otherwise
  `~/.local/share/void/native-approvals`), and
  `~/Library/Application Support/VOID/native-approvals` on macOS. G63
  durable execution is only available where the existing managed catalog
  supports it: Linux x86-64 GNU and Windows x64 MSVC targets. Older
  conventional native manifests lack a hash-bound artifact inventory and
  continue to require the explicit flag.

Verification proves which bytes are requested; it does not sandbox them.
Approved native code runs with the user's privileges and uses the existing
OS loader and system-library trust boundary. The user account and its
private data are the trust boundary. Code already running with the same
user's privileges can change that user's files. JIT's private copy adds one
checked payload read and write per terminal run using durable approval.
There is no new persistent native build cache or Runtime ABI.

The design takes the narrow idea of local, content-scoped consent and
explicit revocation from [direnv's manual](https://direnv.net/man/direnv.1.html)
and [its approval implementation](https://github.com/direnv/direnv/blob/master/internal/cmd/rc.go).
VOID's consent is based on its own verified application and native identities.

## Qualification

Qualification is focused on the affected compiler and native workflows.
The independent C-provider fixture checks first approval, decline, exact
consent, new-process reuse, durable builds in both profiles, piped input,
CI, explicit flag compatibility, copied-project isolation, changed
catalog/target/descriptor/artifact/binding/source/lock, malformed and
hard-linked records, a changed catalog during the prompt, approval renewal
after adding a valid dependency, and revocation. Its generated AOT
executables must print exactly `42`. The bundled raylib workflow verifies
durable approval/reuse/revocation and independently linked AOT through
the existing path with its headless pixel oracle (`241`). The existing
managed-native suite checks that native initializers cannot run during
verification or before authorization. Native source changes use existing
lock/tamper diagnostics. G60's frontend algorithm, the G61 resolver and
the G62 template implementation are retained; only output admission guards
for the new user-data directory touch template creation.

| Profile | Focused suite | Bundled raylib | Stack check |
| --- | --- | --- | --- |
| Linux x86-64 Release | 12/12 | Passed | Independent C fixture ran at a 1,024 KiB stack limit |
| Linux x86-64 fatal ASan/UBSan/LeakSanitizer | 12/12 | Passed | Normal sanitizer stack; independent C fixture also rerun after the Windows-only terminal harness adjustment |
| Windows 10 x64 build 19045 Release | 7/7 | Passed | 1,048,576-byte reserves verified for the compiler, helper executables and independently linked test applications |
| Windows 10 x64 build 19045 Debug | 7/7 | Passed | 1,048,576-byte reserves verified for the compiler, helper executables and independently linked test applications |

Both CLI optimization profiles exercised JIT and separately linked AOT.
The tests used the pinned LLVM/Clang/LLD 23.1.4 toolchain. Linux's selected
suite includes the managed-native callback control; Windows excludes the
Linux-specific managed-native resolution/callback coverage. No broader
compiler or platform campaign is implied. The Windows receipt reverified
3,357 input files and confirmed the SDK and provider archives were unchanged.

The first Windows terminal harness launch inherited the test runner's
redirected stderr. The compiler correctly refused it. The final harness
uses a Windows [pseudoconsole](https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session)
and binds the test child's actual console handles; the authorization
check was not relaxed.

One Linux raylib startup check used the same prepared project, a real
pseudoterminal, exact output checks and all six orderings of the three
variants. Median time to first output was 731 ms for G62 explicit (range
693–834 ms), 726 ms for G63 explicit (661–781 ms), and 767 ms for G63
durable (701–899 ms). The durable path had about 41 ms more at the median
than G63 explicit. Ranges overlap; this is a focused observation, not a
whole-compiler performance result. G60's validation improvement remains in
place.

The exact source diff and per-file hashes are delivered with the external
review evidence. The patch is replayed against the stated baseline and
verified against all 16 postimages. Work is uncommitted.

**Recommendation: INTEGRATE.** The approval lifecycle and negative controls
passed on both qualified hosts. Durable approval is limited to the
existing managed-native catalog targets and a real terminal; automation
and conventional providers use the existing explicit flag.
