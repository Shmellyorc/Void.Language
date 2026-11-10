# Stage 9 application workflow — 21 October 2026

The [Phase 1 matrix](PROJECT_ARCHITECTURE_ROADMAP.md#phase-1-compatibility-and-migration-matrix)
freezes proposed manifest-only discovery, explicit project-entry precedence,
manifest-root VFS/output behavior and unchanged bare explicit-file semantics. The
[later argument implementation](voidup/PHASE_B_REPORT.md) already supports literal
application forwarding after an explicit selection and `--`; the original Stage 9
no-application-operands wording below is historical for that extension. Phase 1
preserves actual forwarding, legacy precedence and eager-global order. HOLD remains.

## Later approved project direction, 8 November 2026

The [project architecture roadmap](PROJECT_ARCHITECTURE_ROADMAP.md) records the
approved replacement for filename-based discovery, native `void.project.toml`,
embedded app identity, project commands and general typed C linking (dynamic by
default, static optional). **Planning only:** the Stage 9 implementation and all
CI evidence below retain their original scope. Never require a hardcoded filename
or mandatory `entry` in the future manifest. Compiler analysis will select one valid
main in the selected application source graph or diagnose ambiguity; explicit
entries, top-level/legacy classification and libraries without main remain supported.

## Stage 11 global declaration integration

File-level globals coexist with main or legacy entry and initialize before the
selected entry runs. They no longer count as executable script statements for
entry competition. A selected file containing only globals and ordinary helpers
initializes and exits zero; invalid main signatures retain their existing entry
error. Remaining script statements retain their order after all outermost
initializers; nested block vars remain lexical. Selection/discovery, arguments,
status policy and native publication are unchanged. See [RUNTIME_GLOBALS.md](RUNTIME_GLOBALS.md).


Boss authorized all three milestones as one mission. This supersedes Sprint 4's
mandatory application flag and the research proposal's optional compatibility
flag. Entry language semantics, native admission and source identities are unchanged.
[Acceptance and review handoff](stage9/ACCEPTANCE.md) separates local evidence from
Boss-controlled native hosted verification.

## Commands

See the [current CLI reference](CLI.md) for help/version, `--no-cache`,
operand delimiters, examples and installation diagnostics.

```text
voidc check <file-or-directory>
voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--cache]
voidc build [file-or-directory] [-o executable] [--no-opt]
voidc emit <file-or-directory> -o <object> [--no-opt]
```

At the inspected baseline, absent run/build paths select cwd. Directories use
shared deterministic exact-case
`main.void` discovery. Explicit `.void` files may have any filename; their parent
is the project root. Options may precede or follow operands. Unknown/duplicate
options, removed `--app`, unsupported application operands, cache outside run and
missing/extra operands are invalid invocations (status 2). `check --no-opt` remains accepted as an inert existing spelling. Cache retains its
existing Debug policy and safe unavailable fallback; it disables optimization.

All commands use the compiler-owned `ValidatedAnalysis`. Checking accepts libraries;
execution/emission require the validated selected entry. Run uses retained shared
LLVM lowering, including existing JIT/cache/source snapshot/reload contracts.
To supply legacy input for cwd, spell the path explicitly (`voidc run . 5`).
Legacy `entry(i32) -> i32` keeps precedence, optional decimal i32 input, printed
result and successful process status zero. Applications accept no operands, return
their i32 status without printing it, and no-result main/top-level fallthrough
return zero. Typed main requires explicit return on every reachable path.
Imported executable statements and conflicting entry styles reject; an imported
main is never selected. No source scanning or separate parser chooses the mode.
The planned compiler-owned source inventory changes discovery only after separate
implementation; it must not introduce another parser or weaken entry validation.

## Native artifacts and resources

Build emits the existing source object and links the existing static Runtime and
fixture/sink archive through configured Clang **23.1.4**, using argv process
execution. Linux's configured matching LLD retains its version gate. Windows uses
the active SDK/MSVC/UCRT environment and the selected LLVM SDK's CRT policy
(LLVM 23.1.4 requires release `/MT`; see [the repair record](llvm23/CROSS_PLATFORM_REPAIR.md)); macOS receives the
configured SDK, architecture and deployment target. This is host compilation.
Source imports never grant native binding or linker permissions.

Default executable: `<selected-root>/build/<entry-stem>` with `.exe` on Windows.
An explicit `-o` is caller-relative or absolute. Emit remains object-only.
Outputs cannot overwrite selected/imported sources or name `.void` files. Existing
output symlinks, non-files and incorrect physical spelling reject. Physical
identity catches hard-link collisions. [CI #88's repair](stage9/CI88_WINDOWS_PATH_REPAIR.md)
admits exact OS-registered Windows short directory ancestors; ordinary descendant
case and output-file spelling stay exact. Directory reparse points reject on
Windows; POSIX/macOS ancestor-symlink behavior remains unchanged. No complete
output canonicalization changes source identities. Same-parent private staging preserves old
artifacts through source/backend/toolchain/link failures; failed intermediates are
removed. POSIX rename and Windows replacement publish only a successfully produced
artifact. A Windows executing/locked destination can reject replacement without
removing the old executable. Concurrent external source/output mutation is not a
filesystem transaction; frontend captured snapshots remain authoritative.

LLVM unique-path generation is applied only to a generated temporary **leaf**;
percent characters in physical ancestors remain literal. Linux/macOS keep the
existing destination-stage working directory and relative executable output name.
Historical motivation: on Windows, Clang 19.1.7 could not enter a genuine
destination-stage path longer than 260 characters. The retained build strategy emits/links in a short private directory under a validated
Windows temporary root, using relative `program.o`, `runtime.lib`, `fixture.lib`
and `program.exe` names. The complete linker path, including NUL, is bounded to
240 UTF-16 units. Creation is exclusive, collisions retry without adoption, and
native protected owner-only DACLs are verified on both Windows staging directories.
Ancestor handles reject/pin reparse paths; insecure/overlong temporary roots fail
closed. The full project location, analysis and VFS identities remain unchanged.

The generated Windows file must pass LLVM PE/section validation and private-file
checks. Native checked copying uses CREATE_NEW, flush, size and byte comparison
before transfer into existing destination staging. Revalidate the requested
output and call the existing `publish()` unchanged: cross-volume copying precedes
same-parent atomic replacement, with no COPY_ALLOWED fallback. Owned temporary
paths are removed on success/ordinary failure. Locked destination, failed copy,
compile, link or permission checks preserve the previous executable. No global
cwd change or shell expansion occurs. The [permanent Windows engineering record](stage9/WINDOWS_ENGINEERING_RECORD.md)
records CI #95 native acceptance at `e25816bdb80e3f32613edf77c9cf3258232af966`:
Windows Debug/Release 160/160 each, 320/320 combined. The blocking investigation
is RESOLVED; [CI #90's original implementation/evidence](stage9/CI90_WINDOWS_NATIVE_LINK_REPAIR.md)
remains historical. Actual second-volume transfer and a complete seven-job matrix
are not claimed by this acceptance.

CMake generates `void-native.txt` beside the actual voidc target, using target-file
generator expressions (including multi-configuration paths). It lists the pinned
host driver/linker, exact Runtime/provider archives and host link arguments.
There is no cwd/PATH/recursive resource fallback. Missing/malformed configuration,
missing/nonexecutable resources and mismatched driver/linker versions diagnose.
Linker child diagnostics are retained and a nonzero child status is labelled.
Generated applications have no LLVM compiler-library dependency.

```text
cmake --install <build-tree> --config <configuration> --component compiler --prefix <private-prefix>
```

The private layout is `bin/voidc[.exe]`, `bin/void-native.txt`, and
`lib/void/<Runtime/provider archives>`. Installed archive paths are relative to the
configuration file, so the prefix can relocate. The configured matching host SDK
and platform development dependencies remain required at their configured paths;
this does not distribute/download Clang, platform SDKs or system libraries.
Standard-library source selection retains the existing configured root/absolute
`VOID_STDLIB_ROOT` override contract; no new stdlib search or package manager ships.

## Failures and editor integration

Usage errors exit 2; source/backend/runtime/tool/link errors exit 1 with their own
diagnostic context. An application's deliberate nonzero result is returned without
inventing a failure message. Platform exit-status representation applies.
Checked AOT failures now use the platform CRT's stderr, retaining existing source
messages and bounded caller traces. Normal/O2 JIT and real linked executables are
compared on status, stdout and stderr, including exact failure text. Scalar objects
still need only their existing CRT link contract; no new Runtime dependency is
introduced just to print a diagnostic. Runtime cleanup/diagnostic ownership are
unchanged.

voidls and adapters retain shared discovery, local/standard import isolation,
overlays/snapshots, UTF-16, incomplete-source recovery and navigation/completion/
documentation/shutdown. No editor compiler or path resolver was introduced.

No project package manager, network fetching, project manifests, arbitrary FFI, cross compilation,
new language feature, Git mutation, hosted execution or public README edit is part
of this implementation.
