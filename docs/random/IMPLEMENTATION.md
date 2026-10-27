# Random Sprint 4 implementation record

Authorized baseline: b374fd30aad70bd27c334150949b2a06481b1b67.
Initial worktree: modified docs/README.md and untracked docs/random research.
Boss approves all four decisions, overriding bounded API names with rand_T arities.
No Git mutation, hosted CI or root README edits are authorized.

Complete local implementation and verification. Independent Golden Wrench review
is the next gate. Historical filesystem/LLVM incidents are unchanged.

The adapter selects private source implementations after resolving the canonical
public declaration, before ordinary binding. Module function values keep their
actual zero-argument signature; bounded call forms are direct-call syntax only.
Generator checked numeric methods require one or two bounds.

## Implemented source and compiler seams

`stdlib/random.void` implements all ten numeric families, Boolean, inclusive
ranges, checked/plain calls, source value generators, reseeding, Runtime globals,
OS initialization and growable collection operations. [API audit](API.md) lists
66 public top-level declarations, including 62 function/method declarations.
The numeric declarations expose 110 direct-call forms. All public declarations
and their written parameters/results have accepted documentation tags.
[Algorithm attribution](ATTRIBUTION.md) records the adopted primary sources.

`src/frontend/random.inc` selects source implementations only after resolving
canonical public declarations; ordinary binding performs exact typing, named
mapping and contextual literal adaptation. Private domain and XOR helpers retain
canonical access checks. The integer-domain helper emits ordinary constant IR;
XOR has one independently validated u64 operation. Entropy has one independently
validated checked result operation and is forbidden in frontend worker effects
and independently in IR worker graphs. The methods retain ordinary receiver-place
mutation and authority rules. No general overloads or runtime RNG object exists.

LLVM lowering uses ordinary XOR and the existing failure pointer. The native
bridge enters NativeSafe around eight OS bytes and publishes a little-endian seed
only after complete success. Bcrypt is linked by Runtime consumers and propagated
in installed native flags. Windows/macOS native execution remains unverified.
The default uses Stage 11 source globals, with no new Runtime state framework.

## Corrections found during focused engineering

- The first C++ compile used a nonexistent diagnostic enum spelling; corrected
  before executing new VOID code. Initial source checks corrected VOID loop
  spelling and explicitly typed large decimal constants.
- The approved initializer label `seed` conflicts with the module function under
  ordinary no-shadowing. One canonical initializer-parameter exception preserves
  this API; independent ordinary shadowing/duplicate tests still reject.
- Signature help initially had no active parameter documentation on private
  bounded bodies. Accepted source `@param` tags now supply that documentation;
  completion and hover use compiler-owned arity presentations. A recovered
  completion initially tried rendering an unresolved generic collection header;
  numeric presentation is now restricted to the ten concrete numeric families.
- Complex const-list slicing exposed an existing frontend type-construction
  defect: readonly child paths were embedded in the physical array element
  definition as well as projected onto the list. The physical element is now
  authority-free; projected permissions are retained on the returned list type.
  The independent IR validator was preserved and its diagnostic made more precise.
- Test-only corrections: named-bound expected sample was 1, project imports need
  quoted local paths, managed fixed-array fields need nullable elements for their
  implicit defaults, precise managed-construction count is two boxes (buffer
  allocations use their existing path), and direct entropy Runtime attachment
  must end before invoking a separate JIT Runtime. Allocation sweeps now reach
  their first success instead of stopping before all allocation positions.
- Canonical stdlib sources cannot be overridden through ordinary document
  overrides. Restart tests use a separately copied configured stdlib source root,
  preserving that invariant. CLI does not admit async `main`; worker execution
  reuses the explicit async runner and object-emission/link path instead.

These were diagnosed corrections, not unexplained retries. No native signal,
Windows exception or sanitizer termination has occurred during focused random
engineering so far. One graceful internal IR rejection has exact source and
stream evidence at `evidence/internal-validation/incident-5tv3becw/receipt.json`.
The `*_crash.void` filenames follow the existing evidence infrastructure; this
incident is classified as a checked internal validation rejection, not a signal
crash. Historical filesystem/LLVM crash research remains unchanged.

## Preservation boundary

New Python compiler/JIT/AOT/runner tests use the existing shared preservation
module. The host writes exact input snapshots before each analysis; canonical
imports and test generators are retained on incidents. Timeout handling now
kills/reaps through Popen, captures the actual native exit status and preserves
complete streams before temporary cleanup. Existing native-signal, Windows-status,
reserved sanitizer status and unsaved-LSP-overlay behavior is retained. Synthetic
self-tests validate these routes and dispose only their synthetic incidents.

Final receipts are indexed in [HANDOFF.md](HANDOFF.md) and `evidence/FINAL.json`.

## Acceptance corrections and current evidence

The initial Debug consolidated selection passed 30/31; its only failing test was
that forged-worker test's cross-program type setup. The corrected host reports
**2,281 assertions**, with the exact worker diagnostic and function-value checks.
A final presentation correction makes bounded public calls display `public`
in hover instead of inheriting their private implementation visibility.

One subsequent Debug optimized invocation exceeded a 180-second child deadline.
The wrapper preserved actual -9/SIGKILL status and exact original bytes before
cleanup. A running compiler inspected during this investigation consumed about
99% CPU and roughly 2.5 GiB RSS, rather than waiting on OS entropy. The generated
main was split into small concern functions without changing the 792 expected
outputs, assertions or draw sequences. The split fixture passed all four modes
with the same deadline. Final numeric follow-ups retain separate receipts; no
LLVM miscompilation or fixed reliability defect is claimed from this timeout.

The first strict selection passed 4/31 and failed 27/31 because LeakSanitizer
could not inspect threads under sandbox ptrace restrictions. Nineteen wrapped
abnormal exits were preserved with status 86. A minimal unsandboxed check passed
with identical strict ASan/UBSan/leak options, and only the 27 affected tests were
rerun outside that sandbox. Leak detection was not disabled. These environment
failures are not described as memory leaks or undefined behavior in the module.
See [all incidents and provenance limits](INCIDENTS.md).

The Release consolidated selection passed **31/31**. Its representative O2 host
workload completed 20,000 iterations (raw, typed, bounded, four-item shuffle) in
**0.408303 s**, about **48,983 iterations/s**, excluding compilation. This is a
single local workload observation, not a cross-target performance promise.
Generator state is four inline u64 words; the engine allocates no per-draw arrays
or native generator objects. Complex collection snapshots retain the existing
allocation behavior; fault sweeps reached 200 copy and 133 in-place allocation
failures before success in each JIT mode. In-place failure need not roll back.

Focused development receipts are indexed at `evidence/focused/index.json`.
Final effective profile totals, follow-up status and hashes appear in HANDOFF.md.


## Final local outcome

Effective results are **31/31 Debug, 31/31 Release and 31/31 strict sanitizers**,
with the original failures and focused repairs retained separately. Final compact
numeric/1 MiB follow-ups passed **2/2 in each profile**, preserving all **792**
outputs and the independent expected-output hash. The strict follow-up ran outside
the sandbox with leak detection enabled. The complete record contains **130 CTest
executions: 101 passing and 29 initial failures**, all failing areas subsequently
passing. [Handoff](HANDOFF.md), [effective machine receipt](evidence/FINAL.json)
and [source/binary freeze](evidence/SOURCE_FREEZE.json) contain exact scope.

All ten roadmap milestones have local engineering deliverables. Native Windows/
macOS execution remains explicitly unverified; no hosted acceptance or independent
source approval is claimed. No Git mutation or root README change occurred.
**Stop for independent Golden Wrench source review.**
