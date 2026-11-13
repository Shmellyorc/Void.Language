# G28-R — real application readiness audit

13 November 2026. Code baseline:
`e3e6018430ac68235d2d91fd4b7ce614aa8ce3b6`.
Research and focused native execution only; no production changes, Git mutations,
package publication or hosted CI. This report does not confer release approval.

## Assessment

The exercised workflows combine existing facilities without another framework:
typed configuration files with bounded parsing and atomic save/reload; dependent
libraries with shared initialization and checked locks; local TCP/UDP peers and
explicitly hosted asynchronous network/HTTP work; approved native calls; and
compiler-owned package navigation, completion and diagnostic recovery.

G27's two library corrections are present. The three Linux and two Windows
compiler/server pairs are compared against the exact SHA-256 identities in
`g27-library-authoring-2026-11-13.zip`, not inferred from their filenames or
version strings. The production source delta from G27-R is the two reviewed
frontend statements; there is no new Runtime/backend implementation in this audit.

Readiness here means functional behavior in the exercised environments. It does
not establish complete language coverage, crash freedom, a latency guarantee,
public redistribution approval, every filesystem, current macOS or all native
devices. Historical incidents and existing owner-controlled release obligations
retain their original classification.

## Applications exercised

### A. Configuration reader, validator and atomic save/reload

The maintained [configuration application](../../examples/json/configuration.void)
reads a UTF-8 file, parses bounded JSON, selects a required player name and nested
audio settings, applies typed defaults, validates application constraints and
reports distinct I/O, document and domain errors. Its save operation constructs
typed data, stringifies it, creates a private directory, atomically publishes
`game.json` and reloads it. It checks publication separately from synchronization.

The unchanged [configuration driver](../../tests/json/configuration.py) and
[typed-field fixture](../../tests/json/configuration_fields.void) exercise:

- Five compile-time rejection controls: wrong fallback type, readonly collection
  or managed fallback, private DTO fields and custom initialization.
- Both normal and optimized JIT, plus separately emitted and independently linked
  native objects in both optimization modes.
- Eight selected file inputs per JIT mode and all fifteen per AOT mode: defaults,
  explicit values, required fields, null/type/range errors, domain validation,
  invalid Unicode/UTF-8, duplicate decoded keys and resource limits.
- The exact 4,096/4,097-byte boundary; located JSON error paths/offsets; typed
  arrays, dictionaries and managed/nullable data in the field controls; exact saved
  bytes, retained lock-file semantics and reload in every mode.

Linux subprocesses use the driver's real 1 MiB stack limit. The Windows lane
uses the existing native toolchain/link plan; this audit does not infer a fresh
stack-reservation inspection from Linux execution. Native error and sanitizer
assertions remain unchanged.

An additional eleven-command Linux Release application probe copies the same
maintained source into `voidc new settings`, checks and runs it in normal/O2 JIT,
packages and verifies it, relocates the package and moves the original project.
The relocated executable reads valid settings, returns status 3 for an empty
player, saves exact JSON bytes and reloads them. This closes the specific gap
between the standalone example and manifest/D5 application execution.

### B. Dependent libraries, edits, locks and pinned source

The unchanged [package fixture](../../tests/project/packages.py) composes an
application with geometry/algebra and their shared units package, quoted local
helpers, `lib/tuning` and `math`. Shared global initialization prints exactly once;
normal/O2 JIT and native images agree on `units-init\n雪 18\n`. It checks emitted
objects, relocation, source identity, cache behavior where available, legitimate
source edits and refreshed results (`雪 20`), stale/malformed locks and direct
dependency/access boundaries. These are source packages, not new native libraries.

Fresh pinned-source evidence uses a **copy** of G26's retained verified consumer
and D3 snapshot at `build/g26/release-raylib-Git-installed/consumer`. Before use,
its lock matches G26's recorded SHA-256
`77f2f6c82ddbe9d1049a123721f7cc7b62dc71b2723de53a98c66b12f94dd08b`.
Its origin is the historical private HTTPS fixture and its full revision is
`6815a7f3ecc0428f8ea8a03fed934dc65dc59e25`. No Git command, fetch, remote mutation
or new repository was needed. Both `tidy --check` and `check` pass on the copy;
complete file-content hashes of the copy and original remain unchanged by those
two read-only commands.

