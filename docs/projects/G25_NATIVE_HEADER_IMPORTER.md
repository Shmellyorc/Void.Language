# G25 — general C header importer

Starting and unchanged HEAD: `b61a32193fad616ae3863b01809533ab4a721cde`.
Qualification date: 13 November 2026 (America/Vancouver). The initial worktree was
clean. This is an uncommitted implementation; no staging, commits, pushes or
hosted CI were performed. G24's canonical package remains unchanged.

## CLI and generated files

```text
voidc native import HEADER --provider ID --out NEW-DIR
    [--source /native.void] [--clang PATH] [--language c|c++]
    [-- -Ipath -Dname=value -Uname]
```

The existing verified native SDK supplies the default Clang driver. An explicit
`--clang` must report pinned 23.1.4. The supported host profiles remain x86-64 GNU
Linux and x64 MSVC Windows. The destination's parent must exist; the destination
must be new. The module path is relative to the application/package root, so a
project's `src/ffi` output uses `--source /src/ffi/native.void`.

Typical output:

```text
api.void                 editable wrappers, constructors and numeric constants
native.void              physical G24 declarations, verified against descriptor
provider.descriptor      conventional revision-4 / native ABI 3 candidate
imported.h               original-header include, exact POD aliases, C exports
imports.c                fixed-prototype value forwarders
coverage.json            original declarations, locations, categories and reasons
README.import.txt        review/build/approval instructions
```

C++ mode uses `imports.cpp` and adds `checked.cpp`, which includes the existing
native-bind output under C linkage after parsing the original header. It supports
eligible top-level `extern "C"` value functions, `noexcept` declarations and plain
POD records. Classes, methods, templates, namespaces, ordinary overloads and
exception transfer are explicit exclusions requiring a supplied C interface.

A successful small-fixture import reports **11 generated functions**, four POD
records, five resolved typedefs, two enumerators, four numeric object macros and
one excluded header guard. The extra C names and export macro deliberately
collide with the generator's usual names: C symbols, record aliases, probe names
and export macros are selected against existing Clang declarations/preprocessor
names. The original `vdi_call_0` executes correctly, rather than becoming a
recursive adapter. The report records the actual export macro.

