# Trusted static native providers

## G25: import a C header explicitly

`voidc native import` inspects a header with pinned Clang 23.1.4 and publishes
editable candidate files into a **fresh directory whose parent already exists**.
The importer runs only when requested. Normal check/build/run reads the saved VOID
source and the authoritative admitted descriptor.

```text
voidc native import HEADER --provider ID --out NEW-DIR
    [--source /native.void] [--clang PATH] [--language c|c++]
    [-- -Ipath -Dname=value -Uname]
```

`--clang` defaults to the existing verified compiler SDK configuration. The target
is the host's existing Linux x86-64 GNU or Windows x64 MSVC profile. Use attached
preprocessing arguments after `--`, and use the same configuration when compiling
the native library. `--source` is the physical declaration module's canonical path
relative to the application/library root. Its default `/native.void` is suitable
when that file is at the root; placing output in `src/ffi` requires
`--source /src/ffi/native.void`.

| Output | Purpose |
|---|---|
| `coverage.json` | Each selected declaration's original location/type/text, category, exclusion reason, normalized name, input hashes, target and preprocessing |
| `native.void` | Physical, descriptor-checked native declarations using the G24 `native "provider"` contract |
| `api.void` | Editable ordinary wrappers, record constructors, enum values and numeric constants |
| `provider.descriptor` | Candidate conventional revision-4/ABI-3 contract for review and existing native-bind |
| `imported.h`, `imports.c` | Header include, exact POD aliases and fixed-prototype value forwarders |
| `imports.cpp`, `checked.cpp` | C++ mode's equivalent forwarders and C-linkage wrapper around native-bind's C output |
| `README.import.txt` | Required review, compilation and approval steps |

Review the report's `generated`, `reviewed_adapter_required` and `unsupported`
rows. Selected APIs come from the requested header; included headers supply scalar
typedef resolution. Import each public header explicitly when an umbrella header
only includes other files. C names/types remain in the report; snake-case VOID
names and reserved-name adjustments are explicit. Generated value forwarders
normalize C integer spellings to existing descriptor aliases without assuming
ownership. Original symbols are called from these forwarders, not exposed as a
new pointer ABI.

Automatic coverage includes fixed scalar functions, compatible natural POD
records, named/typedef-owned anonymous records, nested records, fixed scalar/record
arrays, enums and Clang-evaluated integer/finite floating object macros. Enum values
use their derived integer representation. Macro undefinition/redefinition is
accounted for. No C expression evaluator or preprocessor is implemented in VOID.

The existing ABI bounds remain: 16 fixed arguments, 64 records, 64 fields per
record, natural alignment at most 8, record size at most 4096 bytes, one-dimensional
arrays of 1..32 elements, and the existing descriptor/function bounds. Packed,
over-aligned, qualified or bitfield records need a reviewed value-copy adapter.
Distinct C field types missing an existing exact scalar alias are reported, even
when their widths match. Function value forwarders handle those numeric spellings.
Pointers, output buffers, opaque owners and callbacks require explicit extent,
allocator, acquire/release, retention and drain contracts through G3/G4/G5.
Arbitrary varargs, unions, flexible arrays, globals, function-like/string/config
macros and nonconstant expressions are not silently exposed. Enum constants beyond
the signed 64-bit evaluator profile, non-finite/negative-zero floating constants
and unsupported C scalar types require explicit action.

For C++ use `--language c++` and an explicit top-level `extern "C"` interface.
Eligible value functions and POD structs are supported; classes, methods,
templates, namespace APIs, overloads and exception transfer require a supplied C
wrapper. Review that no exception escapes any imported C-compatible function.
The generated C++ glue parses the original header before placing the checked
adapter under C linkage. It adds no C++ object or exception ABI.

A header with no callable supported functions produces coverage and any pure
numeric `api.void` constants, with `descriptor_ready_for_review = false`. Existing
descriptors need at least one callable operation; records-only headers identify
that limitation rather than adding a dummy callable. Syntax/tool/input failures
publish no destination. Existing destinations, even identical ones, are refused.
Regenerate to a fresh directory, compare results, and retain developer edits.

### Complete Linux example using the independent fixture

Run from the repository root with the matching compiler on PATH. These are
explicit development commands; dependency fetch never executes this process.