A further thirteen-command Release probe adds a freshly generated local helper
to that copied pinned consumer through G27 `add`, then uses both `pkg/helper`
and the verified raylib source. It returns exact stdout `83\n` in normal/O2 JIT,
native AOT and relocated D5 execution after moving the source projects. Native
approval remains explicit through the existing catalog and `--allow-native`.
The original retained G26 project is never edited. This establishes mixed local
and pinned **offline consumption**, not fresh public GitHub acquisition or
publication of raylib's still-unpublished standalone repository.

### C. Network applications using the supported hosts

On Linux, the unchanged [network driver](../../tests/net/socket/run.py) exercises local
peers only. It combines native DNS controls, source address/endpoint handling,
TCP connect/listen/accept/transfer/half-close, bounded reads, UDP datagrams and
recoverable timeout/closed/busy/error outcomes.

[Synchronous source](../../tests/net/socket/source_sync.void) runs through normal
and optimized JIT and a freshly built native executable in each mode. The
[TCP echo server/client](../../examples/net_sockets/) are independently compiled
VOID applications exchanging data over an ephemeral loopback port; the client
uses application arguments, conversion, byte arrays, UTF-8 and checked cleanup.
The UDP and IPv6-address examples are also exercised.

[Async source](../../tests/net/socket/source_async.void) combines DNS, TCP, UDP,
file I/O, a real local HTTP response, task groups and cancellation with only one
native worker. It uses the existing `void_io_runner --async-one-worker` and
prelinked `void_network_async_aot` host. These are deliberately explicit scheduler
adapters. Successful execution does **not** imply support for an ordinary async
CLI main. These complete-driver/async results are Linux evidence; see the explicit
Windows qualification boundary below. Existing auxiliary binaries are reused and their identities recorded;
this research does not rebuild or substitute a new host.

### D. Native integration within a reusable-library application

The unchanged [physical raylib fixture](../../tests/project/raylib_physical.py)
generates an engine library, adds real raylib source to it, adds that library to
an application and verifies separate source/native authority. Missing application
requirements fail; run without `--allow-native` fails. Approved normal/O2 JIT,
linked AOT and verified relocated D5 execute `LIBRARY: 41`.

The fixture then edits a local wrapper: the stale lock is rejected, tidy admits
the legitimate edit and JIT/AOT observe `PHYSICAL: 42`. A new ordinary wrapper is
usable. Invalid native signatures, non-pass declaration bodies, wrong providers
and removal of approval remain rejected. The inventory covers 600 API entries;
that count is **not** a claim that all 600 functions execute in this test.
This is headless qualification, not new graphics/audio/device coverage.

### E. Developer tooling and legitimate changes

The package fixture uses the real current `voidls` process over stdio through the
maintained `tools/lsp_client.py`. It verifies application→package and transitive
definition locations, import completion, invalid partial-import diagnostics and
repair in the same session. A broken on-disk package is diagnosed at its physical
file; fixing it, refreshing its legitimate lock and calling `void/refreshProject`
clears the diagnostic and restores navigation.

Consumed locked package buffers intentionally do not gain overlay or rename
authority merely by opening them from a consumer. Their saved, verified bytes
remain authoritative. Ordinary `lib/...` source and the package's own authoring
project are different contexts. No new overlay policy is inferred from the word
“editable.” See [local libraries](../LOCAL_LIBRARIES.md) and G27's authoring guide.

Fresh evidence is protocol/native-server behavior, not a new VS Code GUI run.
Existing G26 real-editor acceptance and G27 focused lifecycle qualification are
reused. Vim/Neovim project parity is not newly qualified.

## Focused execution results

The selected Linux drivers all passed: Release **4/4**, Debug **3/3**, strict
**3/3**. Configuration and package/editor drivers passed on native Windows Debug
and Release; the physical native-library driver also passed on Windows Release.
Both Windows profiles also passed all eight direct socket commands (normal/O2
JIT, build and AOT execution, UDP and address examples).

