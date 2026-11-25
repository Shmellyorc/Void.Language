# Project storage and engineering retention

This policy supersedes earlier instructions to retain VOID engineering evidence
or approved native staging directories in `/tmp`.

## Compiler storage

Manifest-selected `voidc run` executes with the selected project root as its
working directory, including when invoked with `--project` from elsewhere.
Relative application assets resolve from that root. Artifact storage does not
determine the application's working directory. Standalone source invocations keep
their existing working directory.

Compiler-owned project storage is:

| Location | Ownership and lifetime |
| --- | --- |
| `.void/acquire-<random>/` | Invocation-owned package preparation; checked inputs and outputs. |
| `.void/cache/v1/` | Validated and authenticated JIT cache; optional, reusable. |
| `.void/run/native-<random>/` | One native invocation; private payload and `.owner` lease. |
| `.void/build/<entry>` | Default manifest application executable and its managed native payload. Explicit `-o` remains relative to the caller. |

Ordinary project, `.void`, cache, run-parent and build directories follow actual
OS access permissions, including accessible shared workspaces. New ordinary
directories/files use `0777`/`0666` subject to umask and inherited ACLs; existing
user directories are not chmod-repaired. Held identity, no-follow/type checks,
native consent and authenticated cache publication remain mandatory. Cache
authentication credentials occupy a separate confidential per-user leaf.
Invocation-owned native run/build leaves remain private: POSIX mode `0700`,
native payload/ownership records `0600`, and protected Windows DACLs. macOS
private native leaves clear inherited allow ACLs. Ordinary Windows parents retain
their inherited access; directory handles remain pinned. See the
[correction report](projects/UNIFIED_DEVELOPER_EXPERIENCE.md) for Linux evidence
and pending native Windows/macOS qualification.

Saved approval still binds the exact project, provider, artifacts, binding,
application, entry and compiler identity. Native payload hashes remain mandatory.
On Linux, approved libraries and their managed dependencies are captured through
pinned descriptors, rehashed, sealed in bounded anonymous kernel files, and
loaded from those sealed descriptors in dependency order. License notices are
verified but never executed. Replacing a project or staged file after capture
cannot replace a loaded image. The conventional system-library loader contract
remains in effect. Windows retains its private staged DLL loader contract.

Package preparation reuses the project directory publisher's exclusive staging
and identity checks. An existing `.void` follows OS access permissions and path
integrity checks; it is never chmodded. Success and recoverable failure remove the
invocation's acquisition leaf. A substituted leaf is preserved. Hard-killed
acquisition leaves carry no later reclamation authority and require manual review.

## Cleanup ownership

`.void/` as a whole is **not** compiler-owned disposable space. The compiler
does not recursively delete it or its arbitrary contents.

For abandoned entries, only exclusively generated native run leaves with an exact
`VOID native run 1` ownership record are eligible for reclamation. The record lists the compiler's
payload filenames. On POSIX a held `flock` lease protects active runs; Windows
uses a no-sharing record handle. Normal completion and recoverable failures
release the invocation's stage. A subsequent invocation examines at most 128
entries for abandoned unlocked stages, avoiding PID-reuse assumptions and unsafe
signal-handler filesystem work. Symlinks, foreign ownership, malformed records,
and unknown files do not confer deletion authority. Unknown user files prevent
abandoned-directory removal.

While constructing a run, the creating invocation separately records successful
exclusive directory creation and the directory's device/inode identity. If a
later directory open or other initialization step fails, it can remove that same
verified empty leaf without depending on a stage descriptor. It removes only its
own exclusively created `.owner` file, verifies identities before cleanup, and
uses nonrecursive directory removal. Replacement directories and unknown files
are preserved.

If the process is killed before it finishes a valid ownership record, the next
invocation preserves the unmarked or partial-record entry for manual review.
A `native-*` name, private mode or same-account ownership alone never grants a
later process deletion rights. This intentionally conservative crash window is
distinct from recoverable constructor failure, whose creating process can clean
its recorded directory identity. The same rule applies if initial identity
capture itself fails: report the OS error and preserve the ambiguous entry.

Directory open/inspection failures report their underlying OS errors separately
from deliberate ownership/privacy rejections. Windows payload staging uses
exclusive native file creation and checked write, flush and close operations;
failures include the artifact path, Windows error number and system description.

Do not place user files inside generated `native-<random>` leaves. Those names,
the `.owner` record and listed payload names belong to the compiler. Windows's
normal completion removes its exclusively created run leaf; abandoned Windows
cleanup preserves unknown entries. Keep user data outside compiler namespaces.
After a hard crash, cleanup may wait until the next native invocation. A project
never run again may retain its bounded interrupted stage for manual review.

Windows AOT linking retains the existing bounded, automatically cleaned private
short-path directory when its linker requires the full UTF-16 executable path to
fit the existing 240-character constraint. This does not alter application cwd.

## Engineering retention

Repository policy: `docs/` contains legitimate Markdown only. Generated evidence,
receipts, inventories, logs, snapshots, binaries and temporary investigation data
stay outside the repository. Git history retains superseded tracked material.
Follow the current [agent workflow](WRENCH_CLANKER.md#permanent-documentation-policy--25-november-2026).

Retained engineering reports belong in
`~/Documents/Void.Language-archives/<milestone-or-investigation>/`.
Markdown reports with concrete measured results are the primary record. Keep
original failures needed to explain unresolved problems; delete abandoned
generated binaries, copied toolchains, disposable fixtures and redundant raw
evidence. Do not make another bundle merely because a task finished.

`tools/engineering_storage.py` centralizes retained harness output. Its
`VOID_EVIDENCE_ROOT` override must remain outside temporary storage. VOIDUP
qualification shares `voidup-qualification/`; use distinct meaningful run names,
not repeated equivalent runs. Cross-process fixture retention must be explicit
through a caller-supplied `--reuse-work` directory; otherwise fixture scratch is
owned by `TemporaryDirectory` and cleaned at process exit, including exceptions.
Crash recorder defaults retain original failures in `compiler-crashes/`.

Temporary scratch may use the OS temporary directory when its owner cleans it.
For a needed diagnostic rerun, use
`VOID_TEST_DIAGNOSTICS_DIR=~/Documents/Void.Language-archives/<investigation>`.
The test launcher rejects retained `/tmp` diagnostics. After a forcibly killed
harness, inventory its abandoned scratch before deleting it. Never sweep `/tmp`.

Migration records preserve historical bytes and hashes. Current handoffs must
link to verified retained destinations or the relocation index and explicitly
identify discarded generated artifacts; historical commands are not proof that
their former `/tmp` files still exist.