```bash
G25_ROOT="$PWD"
G25_DEMO=$(mktemp -d)
G25_CLANG="$G25_ROOT/.toolchains/llvm-23.1.4/bin/clang"
voidc new "$G25_DEMO/app"
voidc native import "$G25_ROOT/tests/ffi/native_import/values.h" \
  --provider org.example.values --out "$G25_DEMO/app/src/ffi" \
  --source /src/ffi/native.void
# Review coverage, effects, declarations and adapter inputs before continuing.
voidc native-bind "$G25_DEMO/app/src/ffi/provider.descriptor" imported.h \
  -o "$G25_DEMO/app/src/ffi/checked.c"
mkdir "$G25_DEMO/catalog"
"$G25_CLANG" -std=c11 -shared -fPIC -fuse-ld=lld -O1 \
  -I"$G25_DEMO/app/src/ffi" "$G25_ROOT/tests/ffi/native_import/values.c" \
  "$G25_DEMO/app/src/ffi/imports.c" "$G25_DEMO/app/src/ffi/checked.c" \
  -o "$G25_DEMO/catalog/libvalues.so"
cp "$G25_DEMO/app/src/ffi/provider.descriptor" "$G25_DEMO/catalog/"
cp "$G25_ROOT/LICENSE" "$G25_DEMO/catalog/VOID-MIT-LICENSE.txt"
export G25_DEMO
python3 - <<'PY'
from pathlib import Path
from hashlib import sha256
import os
p=Path(os.environ['G25_DEMO'])/'catalog'
def identity(name):
    data=(p/name).read_bytes()
    return f'{len(data)}:{sha256(data).hexdigest()}'
(p/'void.native.toml').write_text(f'''format=1
[native]
id="org.example.values"
version="1.0.0"
abi=3
target="x86_64-unknown-linux-gnu"
revision="g25-example-1"
descriptor="provider.descriptor"
descriptor-identity="{identity('provider.descriptor')}"
library="libvalues.so"
adapter="libvalues.so"
[native.runtime]
"libvalues.so"="{identity('libvalues.so')}"
[native.link]
[native.notices]
"VOID-MIT-LICENSE.txt"="{identity('VOID-MIT-LICENSE.txt')}"
''')
PY
cat >> "$G25_DEMO/app/void.project.toml" <<'TOML'

[native-requirements.values]
id="org.example.values"
version="=1.0.0"
abi=3
TOML
cat > "$G25_DEMO/app/src/app.void" <<'VOID'
use "./ffi/api.void" as ffi
func main() -> i32:
    var point = ffi.g25_shift(ffi.g25_point(20, 1.5), 2)
    print(ffi.g25_add(point.x, 20))
    return 0
VOID
# Explicit approval of the reviewed, locally built catalog:
export VOID_NATIVE_CATALOG="$G25_DEMO/catalog"
voidc resolve --project "$G25_DEMO/app"
voidc resolve --locked --project "$G25_DEMO/app"
voidc check --project "$G25_DEMO/app"
voidc run --project "$G25_DEMO/app" --allow-native --profile debug
voidc run --project "$G25_DEMO/app" --allow-native --profile release
voidc build --project "$G25_DEMO/app" --allow-native -o "$G25_DEMO/out/app"
"$G25_DEMO/out/app"
```

Each execution prints `42`. On Windows use the VS 2022 **x64 Native Tools Command
Prompt**, pinned Clang/LLD 23.1.4, a DLL output and matching COFF import library.
Record the DLL in `native.runtime` and the generated `.lib` in `native.link` with
actual identities; every catalog file must have its existing D4 role. The native
Windows fixture qualifies this path. For strict Linux instrument the provider
with ASan/UBSan to match the strict compiler/Runtime; keep leak detection enabled.

The [pinned SQLite fixture](../tests/ffi/native_import/sqlite_case.py) demonstrates
an explicit database owner: open an in-memory database, create/insert/select,
finalize statements and consume the owner with close. The importer reports its
pointer APIs for review. That small handwritten wrapper extends the generated
descriptor using the existing resource ledger; it is not an automatic ownership
inference. Pinned source identities are in
[`sqlite.pin.json`](../tests/ffi/native_import/sqlite.pin.json).