| Workflow | Linux Release | Linux Debug | Linux strict | Windows Debug | Windows Release |
| --- | --- | --- | --- | --- | --- |
| Configuration, typed fields, exact atomic save/reload; normal/O2 JIT/AOT | PASS | PASS | PASS | PASS | PASS |
| Libraries, source edits, locks, relocation, real voidls | PASS | PASS | PASS | PASS | PASS |
| Full maintained TCP/UDP/DNS and explicit one-worker async/HTTP driver | PASS | PASS | PASS | Not qualified: auxiliary targets absent | Not qualified: auxiliary targets absent |
| Direct unchanged synchronous socket source; normal/O2 JIT/AOT | Included above | Included above | Included above | PASS | PASS |
| Physical native library, approval guards and relocated D5 | PASS | G27 evidence reused | Not rerun | G27 evidence reused | PASS |
| Extra packaged configuration and mixed local/pinned application journeys | PASS | Not rerun | Not rerun | Not rerun | Not rerun |

The two extra Release journeys passed 11 and 13 commands, preceded by two
successful read-only pinned-consumer commands. Counts describe driver/command
invocations, **not** a fabricated aggregate of individual assertions or CTests.
The original two unsuccessful setup invocations remain failures in their own
receipts; successful continuations do not overwrite them.

