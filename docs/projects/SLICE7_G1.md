# Slice 7 Gate 1 verified C ABI primitive calls

Baseline `2f667ea338127b8f91c3136804377ef3fa5218d6`. Golden Wrench
authorized a host-admitted linked fixture, independent typed IR, normal
and O2 JIT and independently linked AOT. Verified dynamic startup loading
remains the later acceptance mode. Hosted CI remains on hold.
[Architecture record](SLICE7_ARCHITECTURE_REVIEW.md).

## Canonical descriptor and authority

The host API is `CPrimitiveAdmission::admit(descriptor, linked_thunk)`. It
returns an immutable, owning capability. `analyze_project(entry, admission)`
uses the existing explicit-entry frontend and the existing source graph. The
descriptor supplies the generated declarations and a canonical one-file VFS
identity. A collision with a physical source is an error on capture or
recapture. The generated source has an owned diagnostic identity. Ordinary
source imports and schema 1 grant no native authority.

The descriptor is ASCII, LF-terminated, and space-separated in this exact
order. The parser rejects unknown fields, alternate formatting, unsupported
revisions and duplicate source bindings or C exports. Limits are 16,384
bytes total, 1,024 bytes per record, 64 bindings and 16 arguments per
binding. Bind record order defines the zero-based dispatch ordinal.

```text
void-c-primitive 1 1
provider <lowercase logical package>
target <canonical target triple> <C ABI profile>
source </canonical_file.void>
thunk <lowercase source SHA-256>
fixture <lowercase source SHA-256>
effects direct-sync nonblocking no-callbacks
call c-default
aliases c_int:i32 c_long:<target signed width> c_size:u64 c_bool:bool
bind <VOID name> <exact C export> <C result> [<C parameter> ...]
```

The two revisions identify the descriptor and transport. The primitive
prototype accepts `i8/i16/i32/i64`, `u8/u16/u32/u64`, `f32/f64`,
`c_int`, `c_long`, `c_size` and `c_bool`. `void` is result-only.
The argument list specifies exact ordered layout. The independently
compiled native C prototype selects physical registers, stack locations,
extension and return ABI. Linux x64 GNU uses SysV LP64 and Windows x64 MSVC
uses LLP64. Both profiles have 32-bit signed `int`, 64-bit unsigned
`size_t` and an 8-bit C Boolean representation. `long` is signed
64-bit on Linux and signed 32-bit on Windows. The thunk checks the target
profile and C layout facts at compile time with pinned Clang 23.1.4.

The fixture build pins SHA-256 source identities and seals the entire
target-specific descriptor in the compiled thunk. The independently
compiled fixture presents its own identity. Admission probes that the
supplied trusted thunk agrees with every descriptor byte and the linked
fixture identity. These hashes identify the C inputs to the pinned build.
Exact emitted object hashes and compilation commands are recorded separately
for each configuration. This is a linked, trusted host contract. The
descriptor neither authenticates arbitrary executable bytes nor allows
loading a native object by path.
[Parser and admission API](../../include/void/c_primitive.hpp) ·
[Independent parser](../../src/ir/c_primitive.cpp) ·
[Fixture source](../../tests/ffi/primitives_fixture.c) ·
[Thunk source](../../tests/ffi/primitives_thunk.c) ·
[Target build](../../tests/ffi/primitives.cmake).

## Checked dispatch and independent IR

```c
uint32_t vd_c_primitive_dispatch_v1(
    const char *descriptor, uint32_t size, uint32_t ordinal,
    const uint64_t *arguments, uint32_t count, uint64_t *result);
```

Argument and result words are low-bit encodings with zero upper bits.
Signed bit patterns are reconstructed at their exact widths, floating
arguments and results use their binary32/binary64 bits, and Boolean words
are zero or one. A zero-argument binding receives a null argument pointer.
A void result receives a null result pointer. The probe uses
`UINT32_MAX`, zero count and both pointers null. Status zero is success,
one rejects the descriptor or fixture identity, and two rejects the
transport call shape. The dispatcher checks identity before calling any
fixture function. No native pointer is a VOID value.

The compiler emits a small checked IR wrapper per declaration using the
existing binder and ordinary direct-call lowering. The independent
validator requires the sealed admission and the exact binding-module
identity, source name, provider-qualified export, ordinal, argument slots,
operand and result types, block and terminator. Ordinary IR callers
require a direct import of the admitted module. Callable references to
the raw binding and legacy raw imports in an admitted graph are rejected.
G1 rejects async and worker activations for an admitted graph and rejects
native calls during eager global initialization. The prior native fixture
and sink rules retain their own checks.

The normal and O2 JIT paths use the same LLVM lowerer, and the ORC symbol
lives on the specific code generation tracker. The raw symbol cannot be
installed through the ordinary JIT host import API. Persistent object cache
use is unavailable for G1 host-admitted graphs. Compatibility includes the
exact descriptor. The AOT fixture links an emitted VOID object, independently
built C fixture and thunk, and the existing Runtime. Its adapter verifies
the linked descriptor and fixture before starting the Runtime. The ordinary
host executable builder cannot implicitly link a G1 fixture. Neither
the fixture nor its thunk is linked into the production compiler or
Runtime.
[Frontend](../../src/frontend/compiler.cpp) ·
[IR validation](../../src/ir/module.cpp) ·
[LLVM lowering and admission](../../src/backend/llvm/backend.cpp) ·
[Process and direct IR tests](../../tests/ffi/primitives.cpp).

## Qualification