Header import derives declarations. `native-bind` verifies the reviewed descriptor
and emits the checked adapter. Native compilation creates binaries. The approved
catalog records actual binary/link/notice identities. Resolution locks those
identities, and execution requires the existing opt-in. A separately locked Git
source package verifies source integrity; it grants no native execution permission.
Local edits compile normally, while edits to fetched locked snapshots remain
subject to the existing source-lock checks. No new source package workflow is
introduced. See [G25 qualification](projects/G25_NATIVE_HEADER_IMPORTER.md).


## Physical C declarations, G24 source conversion

The conventional C ABI now also accepts `source-file /src/interop.void` in an
approved descriptor. This selects an ordinary project or package file instead
of the legacy `source /interop.void` virtual module. The real file begins with
`native "provider.identity"`, followed by the complete descriptor's resources,
records and function declarations. Native function bodies contain only `pass`.
The marker grants no native approval: a matching admitted descriptor is required.

The existing admission checks the provider, source path, complete inventory,
sealed resources, record layouts and every function signature, including unused
declarations. Executable bodies/default arguments, extra declarations and two
matching modules are rejected. Package identities are contextualized through the
existing source graph; loading, ABI transport, G5 callbacks and generation
ownership remain the existing mechanisms. Legacy virtual descriptors still work.

`voidc native-bind descriptor header.h -o NEW.void` explicitly exports initial
declarations for a `source-file` descriptor. As with C output, the destination
must be new. Compilation never invokes this export or replaces source bytes.
Normal relative imports, dependency lock verification and LSP physical locations
apply. See the [physical-source qualification](projects/G24_PHYSICAL_SOURCE_IMPLEMENTATION.md).

The sections below retain their original implementation history and scope.

Slice 1 now validates application/library metadata and rejects nonempty reserved
native/dependency declarations through the explicit compiler-library parser only.
[Engineering evidence](projects/SLICE1.md) records the qualification. Source-library
analysis, project admission, resolution and native linking/loading remain deferred;
current import/provider behavior is unchanged.

