# OS engineering record

Boss authorized the complete 13-function DESIGN/ROADMAP on 27 October 2026.
The research documents retain their original proposal wording as historical evidence.
Initial worktree and protected README hashes: [initial receipt](evidence/initial.json).
Existing docs/README edits and the separately appearing stdlib backlog are preserved.

Implementation is present: all public wrappers, managed errors, closed operation
contracts, canonical frontend and independent IR guards, LLVM JIT/AOT lowering,
rooted Runtime publication, Linux/Windows/macOS adapters and both native link paths.
No Git history, hosted CI or protected root README mutation is authorized.

## Retained failures and corrections

- First source check: ordinary enum equality is unsupported and imported `path`
  names cannot be shadowed. Use enum match and byte-only device-stem validation.
  Original facade: `evidence/os-before-source-correction.void`; original and corrected
  check logs are retained. No compiler behavior was weakened.
- Relative malformed HOME: classify native absolute/relative form before UTF-8
  validation, so relative HOME yields invalid_path and malformed absolute HOME
  yields invalid_utf8. The initial execution log preserves the observed mismatch.
- Initial AOT fixture incorrectly supplied the run-only `--no-cache` option to
  `build`. Status 2 and exact source/tool hashes are preserved in the crash-harness
  receipt `incident-z8cj64i3`. This was a rejected CLI invocation, not a compiler
  crash. The fixture now applies that flag only to JIT runs.
- Empty string bridge defect: Runtime represents empty strings as nullptr. The
  ABI incorrectly treated pointer nullness as arity. Arity remains independently
  validated in IR; the native bridge now accepts an empty managed string, returning
  invalid_argument for `env("")`. Original source, diagnostics, fixture and binary
  hashes are retained under `evidence/empty-string-*` and the pre-correction ABI.
- The expanded ID fixture caused expensive compilation through hundreds of static
  calls. The same 9 valid and 61 invalid tokens now run through compact loops and
  the same four public getters. Original expanded source/generator are retained;
  no case or assertion was dropped.
- Cwd fixture expected a path with no trailing separator. The existing
  `path.absolute("")` intentionally retains the empty joined component's trailing
  separator; both documented spellings agree. Preserve that behavior and compare
  with an independently queried native cwd using that existing spelling. Original
  source/diagnostics/tool hashes are retained in `incident-rhvfk4d_`.
- The added error-field completion fixture omitted the required `kind` enum
  initializer. An independently checked source retained in `incident-u0nfgzyr`
  identified the valid language rejection. The fixture now initializes kind;
  no editor/compiler rule was relaxed.
- The first owner-async fixture tried to use unsupported async CLI main. The
  source was correctly rejected with no entry; `incident-rg_cp3ha` retains the
  Release command/fixture/binary hashes. The corrected fixture uses the existing
  explicit async host and async object emitter, as tests/random already does.
  Normal/O2 JIT and independently linked/executed async AOT remain required.
- Strengthened independent IR fixtures initially omitted the existing required
  stored-field declaring-owner metadata. The validator correctly rejected that
  supposed positive fixture. Original host source, tool hash and diagnostic are
  retained by the preservation wrapper. The synthetic canonical reply now carries
  the same field owners as real compiler-produced IR; rejection guards remain.
- Final Apple source audit removed a nonexistent Cocoa out-of-memory enum name.
  Foundation allocation/invariant exceptions and POSIX NSError ENOMEM retain
  Runtime failure; no unrelated Cocoa code is guessed as ENOMEM. URL conversion
  now validates decoded text/NUL before filesystem C-string conversion and maps
  the documented character-conversion exception to invalid_utf8. These are Apple
  source corrections, not native compilation/execution evidence.
- Explicit native out-of-memory diagnostics (errno/Win32/HRESULT/NSError) now use
  checked Runtime failure, as required, rather than an ordinary environmental Err.

## Final local evidence

Initial Linux Debug checks passed: 80 location/identity processes, 62 frontend
processes, 103 initial native/ABI/GC/IR/global/reload/cache assertions, 40 LSP
assertions, 6 fresh-process cache commands and 8 error processes covering 70 IDs
x four getters. The final strengthened host checks, consolidated Debug/Release/strict results,
actual stack receipts, supplemental vectors and native limits are recorded in
[HANDOFF.md](HANDOFF.md). These initial counts remain historical checkpoints,
not the final acceptance total.

The POSIX test seams use the existing linker-wrapper approach, scoped entirely
to the test executable. Production contains no global fault hook. Runtime(1)
forces collection through string publication; a readlink wrapper independently
collects on another thread while the owner is NativeSafe. PID's object-count
check verifies no managed string allocation. Existing allocation budgets sweep
native and managed failure points.

Windows COM ownership follows the [native API contract](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex).
The Apple adapter uses Foundation file URL filesystem representations, create:NO,
scoped autorelease ownership and a contained Objective-C exception boundary.
Actual Windows/macOS execution is unavailable on this Linux host. No native
acceptance, packaged Linux launcher execution or signed App Sandbox pass is claimed.

H1–H7 remain separate historical incidents. No ptrace or hosted job was used.

Final local effective acceptance is 75/75 unique configuration/suite outcomes,
with original failed attempts and later OS checks preserved separately. The
strongest independent host has 127 assertions per profile; explicit launcher
fixtures pass 92 process checks per profile on actual hard 1 MiB stacks.
Additional portable PID test infrastructure accounts for Windows exec semantics
using independently captured child PIDs rather than assuming POSIX PID-preserving
exec. The Windows branch remains unexecuted; Linux cache/install rechecks pass.

Early source-check and relative-HOME assertion logs/fixtures were preserved before
full automatic failure recording was installed. The old compiler executable was
rebuilt before its original hash was taken; that original hash cannot be supplied.
Later preserved failures include actual binary hashes. No hash is reconstructed
and presented as the original.

Engineering stops for independent Golden Wrench source review. Actual native
Windows/macOS verification, including final Apple-only source corrections, remains
unclaimed. Protected README/research hashes and HEAD are unchanged.