Strict Linux uses `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No sanitizer suppression was
added. The independently linked strict images use the maintained sanitizer link
manifest. No compiler/runner crash was observed in this selection.

Windows reports **Windows 10 build 19045**, x64 VS developer environment 17.14.42,
MSVC **14.44.35207**, SDK **10.0.19041.0**. Both compiler/server pairs match G27's
accepted SHA-256 records; all 29 guest stdlib files match the current Linux
checkout byte-for-byte after path-separator normalization. The native manifests
select physical LLVM/Clang/LLD **23.1.4** and profile-matched host/Schannel inputs.
No Linux cross-compilation or shared-folder executable serves as Windows proof.

Each maintained invocation retains its own assertions. Whole-driver elapsed
times, exact commands and outputs are archived, but include compilation/linking,
repeated subprocesses and local peers. Linux and the VM ran concurrently; these
are **not performance comparisons**. A 900-second per-driver bound prevented
unbounded campaigns. No whole-suite or hosted run was launched.

## Failure investigation: private save refused the initial scratch ancestry

**Observed:** the first Linux Release configuration invocation returned status 1
and `input /@stdlib/io.void::error_kind.permission_denied` at its normal-JIT save.
The test-owned intermediate `release-tmp` directory had mode **0775**, inherited
from the shell's umask/default creation. The enclosing task root and final fixture
directories were 0700. The exact failure log, fixture and permission observation
are preserved; it was not a SIGSEGV or sanitizer failure.

**Responsible path:** `stdlib/io.void:1751–1761` delegates private-directory
creation to the checked host bridge. In
[host_directory.cpp](../../src/runtime/host_directory.cpp), `safe_ancestor`
at lines 74–78 rejects group/world-writable ancestors except the documented
root-owned sticky case. `root` checks each held ancestor at line 92 before opening
the next component. The 0775 intermediate fails even beneath a private outer
directory. This is the existing conservative policy, not a new Linux API failure.

**Expected behavior:** private-directory admission intentionally checks the whole
ancestry, ownership and ACL, not merely whether the final directory is writable.
The [host ownership contract](../voidup/REPORT.md) and
[atomic-save contract](../io/CONTRACT.md#atomic-application-saves) specify this.
The [Linux open/openat documentation](https://man7.org/linux/man-pages/man2/open.2.html)
explains held-descriptor and no-follow semantics; it does not itself impose
VOID's additional ownership policy. On Windows, the
[Microsoft file-security documentation](https://learn.microsoft.com/en-us/windows/win32/fileio/file-security-and-access-rights)
explains explicit security descriptors and inherited ACLs; accordingly the guest
scratch root uses VOID's existing private-root verifier, not ordinary directory
creation as a substitute for security admission.

**Smallest correction tested:** set only the task-owned intermediate directory
to 0700 and create subsequent Linux profile scratch with explicit 0700. The
unchanged maintained Release fixture then passed all four execution lanes.
No product condition, ACL principal, directory-sharing mode or durability barrier
was removed. The original receipt remains separate from the corrected run.

**Classification:** investigation setup error and successfully enforced intentional
restriction; not a new P0/P1/P2/P3 production defect. No compiler/Runtime repair
is justified by this failure. An existing user directory must not be silently
chmod-repaired by VOID.

The Windows launcher also initially assumed `C:\BuildTools` directly; the
established `vswhere` lookup selected the actual `C:\BuildTools\VS2022`.
That launcher correction is outside the product. Windows native results require
the verified guest OS/compiler identities and actual execution, not the VM label.

### Windows network harness prerequisite

The initial Windows Debug network driver could not launch
`VoidG20-20261112/Debug/void_network_native.exe`: **WinError 2**, because that
auxiliary executable and the two async fixtures were not built in the retained
G27 compiler/server configuration. Configuration and package/editor drivers had
already passed. A read-only search of established `C:/Users/a/Void*` workspaces
found only older `void_io_runner.exe` copies, not the complete matching network
fixture set. Those older executables were not substituted.

This is an audit prerequisite failure, not a reproduced socket/Runtime defect.
The exact traceback is retained. The disposable preservation wrapper also could
not print a Unicode scratch path under cp1252; it had already detached cleanup,
so the workspace remained available. Subsequent audit subprocesses use Python
UTF-8 diagnostics; no production Unicode conversion or fixture assertion changed.

The bounded continuation runs the **unchanged** `source_sync.void` directly in
normal/O2 JIT and freshly built AOT for each Windows profile, plus the maintained
UDP and IPv6-address examples. It exercises TCP, UDP, localhost DNS, exact 70,000
payload bytes, half-close, EOF, timeout and datagram error/progress assertions.
It also completes the remaining Release configuration, package/editor and native
library drivers. It does not label this subset as a passed full network driver.
Fresh Windows native DNS-control and one-worker async-host qualification remain
**unqualified in G28-R**. No production correction is justified by missing test
executables, and a broad native rebuild was not initiated to conceal the limit.

## Source and ownership review

- `project_creation.cpp::plan_creation` now emits the known library starter entry;
  `project_resolution.cpp::add_dependency` accepts library consumers and still
  uses the same Resolver and `publish_package_edit`. G27 is completed work, not
  a new finding. The fresh native and mixed-source probes exercise that integration.
- `frontend/json.inc` performs compiler-owned schema admission and generates
  concrete adapters into the existing language pipeline. Private/resource/callable
  schema restrictions are deliberate. The tests verify retained typed error paths,
  defaults and readonly/identity boundaries rather than adding another JSON engine.
- `runtime/network_abi.cpp` uses the existing NativeResource ledger, owner-thread
  checks and precise reply roots. Native Work is integrated into the existing
  async mechanism; no source-worker exemption or new scheduler was needed.
- `runtime/host_save.hpp` and the platform directory implementations retain staged
  publication, writer exclusion, progress/error records and real synchronization.
  Linux test scratch on tmpfs does not prove power-loss durability. Admitted
  filesystem and platform boundaries remain those in the IO contract.
- `lsp/main.cpp` and the current tooling sessions retain independent open-document
  diagnostics, immutable revision acceptance and package source authority. Nothing
  here justifies reopening G21/G22 or duplicating editor resolution.

The object model is unchanged, including extends/using/interfaces, virtual/override/
super/has and operators. The audit found no need to redesign it to execute these
applications. No speculative correction or production candidate was built.

## Boundaries and next-work decision

Classify only reproduced product failures as new defects. Accepted negative
controls—malformed JSON, exhausted budgets, stale locks, private visibility,
undeclared imports and missing native permission—are required behavior.

Ordinary async CLI roots remain intentionally unsupported; explicit host adapters
work within their documented contracts. This audit does not silently authorize
an async application-hosting extension. Current macOS, arbitrary native devices,
public standalone raylib acquisition, public bootstrap/release distribution and
clean-machine deployment are outside the fresh qualification. D5 relocation here
is not a claim of all-platform distribution readiness.

Preserve the unresolved historical optimizer incidents, Windows symlink-prerequisite
and executable-cleanup records and external raylib/Gallium graphics/audio leak
classifications. Headless success does not repair or reclassify them. Fatal
ASan/UBSan with leak detection qualifies the instrumented compiler/Runtime scope;
it does not mean generated JIT instructions receive full sanitizer instrumentation.

**No new P0/P1/P2/P3 product defect was reproduced in the completed scope. No new
repair implementation milestone is justified by G28-R.** Continue using the
current application model and return this evidence to Golden Wrench/Boss. This
is a bounded readiness assessment, not a claim that every application or platform
is qualified. If Windows one-worker async-host acceptance is required for a
specific product, first provision the existing matching test targets and run the
maintained driver; missing audit executables do not justify changing the language
or Runtime. Historical limitations retain their existing decisions.

## Research inventory and retention

Repository deliverable: this report only. Scratch:
`/tmp/void-g28r-_ointuh5`. Native Windows task root:
`C:\Users\a\VoidG28Research-20261113`. Transport:
`/home/shmellyorc/VM/g28r-20261113`. Coordinators, exact inputs/identities, original
failure and successful receipts are retained selectively under the established
verified ZIP policy. No G26/G27 evidence, active build, SDK or provider catalog
is removed. The disposition below records verified cleanup.


Verified archive: `/home/shmellyorc/Documents/Void.Language-archives/g28-real-applications-2026-11-13.zip`.
SHA-256: `5ebd237f63c1852be804d8cf2b2bb4a09807bf80922d6b27b75b2968623a9302`.
It contains **137 selected files**, 1,859,475 uncompressed bytes, in **291,292 bytes**
(ordinary ZIP DEFLATE level 9). CRC, actual extraction and every restored member's
SHA-256 passed; selected originals were rehashed. The external existing
`archive-index.json` records this archive.

The embedded manifest records member paths, absolute original paths and hashes.
Retained material includes exact coordinators and replay instructions, binary and
source identities, original permission and missing-executable failures, successful
profile receipts, pinned-consumer identity and extra application sources/manifests.
Routine executables, objects, copied native packages, redundant vendor trees and
screenshots are reproducible and excluded. The replay note identifies the forty
unused pre-existing Linux cache entries incidentally copied in the Windows input
packet; they are unnecessary for the uncached source probes and are not archived.
All 79 transported inputs were verified unchanged before guest cleanup.

Task-only storage before final cleanup:

| Location | Logical bytes | Allocated bytes where measured |
| --- | ---: | ---: |
| Linux audit scratch | 46,912,114 | 47,443,968 |
| Host↔guest transport | 7,361,777 | 7,487,488 |
| Private Windows NTFS audit root | 56,567,372 | Not measured |

These are end-of-test inventories, not peak usage. Successful maintained drivers
already removed their own larger temporary outputs. Cleanup requires verified
archiving and checks the guest file inventory before deleting its exact task root.
No active compiler build, SDK, provider catalog, G26/G27 archive or retained G26
consumer is included. The original G26 consumer's 59 file-content hashes remain
identical to the initial identity record.

The three exact task roots and the task-location pointer were removed after
verification. Free space after cleanup: `/tmp` **16,512,016,384 bytes**,
host root filesystem **362,357,272,576 bytes**, Windows C: **8,810,258,432 bytes**.
Before final cleanup the corresponding values were 16,464,568,320,
362,382,675,968 and 8,753,635,328 bytes. Filesystem-wide differences include
concurrent host/VM activity and are not claimed as exact recovered file bytes.
Cleanup receipts are retained in the existing external archive index.

Final Git inventory: HEAD remains `e3e6018430ac68235d2d91fd4b7ce614aa8ce3b6`;
tracked working-tree and staged diffs are empty. The sole untracked repository
deliverable is `docs/compiler-health/G28_REAL_APPLICATION_READINESS.md`.
No production/source/test/build edits, staging, commit, branch, push, package
publication or hosted CI occurred. **Stop for Golden Wrench/Boss review.**