The [Phase 1 manifest freeze](PROJECT_ARCHITECTURE_ROADMAP.md#proposed-manifest-and-identity-model)
reserves `native`/`dependencies` but rejects nonempty declarations in schema 1.
No new provider/linker admission is designed into initial project creation or
analysis. Existing provider/resource receipts remain required; metadata cannot
replace them. The dynamic-default C/raylib roadmap stays intact for later phases.
Implementation remains HOLD.

## Approved general C interoperability direction, 8 November 2026

The [project/native architecture roadmap](PROJECT_ARCHITECTURE_ROADMAP.md#proposed-general-purpose-c-interoperability)
plans reusable typed C functions, structs, enums/constants and calling conventions,
with dynamic linking by default and static archives optional. Raylib is the first
substantial target, not a compiler subsystem. No public FFI grammar or general
dynamic loader is implemented by this record. The static providers below remain
valid implementation evidence, not a static-only future policy.

Reuse explicit descriptors, typed IR admission, NativeSafe/precise roots, Runtime
resources, provider quiescence and exact generation leases. Dynamic-library handles
must outlive resources, callbacks and retained code. Project source/manifest metadata
cannot authorize native loading or installation. The roadmap covers verified
artifact discovery, target ABI/version constraints, JIT/AOT and VOIDUP boundaries;
native per-platform/raylib acceptance remains future work.

## Implemented trusted-provider foundation

Stage 8 Sprint 4 generalizes admission to a second independently admitted static
in-memory sink. See [the current contract](STAGE8_APPLICATION_NATIVE.md) for
approval, implementation/verification status, exact supported operations and
limitations. `native_sink_project()` and `native_fixture_project()` each select
one compiled catalog; unknown libraries, symbols and signature/flag substitutions
reject. Provider identity is checked in resource IR and the shared Runtime registry.
CLI/LSP imports grant no authorization. No dynamic loader, raw-pointer ABI, source
FFI DSL or new worker privileges are added.

The sink uses a separate disposable C implementation with shared lifecycle,
rooting, owned strings, callback admission, shutdown draining and generation
leases. [Example](../examples/native_sink/main.void) requires explicit C++ host
admission. One provider descriptor per source graph remains the bounded API.

## Original fixture delivery history

### Stage 6 Delivery 4: trusted static native fixture

Delivery 4 is hosted-accepted at `619331ccb83227419081bc93cf47bd3a0b8b64cc`,
[CI #76](https://github.com/Shmellyorc/Void.Language/actions/runs/39359190869),
owner-confirmed 7/7 jobs and 917/917 suites. Delivery 5 integrated acceptance is
locally complete: 139/139 each in Debug, Release and strict ASan/UBSan/leak
configurations (417/417), with 1 MiB and editor/package checks passed. The bounded
D6 contract below is unchanged. Only the owner can commit, push, run hosted CI and lock Stage 6
after all seven jobs pass. Stage 7 remains unauthorized.

## Delivery 5 integration boundary

The real `integrated.void`, `integrated_cancel.void` and `integrated_failure.void`
programs compose this binding with owning-executor async, groups and value-only
workers. The native resource stays local to its acquisition thread through await
and cleanup. Use existing explicit `Jit::compile_async` / `emit_async_object` host
APIs for these async entries; ordinary CLI/LSP opt-in rules remain unchanged.
Tests cover normal/O2 JIT and separately linked AOT, GC in foreign callbacks and
workers, cancellation/failure cleanup and callback-before-admission shutdown while
a worker is held. No native resource or callable crosses the worker boundary.

Cached old roots, queued/running jobs and retained registrations keep their code
through compatible replacement. Tests invoke the old callback after its root and
NativeCode release, then unregister and verify retirement; private async frame
changes do not migrate old operations. A callback's old task token is rejected
inside a replacement executor, even though its own native entry remains leased.
Repeated reload checks count managed objects and live generations, not process RSS.

Fresh-process cache regressions use real C bridges and a stack-probed synchronous
worker frame. They prove relocation/rebinding and fresh native resource/callback
creation on cold/hit runs, corrupt-artifact fallback and safe Windows bypass.
Corrupt artifacts remain misses until explicitly removed; this integration does
not introduce automatic cache repair, new granularity or file watching. Neither
live Runtime state nor pointers/handles/callback addresses become cache payloads.

The ordinary compiler-owned tooling session and real LSP process still reject an
unprovided native descriptor, including after other explicitly authorized native
projects have run in the host. Overlay repair restores references without accepting
stale results. No Stage 7 tooling feature or general FFI is introduced.

## Explicit project trust and one compilation path

`src/native/fixture.c` is a real statically linked C library. A host explicitly
opts one project into its compiled catalog using the typed C++ descriptor:

```cpp
auto analysis = vd::analyze_project(entry, vd::native_fixture_project());
// Check analysis.succeeded(), then use the existing Jit or object emitter.
vd::emit_object(analysis.program->lower(), output, optimize, true); // source bootstrap
```

`void_native_example examples/native/main.void` is the runnable embedding example
and prints `19`. Ordinary `.void` wrappers import `/native_bridge.void`. The host
provides that virtual module from its verified descriptor; it cannot shadow a
physical project file. The ordinary CLI/LSP do not automatically authorize this
fixture. No public native grammar, manifest parser, package resolver, dynamic
loader or arbitrary-symbol mechanism is delivered.

The descriptor names the compiled library and exact callable names, symbols,
parameter/result types, affinity, blocking and callback behavior. Unknown symbols,
ABI mismatches and modified behavior fail before source publication. Header
functions are checked declarations, never executable placeholder bodies. Calls
become a verified `native_call` in existing typed IR and use existing LLVM
lowering for normal/O2 JIT and independently linked AOT. IR independently rejects
unsupported signatures, unverified bridge imports, forged resource construction,
payload access, incompatible callback provenance and worker/native effects.

The compiled bridge invokes real C prototypes for i8/i16/i32/i64, u8/u16/u32/u64,
f32/f64 and bool. Private LLVM transport uses u32/u64/pointers; the target host
compiler applies exact C scalar ABI rules. VOID managed aggregates, resource
payloads, varargs, arbitrary C++ objects and source pointers are not public ABIs.

## Strings and deterministic resource ownership

VOID keeps one UTF-8 string type. Terminated APIs receive a safely owned temporary
and reject embedded NUL. Length-aware calls, returned owned buffers and retained
copies preserve embedded NUL and UTF-8 bytes. Returned C allocations are released
even if conversion into a managed string fails. Retained bytes live in the
resource's owned backing string; the C pointer is cleared before that storage is
released or replaced. No pointer into caller storage survives a boundary.

The binding's `resource` is an opaque managed identity. Source cannot construct,
extend or access its payload. The C pointer exists only in the Runtime registry;
it is never a source-visible integer or cache payload. A normal managed VOID
wrapper uses `result<resource, string>` acquisition and existing `deinit`/`with`.
The wrapper is allocated before native acquisition, and registry publication is
transactional. Failed allocation/admission releases the C resource and roots.

Managed assignments share one checked identity. Explicit close rejects duplicate
close and use after close. Existing one-shot `deinit` safely finishes an already
closed wrapper. Resource-owning value `deinit` remains rejected by the existing
managed-receiver requirement; no copyable native pointer ownership is introduced.
Resource handles and callable-containing wrappers cannot cross task/worker
boundaries. Raw handles are binding-private identities, not transferable values.
GC does not invoke native cleanup. Explicit close or Runtime shutdown unregisters,
quiesces, releases retained backing, frees C storage and releases persistent roots.
Dropping an owner without deterministic cleanup retains its resource until shutdown.

## Callbacks, threads and lifetime

The fixture stores real callback/context registrations in C. Dispatch reserves a
provider call and snapshots that registration while locked, then invokes the
stable compiled trampoline without provider locks. Existing CallbackRegistration
admits same-thread or foreign-thread execution into the same Runtime. Callback
captures have precise Handle roots and a lease on their exact native generation.

For this delivery registration accepts directly named functions and freshly
formed literal/bound closures in the registering generation. Forwarded or stored
callable values diagnose: the existing callable ABI carries no generation token.
This avoids pairing an old discriminator/environment with a new dispatcher.
Registration is one-shot; unregister drains and releases its lease. A later
registration requires a new resource. Self-drain fails through checked diagnostics
rather than deadlocking. Callback failures are copied while their code lease is
held, then returned through the existing RuntimeFailure path; C++ exceptions never
unwind through generated source code. Reached cleanup cannot overwrite a primary
native failure's owned message or bounded source frames.

Pure scalar/string operations are thread independent. Resource operations require
the acquisition thread. Blocking operations and synchronous foreign dispatch/join
enter NativeSafe; the collector can rendezvous while the foreign callback is
Managed. Foreign callbacks cannot operate on their caller's thread-affine resource.
Shutdown may drain on another NativeSafe host thread. Provider unregister prevents
new reservations and waits for all reserved dispatches, including callbacks held
before admission; registration drain then waits for admitted callbacks. Roots,
retained bytes and code leases remain until both barriers finish. Concurrent
shutdown calls serialize their drain. Shutdown from inside an active callback is
rejected before changing Runtime admission state.

JIT generations own the existing shared ORC engine, not the compiler's Runtime
owner. This breaks a callback lease → generation → compiler → Runtime ownership
cycle without weakening retirement. Compatible reload keeps a registered callback
on its original code until unregister; replacement calls use replacement code.
Debug cache identity includes the actual compiled C fixture. Cache hits relocate
ordinary code and bind the verified bridge anew; no C pointers, active callbacks,
roots or native resource state are serialized.

The Windows G11 qualification uses the managed catalog with the real
raylib Win32 software provider. The separate G5 fixture checks retained
callbacks through both Windows admission paths. Its reload harness observes
the provider and adapter DLL mappings while a registration retains its
generation and after their Runtime and generation owners retire.
[G11 Windows evidence and replay](projects/G11_WINDOWS_MANAGED_NATIVE_QUALIFICATION.md)

## Bounds and acceptance

Permanent source tests cover all scalar widths, bool/floats, UTF-8/NUL transfers,
returned/retained ownership, failed acquisition, cleanup, duplicate/use-after-close,
same/foreign callbacks, pre-admission shutdown, GC, allocation failures, callback
self-drain/affinity, descriptor/IR rejection, cache hits and old-code retirement.
The main and four checked-failure programs have normal/O2 independent AOT coverage.
Acceptance also retains all existing suites, strict sanitizers/leak checks, editor
packaging and 1 MiB stack checks. See WORK_LOG.md for actual results.

General native libraries, public binding syntax, asynchronous native completion,
worker/native calls, native-resource task transfer, UTF-16 policy, arbitrary
callable-generation transfer and dynamic library ownership remain deferred.