| Configuration | Result | CTest and ABI evidence |
| --- | --- | --- |
| Linux x64 Release | 42/42 | [CTest](evidence/slice7-g1/linux/release-ctest.txt) · [ABI and process receipt](evidence/slice7-g1/linux/release-artifacts.json) |
| Linux x64 Debug | 42/42 | [CTest](evidence/slice7-g1/linux/debug-ctest.txt) · [ABI and process receipt](evidence/slice7-g1/linux/debug-artifacts.json) |
| Linux x64 fatal ASan/UBSan | 42/42 | [CTest](evidence/slice7-g1/linux/strict-ctest.txt) · [ABI and process receipt](evidence/slice7-g1/linux/strict-artifacts.json) · [Corrective command and hashes](evidence/slice7-g1/linux/strict-corrective-command.txt) |
| Windows 10 x64 build 19045 Release | 41/41 | [CTest](evidence/slice7-g1/windows/Release-ctest.txt) · [Direct JIT and IR](evidence/slice7-g1/windows/Release-g1-direct.txt) · [Compile and binary receipt](evidence/slice7-g1/windows/Release-result.json) |
| Windows 10 x64 build 19045 Debug | 41/41 | [CTest](evidence/slice7-g1/windows/Debug-ctest.txt) · [Direct JIT and IR](evidence/slice7-g1/windows/Debug-g1-direct.txt) · [Compile and binary receipt](evidence/slice7-g1/windows/Debug-result.json) |

Each configuration executed the fixture under normal and O2 JIT and
normal and O2 independently linked AOT. All four modes returned 73 for
input 7. The C oracle and VOID agreed on 64-bit `long` on Linux and
32-bit `long` on Windows. The wrong fixture exited 1 before VOID startup.
The missing transport failed ELF and COFF linking with the same unresolved
symbol. Windows ran the same selection except for the Linux-only
manifest authority test.
[Linux linker transcript](evidence/slice7-g1/linux/strict-LastTest.txt) ·
[Windows linker transcript](evidence/slice7-g1/windows/Debug-LastTest.txt).

All five configurations used Clang/LLVM 23.1.4. Linux strict instrumented
the compiler, Runtime and the independent C units. ASan and UBSan were
fatal with leak detection enabled. The pinned LLVM SDK lacks RTTI, so
vptr checks are unavailable. VOID's emitted machine code uses the
verified normal/O2 lowering. The Windows run verified 3,269 exact packet
inputs, including all 17 G1 source/test paths, and restored the
provider-attested guest source root. Its preflight measured 33.81 GiB
free on the existing Windows 10 C: drive.
[Linux replay](evidence/slice7-g1/replay-linux.sh) ·
[Windows qualification receipt](evidence/slice7-g1/windows/native-results.json) ·
[Windows preflight storage](evidence/slice7-g1/windows/storage.json).

The source fixture verifies both boundaries for all eight integer types,
C aliases, Boolean true/false, signed zero, exact floating results and
a 14-argument mixed register/stack call. A one-value change in the mixed
call changes the result. The suite also calls the independent fixture
directly as an ABI comparison, exercises no-argument and void-result
calls, and reruns the legacy native/provider, project, numeric, Runtime,
GC and reload tests. Independent hand-built IR is validated and executed
before the malformed cases mutate it. AOT links with the wrong fixture
identity and without the transport to exercise the startup and link
failure paths.
[VOID fixture](../../tests/ffi/primitives.void.in) ·
[Missing fixture control](../../tests/ffi/missing.py).

### Preserved original failures

The first Release build command addressed new Make targets before
regenerating the build files. The configure-first replay corrects that
invocation. A second Release build found a wrong diagnostic field in
the test harness. The corrected harness throws the existing
`CompilationError` with its owned records. The first Debug matrix
rejected an unadmitted body at the unknown-callee check before the intended
native guard. The corrected fixture renames the caller's target as well
so the unadmitted body reaches independent native validation. The first
strict matrix then reused a frontend object built across the async-source
diagnostic edit. It passed 41 of 42 tests and failed the new async-source
negative control. The corrective run displaced that one stale object,
rebuilt the frontend and reran the unchanged selection and fatal settings.
[Original transcripts](evidence/slice7-g1/original).

## Source and Git inventory

[Exact G1 patch](evidence/slice7-g1/source.patch) ·
[Before and after hashes](evidence/slice7-g1/source-hashes.json) ·
[Layered Git inventory](evidence/slice7-g1/inventory.txt).

The patch contains 17 source and test paths relative to the accepted
pre-G1 working tree. Seven existing paths carry the scoped G1 amendments
and 1,108 other prior paths retain their exact bytes. The index still
matches all 68,996 HEAD entries, with an empty staged diff. The patch
passed a read-only reverse application check.

## Remaining scope

G1 covers only synchronous host-admitted primitive bindings in the two
listed target profiles. Public declaration grammar, foreign records and
pointers, ownership transfer, callbacks, dynamic library resolution,
dependency admission and raylib remain separate gates. The matrix does
not qualify static-linking fallback policy, installed third-party linkage,
direct executable native imports, Windows Server 2022, macOS, other
architectures or Windows sanitizers.

## End-of-task temporary workspace cleanup

The host removed 83 verified entries from its Gate 1 scratch
workspace and the per-profile probe directories. The Windows guest
removed the restored source overlay backup, the two empty temporary
roots and the two generated probe directories. Free space on the
existing Windows 10 C: drive rose from 33,162,838,016
bytes to **33,164,546,048 bytes (30.89 GiB)**.
The receipts list all removed paths and verify the retained artifacts.

The configured Linux and Windows build trees, qualified binaries,
source archive, extracted Windows harness and toolchains remain
available. The host share retains the Windows qualification receipts
and replay inputs. The cleanup preserved unrelated files, shared
dependencies, reusable caches and Git state.
[Host cleanup](evidence/slice7-g1/host-cleanup.json) ·
[Windows cleanup](evidence/slice7-g1/windows/cleanup-results.json).

Stop at Golden Wrench's independent G1 review boundary.