The [developer guide](../NATIVE_LIBRARIES.md#g25-import-a-c-header-explicitly)
contains a complete compiled/catalog-approved example. That exact example was
executed and prints `42` in normal/O2 JIT and native AOT. The fixtures live under
[tests/ffi/native_import](../../tests/ffi/native_import/).

## Exact representation and admission

For example:

```c
typedef struct G25Point { int x; float y; } G25Point;
G25Point G25Shift(G25Point, int);
```

The importer obtains the record's actual size, alignment and field offsets from
Clang. Its descriptor contains the corresponding `record`, `field` and `bind`
entries, with a unique C typedef alias and a fixed C forwarding symbol:

```text
record g25_point <unique-C-alias> 8 4 2
field x x c_int 0
field y y f32 4
bind g25_shift <unique-C-forwarder> record:g25_point record:g25_point c_int
```

Those placeholders denote the actual collision-free names in the generated
files, not valid literal descriptor tokens. The exported real declaration is:

```void
native "org.voidlang.g25.values"
# Other exact record declarations precede functions.
func g25_shift(arg0: g25_point, arg1: i32) -> g25_point:
    pass
```

The editable public implementation imports `./native.void` and calls it normally:

```void
func g25_shift(p0: native.g25_point, p1: i32) -> native.g25_point:
    return native.g25_shift(p0, p1)
```

`api.void` is ordinary source, and `native.void` is a real descriptor-selected
file. Neither import nor application compilation creates a virtual source module.
Unsupported declarations do not appear as callable pass bodies.

The only ABI admission adjustment allows **conventional revision 4 value-only
descriptors without an invented resource**. Revision 3 still requires its owned
resource contract. ABI 3, the exact descriptor grammar, typed IR, layout/signature
verification, dynamic closure, loader, resource ledger, G5 callback generation
ownership and retirement remain the existing mechanisms. All native declaration export
and candidate ABI validation reuse `CPrimitiveAdmission::generate_declarations` and
its authoritative parser. `native-bind` remains the checked adapter generator.

Clang's JSON AST, record-layout dump and preprocessor output supply declarations,
layouts, typedefs and active macros. Clang evaluates numeric expressions in
explicit probes; the importer contains no C parser, preprocessor or expression
engine. Scalar mappings translate Clang's types to the existing descriptor
profile. Fixed value forwarders preserve original C names/types in the report
while normalizing compatible numeric spellings. These are generated C adapters,
not a claim of new raw-pointer ABI coverage.

The generator is confined to its explicit CLI command. The new SDK-driver helper
reuses existing native-toolchain verification. Output publication uses the
existing exclusive project-directory publisher and its pinned staging checks.
Scratch Clang output is temporary and removed; there is no new cache, registry,
package resolver, loader or Runtime subsystem.

## Coverage and exclusions

Coverage is per active declaration in the requested header. Included headers
supply scalar typedef resolution; their APIs are not automatically selected.
Reports retain original source file/line/column, declaration text when supplied
by the AST, C type, normalized VOID name, category and actionable reason. They
also record the header/preprocessing hashes, target, language and flags.

| Category | Behavior |
|---|---|
| `generated` | Supported fixed value functions, compatible POD records/arrays, resolved scalar aliases, enums and numeric constants |
| `reviewed_adapter_required` | Pointer/opaque ownership, buffer direction/extent, callback lifetime/barriers, or values/layouts requiring a C adapter |
| `unsupported` | Varargs, C++ interfaces outside the C profile, duplicate/colliding VOID declarations, globals and unsupported macros/constants |

Existing native bounds remain: 16 arguments, 64 records, 64 fields, 4096-byte
records, alignment at most 8 and one-dimensional fixed arrays with 1..32 elements.
Packed/over-aligned/qualified/bitfield records, flexible arrays and unions require
an explicit supported representation. Distinct C record-field types missing an
exact existing scalar alias are excluded even when widths coincide; numeric
function forwarders handle the equivalent scalar values. Arbitrary varargs and
callback lifetimes are never guessed. Global objects, function-like/string/config
macros and nonconstant expressions are reported. Enum constants beyond signed
64-bit evaluation and non-finite/negative-zero floating macros require explicit
representation. Macro undefinition/redefinition is accounted for.

A header with no supported callable operation produces coverage and any usable
pure numeric `api.void` constants, with `descriptor_ready_for_review=false`.
Existing native descriptors require a callable binding; records-only headers
report that specific limitation. No dummy operation or native callable is
published to work around it.

## Independent C, C++ and edit/failure proofs

`tests/project/native_import.py` qualifies the small C fixture, C++ C interface,
source editing and negative cases. It executes real computations involving
signed/unsigned scalars, bool, target-dependent `unsigned long`, `size_t`, large
64-bit values, enums, float/integer constants, nested records, typedef-owned
anonymous records and fixed record/char arrays. It also checks generated-name
collisions against actual original C functions/types/macros.

Normal/O2 JIT and native AOT return **42**. Editing the existing generated wrapper
in a disposable project changes each result to **43** without running a generator.
A native declaration signature edit is rejected against the descriptor. Changing
the original record layout after import causes native-bind's generated C layout
assertions to fail. Reimport into the edited output directory is refused and all
edited bytes remain intact.

A further regression reproduces a large enum outside the signed-64 evaluator:
the importer rejects that value and its implicit successors until an explicit
representable reset. It does not emit an invented zero. The test checks Clang’s
GNU/MSVC enum-width difference. The small C/C++/edit/rejection fixture was rerun
on the final generator in all five profiles; SQLite contains no enum declarations,
and its qualified generated API bytes remain identical after this correction.

Rejections include C++ class/template/overload/namespace declarations, arbitrary
varargs, unresolved pointer/opaque/callback contracts, unions, flexible arrays,
bitfields, packed records, duplicate declarations, unsupported macros, missing
headers, invalid provider/source/output paths and overwrite attempts. Header/tool
failure leaves no misleading partially published destination. Constants-only and
unsupported-only headers prove the zero-callable behavior separately.

## Real SQLite 3.53.4

The [official pinned amalgamation](https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip)
was downloaded once for qualification; tests themselves perform no network fetch.
The [pin file](../../tests/ffi/native_import/sqlite.pin.json) verifies archive
SHA3-256 plus `sqlite3.h`/`sqlite3.c` SHA-256 before compilation. Actual native
execution checks `sqlite3_libversion_number() == 3053004`.

On the Linux header profile, **298 function declarations** are accounted for:
18 automatic fixed-value forwarders, 272 requiring reviewed ownership/adapter
contracts, and eight unsupported varargs functions. The complete report has 527
`generated`, 359 `reviewed_adapter_required` and 30 `unsupported` rows, including
typedefs, records, globals and 523 macros. These row totals are not callable counts
and do not claim a complete SQLite API.

The [small handwritten SQLite fixture](../../tests/ffi/native_import/sqlite_case.py)
provides the review that the header cannot: an owned in-memory database, explicit
open/null failure, a query that creates/inserts/selects the sum `42`, statement
finalization on every path, and consuming close. It extends the same descriptor
with one G3/G4 resource and three fixed operations, exports physical declarations
with existing native-bind, and builds the real pinned SQLite source into the
native provider. No SQLite logic enters the compiler or stdlib.

An ordinary D4 catalog records the real descriptor/runtime/notices and, on
Windows, the generated COFF import library in `native.link`. Resolve, locked
resolve, explanation and normal source checking pass. Normal/O2 JIT and AOT open,
execute, obtain the result and close. A deliberate VOID bounds failure releases
the database through the existing cleanup path in normal/O2 JIT and AOT, with an
observable release marker. The catalog, source reports, reviewed descriptor and
locks are retained with the qualification artifacts.

## Native qualification and regressions

| Native configuration | C/C++ importer, edits and rejections | SQLite normal/O2 JIT/AOT + failure cleanup | Focused existing FFI/project suites | G24 600 / callbacks / Git/JIT/AOT/D5 |
|---|---|---|---:|---|
| PikaOS Debug | PASS | PASS | 29/29 | PASS |
| PikaOS Release | PASS | PASS | 29/29 | PASS |
| Linux strict ASan/UBSan/leak | PASS | PASS, no reports | 29/29 | PASS within headless/bounded ownership scope |
| Native Windows 10 Debug | PASS | PASS | 9/9 | PASS |
| Native Windows 10 Release | PASS | PASS | 9/9 | PASS |

Debug/Release Linux qualify the two corrected legacy fixture cases by targeted
replay; the other 27 passing results are retained. Strict passes all 29 in its
focused run. Windows Debug retains seven passing cases plus the two installed-G5
cases replayed after supplying the missing license inputs; Release passes all nine.
The final generator-only large-enum correction is additionally replayed through
the full small C/C++/edit/rejection fixture on all five native builds. SQLite's
API bytes are unchanged by that correction. Default configured Clang selection
passes on Linux and both Windows builds.

Native VS Code **1.141.0** passes **6/6 generated-source checks** on Linux and
Windows Debug/Release: physical wrapper/native definitions, an actual saved source
edit, Check Project, execution of edited result **44**, and no consumer errors.
The original G23 project fixture passes **37/37 each** on all four editor builds,
including Check/Build/Run/Package, real package verification, dirty-buffer refusal
and diagnostic recovery. Only editor tests/runner change; production extension
behavior remains intact.

All LLVM/Clang/LLD tools report 23.1.4. The native Windows receipt records VS
2022 `VSCMD_VER=17.14.42`, x64 host/target, VCTools 14.44.35207 and Windows SDK
10.0.19041.0. OS release/version is `10/10.0.19045`.
Both Windows qualification overlays restore original source bytes after testing.
The exact source importer SHA-256 matches the Windows-qualified source packet.

Strict Linux uses:

```text
ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
```

The expected VOID bounds error is asserted separately from sanitizer diagnostics;
ASan/UBSan/LSan reports still fail qualification. Current G25 native resources are
released during success and checked failure. Existing full-device raylib leaks
retain their separate classification below.

Machine evidence is consolidated in `build/g25/handoff/linux-identities.json`,
`windows-identities.json`, the five SQLite source/native `void.lock` files, editor
receipts and the final `receipt.json`. Full compilation/execution logs and initial
harness failures remain in the Linux/Windows evidence directories. Header and
preprocessing reports, exact reviewed native descriptors, runtime/link/notice
identities and library hashes are preserved in the copied SQLite projects/catalogs.

Linux source/build/evidence lives under `build/g25/`. Windows evidence is under
`/home/shmellyorc/VM/g25-20261113/` with native work in
`C:/Users/a/VoidG25Native`. LLVM/Clang/LLD remain pinned to 23.1.4. Windows uses
VS 2022 x64 Native Tools, host/target x64, in the established native Windows 10
VM (the VM's display name is “Windows 11”). Native dependency catalogs are the
previously verified G24 inputs, and SQLite is independently pinned above.

Focused replay exposed two pre-existing fixture assumptions: G1 expected the
pre-G24 invalid-path wording, and G4's hard-link test crossed `/tmp` and the
repository filesystem. The G1 oracle now expects the exact integrated rejection;
G4 uses the existing `VOID_TEST_DIAGNOSTICS_DIR` on the repository filesystem.
Neither rejection was weakened. The intentional missing-G1-thunk link failure
remains a passing negative test; it is not a prebuild target.

The Windows qualification overlay initially omitted existing install-license
inputs and `tools/lsp_client.py`; those inputs were supplied without production
changes. Completed Debug Git acquisition, locks, JIT/AOT/D5 and tamper stages were
retained and the previously unexecuted LSP stage was resumed successfully.
Initial harness attempts remain in raw logs. No failure was hidden by changing
D3 bounds, bypassing source/native verification or disabling sanitizers.

## Existing G24 boundaries

No file under `verified-packages/raylib/` changes. All 600 interfaces remain
accounted for as 236 direct C ABI mappings, 352 ownership adapters, ten G5
lifecycle adapters and two typed equivalents. Symbols/prototypes/layouts,
physical-source editing/signature guards, all ten callback lifecycles and G5
reload/retirement, and Git consumer JIT/AOT/D5/lock-integrity paths are checked
using existing fixtures. The reviewed package is not regenerated or overwritten.
Read-only parity checks preserve its source bytes.

The independent [external device-leak findings](G23_G24_IMPLEMENTATION.md#strict-leak-classification)
remain retained and unfixed: Gallium 256 bytes, raylib WAV music context 408 bytes,
and sound-alias residual 64 bytes. Current strict C/SQLite/headless/bounded
callback ownership tests are clean with fatal ASan/UBSan and leak detection;
that does not relabel the separate full-device failures as clean. No sanitizer
suppression or relaxed native acceptance was introduced.

Source-package acquisition remains the existing immutable Git workflow. The
prior standalone raylib publication proposal and public-origin limitations remain
separate; G25 does not publish a repository or run hosted CI.

## Exact source changes and final Git checks

Production changes are confined to CLI/tooling and the existing admission:

- `src/main.cpp`: one `native import` command, help and explicit options.
- `src/native_import.cpp` and `include/void/native_import.hpp`: the explicit Clang-based generator, coverage, value adapters and exclusive publication.
- `src/native_build.cpp`: reuse the verified native SDK's existing driver selection.
- `src/ir/c_primitive.cpp`: conventional revision-4 value-only admission; revision-3 ownership remains required.
- `CMakeLists.txt`: link the CLI generator and register the existing Python test style.
- Test fixtures/editor harness and documentation: independent C/C++/SQLite qualification, actual source edits, failures and the G1 diagnostic oracle correction.

No frontend language, IR, Runtime, native loader, callback framework, dependency
resolver, source-package layout or production editor behavior changes. All new
VOID is ordinary physical output. The compiler's application paths do not call
the importer. The helper maps Clang metadata onto existing ABI shapes and reuses
the existing parser/exporter/checked C adapter and directory publisher.

The exact changed paths relative to the starting commit are:

```text
 M CMakeLists.txt
 M docs/NATIVE_LIBRARIES.md
 M docs/roadmap/MILESTONE_REGISTER.md
 M editors/vscode/src/test/runRaylibHost.ts
 M src/ir/c_primitive.cpp
 M src/main.cpp
 M src/native_build.cpp
 M tests/ffi/primitives.cpp
?? docs/projects/G25_NATIVE_HEADER_IMPORTER.md
?? editors/vscode/src/test/nativeImportHost.ts
?? include/void/native_import.hpp
?? src/native_import.cpp
?? tests/ffi/native_import/cpp.cpp
?? tests/ffi/native_import/cpp.hpp
?? tests/ffi/native_import/rejections.h
?? tests/ffi/native_import/sqlite.pin.json
?? tests/ffi/native_import/sqlite_case.py
?? tests/ffi/native_import/values.c
?? tests/ffi/native_import/values.h
?? tests/project/native_import.py
```

`git diff --check` passes; untracked files are checked separately with
`git diff --no-index --check`. HEAD remains
`b61a32193fad616ae3863b01809533ab4a721cde`, with no staged changes. The final
status, changed-file SHA-256 values and machine receipts are retained under
`build/g25/handoff/`. No staging, commits, pushes, public repository publication
or hosted CI were performed. Existing disposable Git fixtures create test commits
only in their temporary repositories.
