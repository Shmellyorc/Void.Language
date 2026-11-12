# VOID.Language feature completeness and developer experience

**Milestone 1 · Source audit and roadmap · Golden Wrench review**

Baseline `4d39a2b121b8ba6fb6cfa3b60d02d8e048b27b59` on `main`. The audit used tracked source, maintained test registrations and recorded results at their stated revisions. LLVM/Clang/LLD **23.1.4** remains the active toolchain. This roadmap proposes work for Boss's subsequent authorization.

The [milestone register](MILESTONE_REGISTER.md) tracks integrated batches, current assignments, qualification scope and remaining gates.

**12 November 2026 reconciliation.** G00-A assesses the integrated source at
`afe14d4c0cd7fd0576ecc194a38c294d7e54ec0a`. The inventory tables below retain
their original audit baseline. Later G01–G03 records establish diagnostic
recovery, trusted project Check/Build/Run and AOT source debugging in their
qualified scopes. G12 is integrated and independently source-reviewed.
The canonical readiness report gives the current developer journey, native
matrix and open public-distribution obligations. G00-A remains **AUDIT IN
PROGRESS**. [Readiness audit](../release/G00_READINESS_AUDIT.md)

VOID already supports ordinary native applications, including manifest creation, dependency locking, normal and O2 JIT, linked executables, and relocatable application directories on the supported Linux and Windows targets. The highest general development priorities are a reliable project-to-run editor workflow, usable source debugging, and a bounded way to run and capture a child process. A public binary distribution has separate trust, redistribution and native installation obligations.

**Status key**

- **IMPLEMENTED** means source and supporting evidence establish the stated capability.
- **PARTIAL** means the working capability has a material limitation.
- **MISSING** means the capability is absent at the baseline.
- **DEFERRED** means a decision or existing contract deliberately leaves the capability outside the current scope.
- **UNVERIFIED** means the evidence does not establish the stated behavior on that host or configuration.

## 1. Compiler and Runtime

| Capability | Status and actual boundary | Implementation and maintained evidence |
| --- | --- | --- |
| Values, types and declarations | **IMPLEMENTED.** Exact-width signed and unsigned integers, f32/f64, bool, immutable UTF-8, contextual numeric literals, typed and inferred locals, pure module constants, runtime readonly locals and initialized module globals. Checked numeric casts and numeric enums exist. Default values and deep readonly views preserve the distinction between value copies and shared reference identities. **DEFERRED** for unrestricted nullable value types and Unicode identifiers. | [Parser and semantic checker](../../src/frontend/compiler.cpp), [numeric operations](../../src/numeric.cpp), [frontend tests](../../tests/frontend), [numeric enum process/IR/LSP](../../tests/numeric), [runtime globals](../RUNTIME_GLOBALS.md) |
| Functions, calls and initialization | **IMPLEMENTED.** Ordinary and receiver calls use an ordered positional prefix followed by named arguments. Pure defaults and concrete default-based parameter inference work within the current declaration rules. Explicit value/managed `init`, named-field construction, function values and closures work. Named and unnamed multiple returns support local destructuring under G15. **DEFERRED** fallible or overloaded constructors. | [Parameter parser](../../src/frontend/compiler.cpp#L883), [constructor binding](../../src/frontend/compiler.cpp#L2377), [initialization regression](../../tests/frontend/initialization.cpp), [function defaults and inference](../STAGE8_CALLS_NUMERIC.md), [advanced language](../ADVANCED_LANGUAGE.md#multiple-return-values) |
| Structures, object model and generics | **IMPLEMENTED.** Value and managed structs, field visibility and const fields, properties, callable structs, events, single managed inheritance, explicit virtual/override/super, one-hop composition, structural managed-backed interfaces, and generic specialization with constraints and inference. Generic body checking occurs at specialization. **DEFERRED** general user static members and extension methods, broader value erasure, interface inheritance/defaults and variance. | [Frontend declarations](../../src/frontend/compiler.cpp), [generic inference](../../src/frontend/generic_inference.inc), [receiver and property tests](../../tests/frontend/methods.cpp), [inheritance](../../tests/inheritance), [interfaces](../../tests/interfaces), [generics contract](../GENERICS.md) |
| Collections and operators | **IMPLEMENTED.** Fixed and growable arrays, ordered typed dictionaries, explicit copy slices, checked and result-based access, structural mutation checks, scalar operations and scoped nominal operators. Overloading includes approved arithmetic, comparison and unary forms. **PARTIAL** for whole-collection equality and dictionary key families. Custom equality does not grant hashing authority. **MISSING** general bitwise/shift source operators. G12 covers optional language expansion. | [Collection and operator checks](../../src/frontend/compiler.cpp), [dictionary Runtime](../../src/runtime/dictionary.cpp), [array operations](../ARRAY_OPERATIONS.md), [dictionary contract](../DICTIONARIES.md), [operator and collection tests](../../tests/stage10) |
| Errors, patterns and control flow | **IMPLEMENTED.** `result`, `Ok`, `Err`, propagation, checked panic/cleanup, sum and numeric enums, scoped `is` captures, statement and expression `match`, statement and typed expression `if`, compile-time `has`, and iterable/range/conditional/unbounded `for`. `reverse`, break, continue, return, pass and compound assignments work. **DEFERRED** general custom iteration and unrestricted compile-time evaluation. The for-based design deliberately has no separate `while`. | [Pattern parser](../../src/frontend/pattern_parser.inc), [pattern semantics](../../src/frontend/pattern_semantics.inc), [pattern tests](../../tests/patterns), [Stage 8 ergonomics](../STAGE8_ERGONOMICS.md) |
| Async and concurrency | **IMPLEMENTED.** Async functions and admitted managed receivers, await, cancellation, lexical task groups, bounded worker `spawn` and checked native completions. Workers have a restricted transfer/effect contract. **PARTIAL** for general-purpose scheduling convenience and native async coverage. Task types remain inferred, and async value receivers, interface/virtual methods and async function values retain their restrictions. See G10. | [Frontend task checks](../../src/frontend/compiler.cpp), [Runtime scheduler](../../src/runtime/async.cpp), [async tests](../../tests/async), [async contract](../ASYNC.md), [await propagation handoff](../async-await-syntax/HANDOFF.md) |
| Lifetime and ownership | **IMPLEMENTED.** Precise nonmoving GC, checked initialization, rooted managed activations, deterministic `with`/`defer`, checked native resources and callback leases. GC and explicit resource disposal are separate. **DEFERRED** weak references and general resource-owning value copies. **UNVERIFIED** constant memory across arbitrary long-running workloads. Current retirement improvements have bounded evidence. | [Heap](../../src/runtime/heap.cpp), [resource ledger](../../src/runtime/native_resource.cpp), [construction checks](../../src/frontend/construction.inc), [GC contract](../GC_RUNTIME.md), [async retention scope](../ASYNC.md) |
| Modules, imports and visibility | **IMPLEMENTED.** Local VFS and standard imports, qualified aliases, `lib/` and locked `pkg/` modules, file-private and project-internal rules, deterministic manifest selection and nested boundaries. Package dependencies use declared identities and edges. **DEFERRED** for general registry discovery and unapproved package-management operations. | [Compiler project resolution](../../src/frontend/project_resolution.cpp), [module tests](../../tests/modules), [locked package fixture](../../tests/project/packages.py), [D2 closeout](../projects/DEPENDENCY_D2_CLOSEOUT.md) |
| Diagnostics and compilation cost | **IMPLEMENTED.** Owned structured spans, related locations, hints, UTF-8 validation, recovery, CLI excerpts and UTF-16 LSP diagnostics. **PARTIAL.** Editor queries use snapshot/revision checks, while project analysis still recaptures and reanalyzes the graph. Object-cache hits still require source analysis. Per-stage compiler timing and incremental reuse need measurements before design. Windows persistence remains deliberately unavailable. See G01, G05 and G08. | [Diagnostics](../DIAGNOSTICS.md), [ToolingSession](../../src/frontend/tooling.cpp), [LSP](../../src/lsp/main.cpp), [native cache](../../src/backend/llvm/native_cache.cpp), [performance qualification](../compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md) |
| JIT, AOT and debug information | **IMPLEMENTED.** A single frontend, typed IR and LLVM backend serve normal/O2 JIT, object emission and host-native linking. Runtime failures have source diagnostics. **PARTIAL.** The backend emits some location/type metadata. Manifest `debug_info` defaults to `none` in both profiles, and explicit `line` or `full` reports VOID5012. No profile-controlled symbol contract or qualified source debugger exists. See G03. | [CLI](../../src/main.cpp), [LLVM backend and DIBuilder](../../src/backend/llvm/backend.cpp#L585), [native linking](../../src/native_build.cpp), [profile test](../../tests/project/profiles.py), [CLI profile contract](../CLI.md#manifest-project-profiles-slice-3) |
| C ABI, callbacks and reload | **IMPLEMENTED** within the checked G1–G5 lanes. Native binding generation, fixed C scalar and approved record calls, copied spans, owned resources and bounded callbacks have real fixtures. Conventional trusted-provider loading and verified managed-catalog admission have distinct guarantees. Generation leases and compatible replacement have tests. **DEFERRED** raw pointers, variadics, arbitrary callback payloads, and a general automatic source reload product. See G11 and G12. | [Binding admission](../../src/runtime/native_binding.cpp), [callback Runtime](../../src/runtime/callback.cpp), [FFI fixtures](../../tests/ffi), [conventional FFI](../projects/SLICE7_G4_CONVENTIONAL_FFI.md), [G5 callbacks](../projects/SLICE7_G5_CALLBACKS.md) |

## 2. Standard library inventory

All **25** tracked `.void` modules are covered below. Pure source APIs use the existing compiler and collection/string operations. The native bridge and tests are cited where a module crosses into Runtime or host code. The installed [standard-library guide](../../stdlib/README.md) is useful for examples. Its older qualification wording, module omissions and operation counts need reconciliation with source and later evidence.

| Module | Status and everyday use | Boundary and evidence |
| --- | --- | --- |
| [archive](../../stdlib/archive.void) | **PARTIAL.** Bounded ZIP/ZIP64 stored/deflate reader with entry, expansion, CRC and close checks. G12 adds a bounded deterministic stored ZIP writer for explicitly supplied files. | Caller controls extraction and publication. General unpack, writer compression and ZIP64 output are deferred. [ZIP export contract](../archive/ZIP_EXPORT.md) · [Reader fixtures](../../tests/host_capabilities) |
| [channel](../../stdlib/channel.void) | **IMPLEMENTED.** Owner-thread typed topic routing, ordered synchronous delivery and exact subscription tokens. | **DEFERRED** for a cross-thread queue or async backpressure channel. [Channel contract](../channel/API.md), [routing tests](../../tests/channel), compiler event/callable semantics. |
| [collections](../../stdlib/collections.void) | **IMPLEMENTED.** Stable copied sorting, search, predicates, transforms, fold, sum, extrema and ordered dictionary extraction/copy. | Fixed callers explicitly slice. Map sometimes needs concrete type arguments or a typed transform. Callback authority and mutation checks apply. [API](../collections/API.md), [native collection storage](../../src/runtime/list.cpp), [tests](../../tests/collections_stdlib). G09. |
| [console](../../stdlib/console.void) | **IMPLEMENTED.** Checked text/byte streams, EOF/timeout distinctions, prompt conveniences, terminal mode, keys, cursor, color and size where supported. | Terminal capabilities vary by host and redirection. Convenience reads have a different failure contract from checked reads. [Native console](../../src/runtime/console.cpp), [combined contract](../combined/CONTRACT.md), [console source fixtures](../../tests/stage10/console.py), [Runtime tests](../../tests/runtime). |
| [convert](../../stdlib/convert.void) | **IMPLEMENTED.** Checked numeric, Boolean and eligible enum parsing, formatting and result unwrap helpers. | `unwrap_or` is eager. This is a closed conversion family. [Compiler conversion admission](../../src/frontend/stdlib_conversion.inc), [contract](../combined/CONTRACT.md), [combined tests](../../tests/combined). |
| [crypto](../../stdlib/crypto.void) | **PARTIAL.** Incremental SHA-256 and detached Ed25519 verification. | No signing/key-management or public secure random byte API in this module. [Native crypto](../../src/runtime/host_crypto.cpp), [host controls](../../tests/host_capabilities). G12. |
| [encoding/ascii](../../stdlib/encoding/ascii.void) | **IMPLEMENTED.** Strict 7-bit encode/decode, byte classes and casing. | ASCII is a bounded domain. [Encoding API](../text-encoding/API.md), [tests](../../tests/text_encoding). |
| [encoding/base64](../../stdlib/encoding/base64.void) | **IMPLEMENTED.** Standard and URL-safe alphabets, explicit padding policy, bounded encode and strict decode. | Whole-value APIs. [Encoding contract](../text-encoding/CONTRACT.md), [tests](../../tests/text_encoding). |
| [encoding/binary](../../stdlib/encoding/binary.void) | **IMPLEMENTED.** Exact-width ten-primitive endian codecs with checked widths and IEEE bit preservation. | No arbitrary object layout serialization. [Compiler codec](../../src/frontend/binary_codec.inc), [wire tests](../../tests/binary), [contract](../binary/CONTRACT.md). |
| [encoding/hex](../../stdlib/encoding/hex.void) | **IMPLEMENTED.** Strict ASCII pairs and bounded encode/decode. | Whole-value APIs. [Encoding API](../text-encoding/API.md), [tests](../../tests/text_encoding). |
| [error](../../stdlib/error.void) | **IMPLEMENTED.** Shared managed base carrying message, path and code. | Recoverable error types stay exact in results, with subsystem detail on derived errors. [Combined contract](../combined/CONTRACT.md), [error and result controls](../../tests/combined). |
| [io](../../stdlib/io.void) | **IMPLEMENTED.** Bounded whole-file text/bytes, file handles, cursor and numeric/Pascal wire methods, directory operations, links, sync/async ordinary file operations, and synchronous atomic saves through a private-directory capability. | General writes/copies may leave partial progress. Atomic saves report publication, synchronization and cleanup separately. [I/O contract](../io/CONTRACT.md#atomic-application-saves) · [G06 implementation and qualification](#g06-atomic-application-saves). |
| [io/memory](../../stdlib/io/memory.void) | **IMPLEMENTED.** Owned stream, independent snapshots, shared cursor, bounded seek and numeric/raw/Pascal reads and writes. | A memory-specific source API. [Memory contract](../memory/CONTRACT.md), [tests](../../tests/memory). |
| [json](../../stdlib/json.void) | **IMPLEMENTED.** Strict bounded tree parser, immutable observations, exact numeric tokens, typed DTO encoding/decoding and stringify. | **PARTIAL** for incremental input and application-specific schemas. Typed conversion is restricted to supported public stored data. Custom initialization/private state and missing/defaulted scalar schema policy require explicit DTO mapping. [Compiler schema adapter](../../src/frontend/json.inc), [API](../json/API.md), [tests](../../tests/json). G09. |
| [log](../../stdlib/log.void) | **IMPLEMENTED.** Level filters, lazy messages, checked managed loggers, single-line framing, console/file sinks, rotation, recovery and awaitable operations. | The default creates no files. File namespaces have an exclusive ownership contract. Historical performance aims and unresolved measurements retain their scope. [API](../log/API.md), [log tests](../../tests/log), [integrated performance evidence](../compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md). |
| [math](../../stdlib/math.void) | **IMPLEMENTED.** Typed scalar functions, rounding, interpolation, trigonometry, powers/logarithms and predicates. | Bounded supported scalar types and documented rounding/domain behavior. [Compiler Math admission](../../src/frontend/math.inc), [API](../math/API.md), [tests](../../tests/math). |
| [net/http](../../stdlib/net/http.void) | **IMPLEMENTED.** HTTP/HTTPS 1.1/2, explicit clients, buffered/streaming sync and async operations, file transfers, multipart, cookies, auth/proxy, TLS and deadlines. | Hosted acceptance covers the stated provider contracts. Windows Schannel has the explicit store identity boundary. Supplied P12 is refused there. [Runtime HTTP](../../src/runtime/http.cpp), [acceptance](../net/http/ACCEPTANCE.md), [tests](../../tests/net/http). G07 covers separate socket APIs. |
| [os](../../stdlib/os.void) | **IMPLEMENTED.** App-aware paths, home/temp, executable/resources, environment lookup, host identity, bundle classification and literal application arguments. | **PARTIAL** environment convenience. Read-by-name exists. Whole-environment enumeration, mutation and implicit child inheritance have no public API. [OS Runtime](../../src/runtime/os.cpp), [Foundation adapter](../../src/runtime/os_apple.mm), [API](../os/API.md), [tests](../../tests/os). G04. |
| [path](../../stdlib/path.void) | **IMPLEMENTED.** Host-aware lexical join, normalize, decomposition and absolute/relative conversion. | Lexical results grant no filesystem authority or existence guarantee. [Filesystem operations](../../src/runtime/filesystem.cpp), [I/O API](../io/API.md), [I/O fixtures](../../tests/io). |
| [process](../../stdlib/process.void) | **IMPLEMENTED.** Explicit native launch with literal arguments, complete environment and cwd. Inherited-stream launch retains synchronous wait and close. Linux x64 GNU and Windows x64 MSVC add bounded binary stdout/stderr capture, deadlines and async task cancellation. | Stdin is inherited. The API owns and reaps one direct child. Separate async wait of an existing child, stdin piping and descendant supervision remain outside G04. [Process API](../process/API.md) · [G04 implementation and qualification](#g04-bounded-subprocess-capture-deadlines-and-cancellation) |
| [random](../../stdlib/random.void) | **IMPLEMENTED.** Reproducible seeded gameplay streams, per-Runtime default with OS seeding, bounded draws, checked variants, choice and shuffle. | Gameplay output carries no cryptographic guarantee. [Runtime](../../src/runtime/random.cpp), [API](../random/API.md), [tests](../../tests/random). |
| [system_error](../../stdlib/system_error.void) | **IMPLEMENTED.** Host capability errors carry portable kind, operation and native/cleanup/progress detail. | Existing process, archive and crypto error translation is reusable for additions in those APIs. [Host ABI](../../src/runtime/host_abi.cpp), [host error provider](../../src/runtime/host_error.cpp), [source tests](../../tests/host_capabilities). |
| [text](../../stdlib/text.void) | **IMPLEMENTED.** Unicode-scalar search, trim, substring, split/join, replacement, padding and pinned Unicode 16.0 full default casing. | **DEFERRED** normalization, case folding, grapheme segmentation and regex. Scalar indexes and pinned case mapping are established contracts. [Native case mapping](../../src/runtime/text_case.cpp), [API](../text-encoding/API.md), [tests](../../tests/text_encoding). G09. |
| [time](../../stdlib/time.void) | **IMPLEMENTED.** Gregorian construction, instants/durations, offsets and IANA zones, ambiguity handling, checked arithmetic, formatting, UTC/monotonic clocks and bounded async sleep. | General inverse formatted-date parsing and deadline composition are deferred. [Temporal core](../../src/temporal.cpp), [temporal tests](../../tests/stage10), [timer contract](../ASYNC.md#g10-monotonic-timers), [installed API guide](../../stdlib/README.md). G09/G10. |
| [url](../../stdlib/url.void) | **IMPLEMENTED.** Checked HTTP(S) addresses, relative resolution, normalization, percent byte/text operations and ordered query pairs. | HTTP(S) scope. URL parsing does not perform DNS or imply raw sockets. [Runtime URL](../../src/runtime/url.cpp), [HTTP/URL examples](../net/http/README.md), [network fixtures](../../tests/net). |

There is no public TCP listener, TCP stream, UDP endpoint or standalone resolver in the module tree. The [TCP/UDP handoff](../net/TCP_UDP_HANDOFF.md) supplies a proposed design and reuse points. Public task syntax and owner-side native completion already exist in the compiler and Runtime. A synchronous topic router or a monotonic clock does not supply an async queue or timer.

## 3. VOIDUP and the application workflow

| Developer step | Status and owner | Evidence or boundary |
| --- | --- | --- |
| 1. Install VOID | **IMPLEMENTED** source-build/install helper with compiler/tooling CMake components, pinned prerequisites and an installed smoke. **PARTIAL** private binary toolchain management through VOIDUP. | Source installation retains host development prerequisites and external SDK references. The helper's native Windows/macOS replay has a separate evidence boundary from accepted hosted component installation. [Install guide](../install/README.md), [helper](../../tools/install.py), [verifier](../../tools/verify_install.py), [qualification](../developer-experience/REPORT.md). |
| 2. Create an application | **IMPLEMENTED** through `voidc new`/`init` and compiler-backed VS Code creation. | IDs/schema and exclusive publication are already covered. An eligible main can have an arbitrary filename. [Creation source](../../src/frontend/project_creation.cpp), [process tests](../../tests/project/creation.py), [real editor host](../../editors/vscode/src/test/creationHost.ts). |
| 3. Add sources and dependencies | **IMPLEMENTED** manifest roots, nested boundaries, imports and library creation. Dependencies are declared as local paths or approved Git origin/revision pins. | No dependency install authority from opening a project. [CLI commands](../../src/main.cpp), [manifest/entry controls](../../tests/project/discovery.py), [dependency tests](../../tests/project/dependencies.py). |
| 4. Resolve and manage packages | **IMPLEMENTED** `deps fetch` for eligible local-file and public smart HTTPS Git, plus `resolve`, `--locked`, `--refresh`, deterministic locks and verified offline imports. **PARTIAL** for change review and discovery. | Exact full revisions, bounded snapshots and explicit transitive authority. No authenticated private Git, registry/catalog search, automatic latest-version selection or dependency editing command. [Git acquisition](../../src/frontend/project_git.cpp), [D3 HTTPS rules](../projects/DEPENDENCY_D3_HTTPS_CLOSEOUT.md), [offline/package tests](../../tests/project/packages.py). G05. |
| 5. Check, build and run | **IMPLEMENTED** through `voidc check/run/build/emit`. Application and legacy entry semantics coexist. | Libraries support check in manifest mode. The CLI already owns entry selection and native artifact publication. No first-party editor task invokes the same selected project yet. [CLI](../../src/main.cpp), [Stage 9 fixture](../../tests/stage9/workflow.py), [project process test](../../tests/project/application.py). G02. |
| 6. Manage Debug and Release | **IMPLEMENTED** optimization/profile precedence and quiet implicit defaults. **PARTIAL** profile-controlled debugging and cross-platform persistent cache. | Debug O0 and Release O2 remain distinct. Explicit `line`/`full` warns. Windows currently falls back to uncached execution. [Profile fixture](../../tests/project/profiles.py), [CLI profiles](../CLI.md#manifest-project-profiles-slice-3), [native cache](../../src/backend/llvm/native_cache.cpp). G03/G08. |
| 7. Integrate native libraries | **IMPLEMENTED** manual native binding and verified D4 catalogs for the admitted Linux/Windows lanes, including approval, lock, provider and adapter checks. | The managed catalog selects one local provider with exact target/version/ABI. macOS managed-native execution is unsupported. G11 records real Windows raylib and the separate retained-callback fixture. [Catalog code](../../src/frontend/project_native.cpp), [D4](../projects/DEPENDENCY_D4_CLOSEOUT.md), [G11 qualification](../projects/G11_WINDOWS_MANAGED_NATIVE_QUALIFICATION.md). |
| 8. Package an application | **IMPLEMENTED** D5 `voidc package` and `--verify` on Linux x64 GNU and Windows x64 MSVC, including resources, metadata, notices and admitted native payloads. | Linux has verified same-app replacement. Windows requires a fresh destination even with `--replace`. macOS D5 is unsupported. Bundle/plain-process OS queries are a distinct accepted capability. [Linux D5](../projects/DEPENDENCY_D5_CLOSEOUT.md), [Windows W2](../projects/W2_WINDOWS_D5_IMPLEMENTATION.md), [packaging fixture](../../tests/project/application_packages.py). G11. |
| 9. Update toolchains and projects | **IMPLEMENTED** explicit VOIDUP install/select/rollback for its private trust lane. Project pins are maintained through manifest edits, explicit fetch and lock refresh. **MISSING** public production bootstrap, project-scoped toolchain selection and a general update/migration workflow. | Toolchain and source-dependency versions are independent. `voidc` owns project graph changes. [VOIDUP manager](../../apps/voidup/manager.void), [VOIDUP commands](../../apps/voidup/cli.void), [lock resolution](../../src/frontend/project_resolution.cpp). G00/G05. |
| 10. Maintain an existing application | **IMPLEMENTED** inspect/check/resolve/verify, project reselection, guarded edits and existing runtime diagnostics. **PARTIAL** for safe debugging, project-aware editor execution and explainable update paths. | The workflows in G01–G05 build on the existing checks. [Project protocol](../../tests/project/protocol.py), [VS Code project host](../../editors/vscode/src/test/projectHost.ts), [dependency closeout](../projects/DEPENDENCY_D2_CLOSEOUT.md). |

VOIDUP is a native VOID application and an ordinary `ALL` build target. Its private engineering CLI implements `list [--available]`, `install [--activate]`, `default`, `rollback`, `which`, `verify`, `doctor` and `run` for `voidc` or `voidls`. Its explicit HTTPS transport, private 2-of-3 engineering keys, serial/revocation checks, inventory verification, bounded ZIP processing, writer lock and controlled child environment already exist. The compiler/tooling component install does not include VOIDUP. Production trust deployment, channel/shim policy, self-update and pruning are outside the private implementation. The Phase B Linux exercise used two actual engineering compiler payloads. Windows native build, host primitive and smoke results have narrower scope than a signed two-release install/activate/rollback replay. Equivalent full Windows and macOS manager flows remain **UNVERIFIED** in the reviewed evidence. [Build target and smoke registration](../../CMakeLists.txt) · [Trust implementation](../../apps/voidup/trust.void) · [Phase B results](../voidup/PHASE_B_REPORT.md) · [Integration and native scopes](../voidup/INTEGRATION_REPORT.md#native-windows-results)

### Qualification boundaries

| Recorded scope | Disposition retained |
| --- | --- |
| Hosted source installation | **ACCEPTED** run `41916674041`, SHA `ffd8920befe04a67e3eaef6aadcdf0233df13563`, three hosted Release hosts with their declared prerequisites. [Preview register](../release/PREVIEW_BLOCKERS.md) |
| HTTP | **ACCEPTED** run `41815539463`, SHA `1afd881ecfa65c616261e7aeb76ecd2bf87770b6`, seven hosted jobs for that HTTP checkpoint. [Acceptance](../net/http/ACCEPTANCE.md) |
| Linux native projects and raylib | Local D1–D5, managed raylib, conventional normal/O2 JIT and independently linked/relocated AOT have recorded coverage. Preserve the visible Mesa graphics leak observation and its scoped exception. [Reconciliation](../projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md) · [Raylib integration](../projects/SLICE7_RAYLIB_INTEGRATION.md) |
| Windows native projects and packaging | W1 run `42986407961` at `01c465d444060fae1e2806a21673db5ccacfcc8f` and W2 run `43069968456` at `5fffda8f406e23507367b19152fb8fdce3483522` are **ACCEPTED** for their focused Windows Server 2022 x64 scopes. W2 passed 10/10 per profile. Local editor and conventional software-raylib results identify Windows 10 x64, build 19045. W2's managed provider was synthetic. [W1](../projects/WINDOWS_NATIVE_PROJECT_QUALIFICATION.md) · [W2](../projects/W2_WINDOWS_D5_IMPLEMENTATION.md#hosted-acceptance) |
| macOS ARM64 | D1–D3 focused evidence and D4 expected unsupported-host refusal retain their recorded scope. Run `43101701646` at `31608c2dd2ca1ae20e3bdd021d0c36581a01dabc` **ACCEPTED** focused application context and editor qualification, 6/6 per profile plus three Debug editor suites. Signed App Sandbox behavior is **UNVERIFIED**. Managed-native execution and D5 remain **DEFERRED** on this host. [Recorded acceptance](../projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md#focused-native-macos-qualification-accepted) |
| Integrated project/raylib seven-job gate | **OPEN / DEFERRED BY BOSS DECISION.** Earlier green matrix runs and focused acceptances retain their original scopes. [Gate record and original requirements](../projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md) |

## 4. Developer tooling and onboarding

| Area | Status at the baseline | Source and evidence |
| --- | --- | --- |
| VS Code editing and compiler-backed assistance | **IMPLEMENTED** semantic tokens with syntax fallback, pairing/overtype/surrounding, native block indentation, live tabs-by-default or four-space setting, and constructor signatures/named completion. Unambiguous constructs have native indentation rules. Bare-name match arms can still need manual indent. | Commits `c6bd9bb1`, `301afc03` and `4d39a2b1`. [Language configuration](../../editors/vscode/language-configuration.json), [extension](../../editors/vscode/src/extension.ts), [semantic protocol test](../../tests/lsp/semantic_tokens.py), [typing host](../../editors/vscode/src/test/editorBasics.ts), [constructor host](../../editors/vscode/src/test/constructorHost.ts). |
| VS Code project lifecycle | **IMPLEMENTED** manifest discovery, creation and entry selection, separate dirty TOML/source overlays, revision gating, file events and trust restrictions. **PARTIAL** for legacy rename/delete resilience. The previous native Windows constructor handoff observed a legacy-entry watcher failure on both retained and candidate servers. See G01. | [Creation workflow](../../editors/vscode/src/creationWorkflow.ts), [project sessions](../../src/frontend/tooling.cpp), [diagnostic publication and watch handling](../../src/lsp/main.cpp), [project protocol tests](../../tests/project/protocol.py), [native project/editor qualification](../projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md). |
| Diagnostics, navigation and edits | **IMPLEMENTED** completion, hover, signatures, definitions, references, document/workspace symbols, guarded rename and compiler-validated whole-document formatting. **PARTIAL** for code actions and broader rename targets. Build stderr has no first-party problem matcher. | [LSP capabilities and validation](../../src/lsp/main.cpp), [protocol tests](../../tests/lsp), [Stage 10 intelligence](../../tests/stage10/lsp_intelligence.py), [formatting fixture](../../tests/stage10/lsp_source_formatting.py). G02/G05. |
| Neovim | **IMPLEMENTED** native LSP adapter, legacy discovery, navigation, completion, guarded rename and formatting. **PARTIAL** manifest parity. Shipped setup still invokes `--discover` and launches `--entry`; it formats with four spaces. Current project/semantic/constructor behavior in newer actual clients remains **UNVERIFIED** beyond recorded cases. | [Lua adapter](../../editors/neovim/lua/void.lua), [host tests](../../tests/editors/neovim_automatic.py), [setup](../EDITOR_INTEGRATION.md). G05. |
| Vim | **IMPLEMENTED** filetype/syntax/indent and legacy LSP adapter with the pinned plugin. **PARTIAL** manifest parity. Shipped setup is `--discover`/`--entry` and formatting requests four spaces. Rename is deliberately disabled under the pinned client's versioned-edit limits. | [Vim adapter](../../editors/vim/autoload/void.vim), [formatter](../../editors/vim/autoload/void_format.vim), [host tests](../../tests/editors/vim_automatic.py), [setup](../EDITOR_INTEGRATION.md). G05. |
| Run/build and debugger UX | **MISSING** first-party VS Code task contribution for the selected project and launch/attach integration. CLI execution and compiler diagnostics already work. Existing raw LLVM debug metadata is not a breakpoint, stepping or local-value contract. | [Contributions](../../editors/vscode/package.json), [CLI](../../src/main.cpp), [IDE proposal](../PROJECT_ARCHITECTURE_ROADMAP.md#proposed-ide-and-public-usability-milestones). G02/G03. |
| Onboarding and portability | **PARTIAL.** Source installation, Hello World, CLI help, examples and editor instructions exist. Several current pages mix dated status with current instructions. Real VS Code 1.141.0 evidence is version/host scoped. Broader VSCodium and distribution portability remain **UNVERIFIED**. | [First program](../FIRST_PROGRAM.md), [source installation](../install/README.md), [CLI docs](../CLI.md), [extension README](../../editors/vscode/README.md), [onboarding fixture](../../tests/cli). G00/G02. |

## 5. Prioritized gaps

### P0 — G00 · Public binary distribution, conditional on shipping

**MISSING** production bootstrap and deployed production trust. **UNVERIFIED** compliant binary payloads and end-user portability. This blocks a public binary preview under the recorded release boundary. Ordinary local source development has a working route, so no blanket P0 language feature is assigned. Reuse VOIDUP's private trust, staging and rollback and keep compiler install separate from app packaging. A release decision must define allowed platforms, payload closure, provenance/notices/corresponding source, key lifecycle and channel policy. Verify the approved immutable payload on a clean native host for each claimed target, with tampered catalog/package, update/default/rollback and interrupted-install controls. A build or host smoke alone cannot establish those results. [Preview register](../release/PREVIEW_BLOCKERS.md) · [Private manager results](../voidup/PHASE_B_REPORT.md) · [Package fixture producer](../../tools/package_voidup_fixtures.py)

### P1 — G01 · Recoverable project diagnostics during file changes

**PARTIAL.** A Windows legacy entry rename race can produce `Project diagnostic has no VFS owner` and terminate the server. The previous constructor handoff reproduced the failure on a retained server and its candidate. The baseline had a diagnostic publication assertion and a second disk capture using a separate diagnostic collector. Repair ownership at the capture/publication boundary, keeping missing-file diagnostics visible and stale results withheld. Reproduce a rename/removal between captures and repair in the same server process on Windows 10 and Linux. Run `project_protocol`, semantic tokens and the existing real project/editor lifecycle suites. [Publication path](../../src/lsp/main.cpp#L293) · [Capture and comparison](../../src/frontend/compiler.cpp#L7097) · [Session revision tests](../../tests/project/sessions.cpp)

### P1 — G02 · A reproducible create-to-run editor workflow

**MISSING** a trusted project-aware Check/Build/Run path from VS Code. **PARTIAL** onboarding and error presentation. CLI help already names Linux and Windows D5. The install/CLI guides still contain Linux-only packaging instructions and older create/resolve claims. The first-program guide starts with manifestless `main.void`. The extension README still says project creation is unimplemented, and the parser's missing-indent hint still directs users to spaces. Reuse compiler-owned selection and argument-vector execution. Define which buffer contents the task will run and show that policy in the editor. Preserve literal paths, arguments, trust, native approval, source ranges and exit status. Verify on Linux and Windows with a generated project, an arbitrary main filename, a locked local dependency, an intentional compile failure and both successful JIT and native execution. [CLI and package help](../../src/main.cpp#L69) · [Editor contributions](../../editors/vscode/package.json) · [Onboarding](../FIRST_PROGRAM.md) · [Indentation diagnostic](../../src/frontend/compiler.cpp#L846) · [Creation fixture](../../tests/project/creation.py) · [Profile fixture](../../tests/project/profiles.py)

### P1 — G13 · Compiler-owned editor intelligence for unreferenced source

**PARTIAL.** Boss reproduced an open `app.void` with incomplete semantic highlighting and IntelliSense until `main.void` imported it with `use`. Every opened VOID source needs compiler-owned analysis of its current buffer, including semantic tokens, completion, hover, constructor signatures, definitions, validated formatting and diagnostics. Keep this analysis outside the selected execution graph, with the existing import, dependency and visibility rules. Verify the original reproduction before and after adding `use`, and exercise malformed buffers, project switches and closure to confirm results stay current. [Compiler tooling](../../src/frontend/tooling.cpp) · [LSP](../../src/lsp/main.cpp) · [VS Code project host](../../editors/vscode/src/test/projectHost.ts)

### P1 — G14 · Editor intelligence during incomplete edits

Boss reproduced a loss of IntelliSense and semantic highlighting while typing
`if game.` inside a nested TestGame condition. Completion must resolve the
receiver in its current lexical scope, retain accessible members and preserve
ordinary diagnostics and validated source roles as the buffer changes.
[G14 implementation and qualification](#g14-editor-intelligence-recovery-and-vsix-packaging)

### P1 — G16 · Enum completion and semantic highlighting

**IMPLEMENTED.** The compiler resolves enum member completion in expressions,
returns and incomplete `match` arms from the current source. Validated enum
type references have their own semantic token. G15 destructuring supplies the
subject type for both named and unnamed return components.
[G16 implementation and qualification](#g16-enum-completion-and-semantic-highlighting)

### P1 — G03 · Debug information and source debugging

**PARTIAL** emission, **MISSING** a supported source debugging workflow. Explicit `line` and `full` requests only warn today. Define the concrete object/executable symbol contract for the first native AOT lane and decide which requests remain unsupported for JIT. Carry the approved mode through provenance and compilation, preserve current implicit quiet defaults and native resource admission, and use existing platform debugger interfaces for launch/attach. Verify emitted sections and linked symbols with the pinned SDK, then demonstrate breakpoints, stepping, a representative local value, and attach/normal termination on the claimed Linux/Windows hosts. Treat optimized-out values and relocated paths as explicit checks. [Existing profile fixture](../../tests/project/profiles.py) · [LLVM metadata](../../src/backend/llvm/backend.cpp) · [IDE/debugger proposal](../PROJECT_ARCHITECTURE_ROADMAP.md#proposed-ide-and-public-usability-milestones)

### P1 — G04 · Bounded child output and wait

**IMPLEMENTED.** `process.capture` and `process.capture_async` collect bounded
binary prefixes from selected output streams with independent EOF flags and
direct-child status. They share native launch and resource handling with the
existing inherited-stream workflow. The async operation uses checked native
completion and structured cancellation. [Public process API](../process/API.md) · [G04 implementation and qualification](#g04-bounded-subprocess-capture-deadlines-and-cancellation)

### P2 — G05 · Maintenance commands and safer editor fixes

**IMPLEMENTED.** `voidc deps explain` reports the verified source package graph
and lock identity in text or JSON without acquisition or publication. VS Code
offers a compiler-validated, version-guarded correction for literal obsolete
loop headers. Import and dependency diagnostics retain their existing guidance.
Vim/Neovim manifest selection remains a separate follow-up.
[Command and schema](../CLI.md#inspect-project-dependencies) · [Editor guard](../EDITOR_INTEGRATION.md#guarded-source-quick-fix)

### P2 — G06 · Application save/replace workflow

**IMPLEMENTED.** An admitted private directory can atomically save bytes or
UTF-8 text with checked publication and synchronization outcomes. The operation
uses same-directory staging, held native identities and a bounded writer lock.
Async saves are deferred. [I/O API](../io/API.md#atomic-application-saves) · [G06 implementation and qualification](#g06-atomic-application-saves)

### P2 — G07 · TCP/UDP and server applications

**IMPLEMENTED.** Linux x64 and Windows x64 have typed numeric addresses,
bounded DNS resolution, owned TCP listeners and streams, and unicast UDP
sockets. The synchronous and async interfaces share nonblocking native
readiness, deadlines and structured cleanup. [Contract review](../net/TCP_UDP_HANDOFF.md#batch-4-contract-review) · [Addresses](../net/address/API.md) · [TCP](../net/tcp/API.md) · [UDP](../net/udp/API.md)

### P2 — G08 · Measurable edit/build performance and Windows cache

**BASELINE MEASURED / OPTIMIZATION HOLD.** G08 recorded native edit latency,
check, JIT, emission and linking on a frozen manifest application. Same-revision
completion must still observe changed package inputs. Windows persistence
remains a separate ownership and publication design. The historical FILE200
target stays advisory.
[G08 baseline and cache review](../compiler-health/G08_COMPILER_PERFORMANCE.md) · [Historical performance record](../compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md)

### P2 — G09 · Data and text conveniences backed by application examples

**INTEGRATED.** Golden Wrench accepted the Batch 10 source at
`e83c1070e5d2f6b49a2b84c52e21fbf2e6e319b1`. It covers bounded JSON
configuration and explicit typed field defaults. `json.get_or` defaults
only absent properties. The TestGame example uses bounded UTF-8 input,
strict parsing, application validation and G06 private-directory saving.
Large incremental JSON, formatted-date parsing, normalization,
grapheme handling and collection conveniences remain separate work.
[JSON configuration API](../json/API.md#explicit-configuration-defaults) · [G09 qualification](../json/IMPLEMENTATION.md#g09-configuration-and-typed-field-defaults)

### G15 · Named and unnamed multiple returns

The language supports ordered return lists and local destructuring with exact
arity. Generic specialization and receiver dispatch share the same private
typed ABI. Return names are compiler-owned editor metadata. First-class tuples,
discard syntax and multiple-return function values, async results, operators
and native bindings remain outside this contract. [Language contract](../LANGUAGE_BIBLE.md#15-multiple-return-values-not-first-class-tuples-agreed) · [Native qualification](#g15-native-qualification)

### P2 — G10 · Async deadlines and coordination convenience

**INTEGRATED** at `ceefd3f9a75257756cb8ab476868731e4359dcca`. Batch 11 adds bounded
`time.sleep(duration)` to the existing executor and external completion path.
The gameplay example awaits the windup and cancels and drains the idle timer
with its task group. G04 and G07 retain their provider-specific deadlines.
General deadline composition and a bounded async queue are deferred.
[Timer contract and qualification](../ASYNC.md#g10-monotonic-timers) · [Turn example](../../examples/async/turn_timer.void)

### P2 — G11 · Native platform extensions and qualification

**INTEGRATED** at `79f212f2907b3fb5eeeabebb0aa8fa5705b82c86`. Batch 12
qualifies the real Windows raylib 6.0 software provider through the D4 catalog
and D5 package. The G5 fixture exercises the separate retained-callback
contract. The retained raylib callback remains on HOLD. macOS managed native
and D5 remain unsupported. Signed App Sandbox remains unverified. The seven-job
integration gate remains deferred.
[G11 qualification](../projects/G11_WINDOWS_MANAGED_NATIVE_QUALIFICATION.md) · [Platform and gate reconciliation](../projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md)

### P3 — G12 · Optional language, FFI, crypto and archive extensions

**INTEGRATED / INDEPENDENT SOURCE REVIEW COMPLETE** at
`afe14d4c0cd7fd0576ecc194a38c294d7e54ec0a`. Batch 13 packages a
validated game configuration, binary checkpoint and manifest as a deterministic
stored ZIP. Callers supply explicit flat names and bounded byte arrays. The
application uses G06 to publish and reopens the result through the existing ZIP
reader. Acceptance covers independently verified record geometry and CRC,
hostile names, limits, ownership, native JIT and independent AOT.
[Design, example and qualification](../archive/ZIP_EXPORT.md)

Advanced source constructs, generalized FFI/reload, broader crypto, ZIP writer
compression, ZIP64 output and directory traversal remain deferred. New source
operators require consistent constant evaluation, typed IR validation and LLVM
lowering, including edge-value tests. Crypto additions need provider vectors
and misuse tests. New native signatures or reload scope need ABI and
owner/generation lifetime negatives in JIT and independently linked AOT where
applicable. [Language boundaries](../LANGUAGE_BIBLE.md) · [Native bridge](../../include/void/c_primitive.hpp) · [Host tests](../../tests/host_capabilities) · [FFI controls](../../tests/ffi)

## 6. First three proposed implementation batches

### Batch 1 — Reliable project check, build and run from the editor

**G13 + G01 + G02.** Give opened unreferenced files their own compiler analysis, then repair the legacy watcher failure. Add trusted VS Code Check/Build/Run commands using compiler-owned selection, an explicit profile and literal CLI arguments. Update the manifest-first walkthrough and the commands it points to.

**Acceptance.** Native Linux and Windows VS Code can create and select a project with an arbitrary main filename, check it, build it, run it and display a real compiler failure at the correct source. Dirty editor buffers have a declared, tested policy. A rename/delete interleaving leaves the server alive, reports or clears the correct diagnostics and recovers in the same session. Switching roots rejects old results. Restricted Mode does not start a task. Use the existing `cli_contract`, `project_protocol`, `project_creation_process`, `lsp_semantic_tokens` and real project, constructor and typing fixtures with their current assertions.

An open `app.void` receives the same supported editor assistance before and after `main.void` imports it. Test current unsaved contents, actual source errors, and tabs and spaces. Opening the file leaves application selection, build inputs, native execution and dependency authorization intact.

### Batch 2 — Profile-controlled symbols and an initial AOT debugger workflow

**G03.** Define `none`/`line`/`full` for the approved executable lane before changing warnings. Carry the settings through manifest/profile selection, cache/build identity, LLVM metadata, native linking and the editor launch/attach configuration. Review platform-specific symbol formats, debugger choice and supported optimization behavior.

**Acceptance.** The pinned SDK's object and executable inspection demonstrates the agreed mode distinctions. Generated manifests stay minimal and default builds stay quiet. Real Linux and Windows debugger sessions hit a source breakpoint, step and inspect a representative local in the supported configuration. Path-with-spaces and source-location relocation controls pass. Existing O0/O2 and independently linked native tests still pass. Each unsupported execution mode retains a clear diagnostic.

### Batch 3 — Checked subprocess capture for developer tools and applications

**G04.** Add one bounded subprocess workflow that can capture stdout/stderr and wait without blocking an owner async activation. Define output limits, status/error reporting and direct-child cleanup up front. Reuse `process`, `system_error`, the existing resource ledger and the native completion interfaces. Keep current launch authority and the VOIDUP inherited-stdio path.

**Acceptance.** A real helper runs with literal Unicode/space arguments and a controlled environment, captures both streams under backpressure, reports exact status and separates timeout, cancellation and launch errors. Exceeding a limit leaves the documented bounded result and reaps the child. Run the affected handle/ownership tests, `host_capabilities_portable`, `host_capabilities_portable_source` and `voidup_smoke` in native Linux/Windows Debug and Release, with applicable strict Linux checks.

## Audit verification and review boundary

The inspection enumerated all standard-library modules with `git ls-files stdlib` and checked maintained CMake definitions against the retained local configurations using `ctest --test-dir build/llvm23-<configuration> --show-only=json-v1`. The configurations listed 466 Debug, 453 Release and 443 strict registrations. These are inventory counts from existing build trees, with no execution or qualification claim for this SHA. The local pinned Clang and LLD each reported 23.1.4. No builds, CTest execution or hosted actions were part of this audit.

Future local verification should use the PikaOS/Linux host and the actual Windows 10 x64 guest, with native evidence for any macOS claim. Preserve existing suite timeouts and fatal ASan/UBSan with leak detection and the configured exit codes. The integrated seven-job gate remains deferred by Boss. Use disposable outputs and retain original failures under the existing evidence policy. [Workflow settings](../../.github/workflows/ci-link-experiment.yml) · [Archive and retention policy](../projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)
## Batch 1 implementation record

The inventory above describes baseline `4d39a2b121b8ba6fb6cfa3b60d02d8e048b27b59`.
The review worktree implements G13, G01 and G02. The existing parser and binder
analyze an open unreferenced document and its explicit imports as editor
metadata. That analysis cannot lower executable code. Dirty buffers outside
the application graph receive their own diagnostics and assistance. A saved
source still participates in the manifest's existing automatic entry-discovery
rules. [Compiler session](../../src/frontend/tooling.cpp) · [Protocol controls](../../tests/project/protocol.py)

The deterministic rename control renamed the legacy `main.void` immediately
before the second directory read during `voidls --entry` capture. A disposable
Linux `LD_PRELOAD` interceptor counted directory `openat` calls for the fixture
root and performed the rename on call two. The retained baseline exited 2 with
`Project diagnostic has no VFS owner`. The recapture collector lacked the
entry anchor's VFS identity. With the repaired collector, the same interleaving
published `VOID3002` at the original URI and the process exited 0 after normal
shutdown. A legacy watch deletion also invalidates the old overlay while the
document is open. The maintained protocol and VS Code tests rename and restore the file
in the same session and verify diagnostic clearing and navigation recovery.
[Compiler recapture](../../src/frontend/compiler.cpp) · [VS Code project host](../../editors/vscode/src/test/projectHost.ts)

VS Code tasks use the compiler-selected manifest root and any explicit entry
override. Check, Build and Run require saved project sources and manifests.
The task terminal launches a native process with the exact argument vector.
The host fixture exercises spaces, a shell metacharacter, an empty argument
and a literal `${workspaceFolder}`. It checks a generated project with a
locked local dependency, runs its native build and JIT application, verifies
exit status 17, and checks that a compiler error appears and clears in Problems.
Explicit native provider authorization and applications that need a TTY use
the CLI. [Project commands](../EDITOR_INTEGRATION.md#vs-code-project-commands)

The focused CTest selection is
`project_sessions`, `project_protocol`, `lsp_semantic_tokens`,
`lsp_stage10_intelligence`, `lsp_stage10_source_formatting`,
`cli_contract`, `project_creation_process`, `project_profiles`,
`project_managed_native`, `project_dependencies` and `project_selection`.
On PikaOS 4 with LLVM/Clang/LLD 23.1.4, Debug and Release passed 11/11 each.
VS Code 1.141.0 passed the 35-check project host and the semantic and
constructor suites in both profiles. The Debug creation suite passed all nine
host modes, including Restricted Mode with no child launches. The shipped
Neovim and Vim adapters passed their maintained Stage 10 host checks in both
profiles. The extension's seven unit tests and TypeScript build passed.

On the native Windows 10 Home x64 guest, build 19045.6466, Debug passed ten of the eleven CTests
on its first pass. `cli_contract` completed its assertions and hit
`WinError 5` when the fixture tried to delete its copied `voidc.exe` during
temporary-directory cleanup. An isolated rerun passed. The cause of the
initial deletion denial was unidentified. The Debug VS Code project, semantic
and constructor hosts passed. The extension unit fixture initially compared
Windows paths with mixed slash styles. It now compares their normalized
forms and retains the same entry and mode checks. Debug passed all seven
unit tests on rerun. The final Debug watcher build also passed the four
adjacent LSP CTests and the 35-check project host again.

Windows Release passed 11/11 CTests in 91.01 seconds. VS Code 1.141.0
passed the 35-check project host and the semantic and constructor hosts.
All seven extension unit tests passed. Both Windows profiles used the
verified LLVM/Clang/LLD 23.1.4 SDK, pinned provider receipts and a private
test temporary root.

The real VS Code semantic suite intermittently missed one nested auto-indent
after typing Enter. The retained baseline reproduced the same assertion in
three separate runs. The test now makes the nested line's workbench
tokenization current before Enter and waits for the formatter to publish an
edit after the file watcher settles. The same typing and formatting results
are required.

The native project host is run from `editors/vscode` after `npm run build`
using `node out/test/runProjectHost.js`. Set `VOIDLS_PATH` to the profile's
built server beside its compiler, `VSCODE_EXECUTABLE_PATH` to the native
VS Code 1.141.0 executable, and `VOID_TEST_PROJECT_TASKS` to `debug` or
`release`. The related suites use `node out/test/runSemanticHost.js` and
`node out/test/runSemanticHost.js --constructor`.

### Changed-file inventory

- Compiler and language server — `include/void/frontend.hpp`, `include/void/tooling.hpp`, `src/frontend/compiler.cpp`, `src/frontend/project_selection.inc`, `src/frontend/tooling.cpp`, `src/lsp/main.cpp`.
- VS Code commands — `editors/vscode/package.json`, `editors/vscode/src/config.ts`, `editors/vscode/src/extension.ts`, `editors/vscode/src/projectTask.ts`.
- VS Code fixtures — `editors/vscode/src/test/config.test.ts`, `editors/vscode/src/test/creationHost.ts`, `editors/vscode/src/test/editorBasics.ts`, `editors/vscode/src/test/projectHost.ts`, `editors/vscode/src/test/runProjectHost.ts`, `editors/vscode/src/test/semanticHost.ts`.
- Protocol fixtures — `tests/lsp/semantic_tokens.py`, `tests/project/protocol.py`.
- Documentation — `docs/CLI.md`, `docs/EDITOR_INTEGRATION.md`, `docs/FIRST_PROGRAM.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `editors/vscode/README.md`.

## Batch 2 native source debugging

Baseline `29d50c9a21cdf93579941b872ef815b2ae7f881a`. The manifest selects Debug
O0 or Release O2 by default and already records the requested debug mode in
profile provenance. Both profiles default to `none`. The baseline accepted
`line` and `full` with VOID5012 and lowered the same LLVM compile unit,
subprograms, type descriptions and source locations regardless of that
setting. On PikaOS, LLDB 23.1.4 stopped inside a VOID function at the expected
line in a directory with spaces. It could not inspect the initialized local.
The frontend resolves written names to typed slots, but the baseline did not
describe those slots to the debugger. The Runtime's failure locations are
separate from LLVM debug metadata. VS Code has project Check, Build and Run
commands from Batch 1 and no debugger integration.

The initial contract covers native project `emit` and `build`. `none` emits
no optional VOID source debug metadata. `line` emits source files, locations
and subprogram ranges for breakpoints and stepping. `full` adds source types
and names and locations for supported scalar bindings in unoptimized code.
Optimization remains under the selected profile, and optimized builds may
inline or remove source statements. Support for additional local scopes,
captured values and Runtime-managed values requires separate qualification.
JIT execution and application packaging remain outside this debugger lane.

ELF can carry DWARF in the executable. The pinned LLD 23.1.4 COFF linker
produced a PE image with embedded DWARF using `/debug:dwarf`. LLDB resolved
the probe's source breakpoint and local description from that image.
On Windows 10 x64, native GDB 17.2 stopped inside the probe, stepped to the
return, printed `doubled = 14` and observed normal exit. The Windows SDK
archive has no `lldb.exe`. Embedding the symbols keeps publication a
single-file operation.

### Implementation and focused qualification

Project `emit` and `build` now apply the selected mode on Linux x64 GNU and
Windows x64 MSVC. Profile keys record the requested mode and effective
optimization. The Windows linker uses the verified Clang SDK's LLD and
keeps DWARF inside the PE executable. The frontend records authored scalar
bindings from validated function identities and typed slots. In O0, the
LLVM lowerer attaches variable locations to their owning declaration
stores. Runtime failure locations and the application publication path
remain independent of this metadata. On these hosts, project `check`,
`emit` and `build` accept explicit `line` and `full` without VOID5012.
JIT, packaging and unsupported native targets still report VOID5012 for
these requests.

The focused CTest selection was `project_native_debug_info`,
`project_profiles`, `project_sessions`, `cli_contract` and
`project_application_process`. The new test checks linked images and
emitted objects for Debug `none/line/full` and Release `line/full`. It
also checks repeated object bytes, native execution, rejection of an
unknown mode and single-file Windows publication. Existing timeouts
were preserved.

| Native host and compiler configuration | Focused CTest | Native debugger |
| --- | --- | --- |
| PikaOS 4 x64 Debug | 5/5, 42.61 s | LLDB 23.1.4, `line` and `full` passed |
| PikaOS 4 x64 Release | 5/5, 9.26 s | LLDB 23.1.4, `line` and `full` passed |
| Windows 10 Home x64 (10.0.19045.6466) Debug | 4/5, 134.10 s. `cli_contract` failed in fixture cleanup. | GDB 17.2, `line` and `full` passed |
| Windows 10 Home x64 (10.0.19045.6466) Release | 5/5, 34.80 s | GDB 17.2, `line` and `full` passed |

PikaOS also passed `project_protocol`, `lsp_semantic_tokens` and
`application_workflow_stage9` in both Debug and Release. The strict
RelWithDebInfo build passed `project_sessions`, `project_profiles` and
`project_native_debug_info` in 15.26 seconds with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
Native debugger sessions ran separately from the leak-detection checks.

Both native debuggers ran the executable under test, hit source breakpoints
in a value-structure initializer and an imported ordinary function, stepped
to the next statement and observed exit 19. Sources and executables lived
under paths containing spaces. In `full` mode the debuggers read
`initial = 7`, `assigned = 8`, `input = 8` and `doubled = 16`. The
debugger runs used a Debug application from each compiler configuration.
Release O2 has verified DWARF and normal native execution. Local-variable
locations are limited to supported O0 parameters and function-level
numeric/Boolean bindings. Managed and captured values, nested bindings
and async frames remain unqualified. Set breakpoints on executable
statements. LLDB resolved a `main` declaration-line breakpoint to both
the generated application adapter and the authored entry body.

The real VS Code 1.141.0 host on PikaOS passed with both compiler
configurations and the official LLVM LLDB DAP extension 0.4.1 using pinned
`lldb-dap`. The tests rejected a missing adapter, missing executable and
dirty source, built the compiler-selected nested project and launched its
executable. They hit a VOID breakpoint, stepped, inspected `observed = 14`
and observed exit 17 and session termination. Extension typechecking and
all seven extension unit tests passed. The first-project walkthrough also
hit its breakpoint in LLDB, printed `answer = 14` and exited 14. Windows
uses native GDB at the command line. PDB-based debugging and a Windows
editor launch remain outside the qualified workflow.

Windows Debug `cli_contract` failed at `tests/cli/contract.py:79` in
`TemporaryDirectory` cleanup with `[WinError 5] Access is denied` on
`broken installation\voidc.exe`. Its isolated CTest retry failed the
same way.
A disposable diagnostic observed that the assertions completed and the
copied file became removable after retries with delays of 0.2 and 1 seconds.
The process responsible for the denial was unidentified. Batch 1 also
recorded this cleanup failure. The original failing results are retained
here and the maintained fixture is unchanged.

Set `build_dir` to the configured native build directory and replay the suite.

```sh
ctest --test-dir "$build_dir" --no-tests=error --output-on-failure \
  -R '^(project_native_debug_info|project_profiles|project_sessions|cli_contract|project_application_process)$'
```

To reproduce the native debugger checks, run
`python3 -B tests/project/native_debug.py <absolute-voidc> <absolute-llvm-dwarfdump> --lldb <absolute-lldb>`
on Linux or the same fixture with `--gdb <absolute-gdb.exe>` on Windows.
Use the verified LLVM 23.1.4 SDK for the compiler and LLVM tools. Run
`npm run build` from `editors/vscode` and then
`node out/test/runDebugHost.js` for the Linux editor check. Set `VOIDLS_PATH`
to the profile's server, `VSCODE_EXECUTABLE_PATH` to the installed editor,
`VOID_LLDB_DAP` to the pinned adapter binary and `VOID_LLDB_EXTENSIONS` to a
directory with the official adapter extension installed.

The task-owned Windows workspace, host transfer directory and PikaOS
temporary workspace were removed. Available space in the Windows guest
rose from 6,748,295,168 to 12,733,493,248 bytes. Available space on the
Linux temporary filesystem rose from 16,528,011,264 to 16,553,705,472 bytes.

### Changed-file inventory

- Build registration — `CMakeLists.txt`.
- Profile and native emission — `include/void/backend.hpp`, `include/void/frontend.hpp`, `include/void/native_build.hpp`, `include/void/project.hpp`, `src/backend/llvm/backend.cpp`, `src/frontend/compiler.cpp`, `src/frontend/project.cpp`, `src/main.cpp`, `src/native_build.cpp`.
- Compiler fixtures — `tests/project/native_debug.py`, `tests/project/profiles.py`, `tests/project/sessions.cpp`.
- VS Code integration and fixtures — `editors/vscode/package.json`, `editors/vscode/src/extension.ts`, `editors/vscode/src/test/debugHost.ts`, `editors/vscode/src/test/runDebugHost.ts`.
- Documentation — `docs/CLI.md`, `docs/EDITOR_INTEGRATION.md`, `docs/FIRST_PROGRAM.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `editors/vscode/README.md`.

## G14 editor intelligence recovery and VSIX packaging

Baseline `6ef25f38644b326a398bb334ba0da4954d57ca4c`. The original
two-file TestGame source works after completing `if game.is_running:`.
The deliberate `if game.` edit reports `VOID1102` at line 15, column 21
and `VOID1101` at the following indent. The baseline returned seven
completion labels, 25 semantic tokens, hover and the initializer
definition before the edit. During the edit it returned zero completion
items, zero semantic tokens and no hover or definition. Completing the
condition restored the original results. Real VS Code 1.141.0 rendered
semantic color before the edit and failed to show the property suggestion
after the edit, consistent with the protocol results.
[Exact TestGame fixture](../../tests/lsp/fixtures/editor_recovery/main.void)

The compiler's completion probe inserted a member name at the cursor,
but the parser still required the condition's colon and block before
the semantic checker could reach the receiver. Separately, the failed
project graph supplied no index for semantic tokens, hover or definition.
The editor parser now admits an unfinished member in an `if` header
without guessing the member or its type. It checks the authored receiver
in the existing lexical scope and uses the ordinary member resolver for
completion. The current snapshot can also supply read-only presentation
from a separately checked graph. The parser retains the original source
offsets, applies at most eight supported repairs per parse and leaves
the ordinary parser and diagnostics on the normal validation path.
The presentation graph cannot lower IR or become the selected project.
Formatting, rename and references still require ordinary validated
analysis. Revision, close and project selection guards also apply to
recovered presentation.

The supported recovery covers a simple receiver and a missing or
partially typed member at the end of an `if` header. It can also discard
a malformed leaf `pass` statement or a bare initializer hole for a
local with no subsequent written use. When an error can change a live
binding, import authority or project selection, ordinary validation still
withholds the affected presentation. The source protocol fixture exercises
an invalid receiver, a shadowing incomplete declaration, inaccessible
fields and methods, an undeclared package, stale versions, document closure,
reselection and UTF-16 positions in nested `if`, `else`, `for` and
`match` scopes. It also verifies that the original errors still fail
`voidc check`. The real workbench test observes the rendered completion
widget and the semantic color on a newly named dirty local. The project
host retains the Batch 1 unreferenced-source checks.
[Protocol regression](../../tests/lsp/editor_recovery.py) · [Workbench regression](../../editors/vscode/src/test/recoveryHost.ts)

The baseline `npm run package` exited 1 because vsce could not rewrite
the Batch 2 relative debugger link from the packaged README. The repaired
README uses full repository URLs for its documentation. The packaging
command and extension metadata retain the existing configuration. The
README link unit test catches repository-relative links and the installed
project host verifies the actual archive's README. On Linux, `npm ci`,
`npm run package` and `npm test` passed with Node 24.19.0. The resulting
`void-language-0.1.0.vsix` passed the ZIP CRC and eight-member inventory
checks. VS Code installed it in a disposable profile and activated that
copy in the project host. The installation recipe reports the failing
stage's exit status and stops before installing if preparation fails.
The generic package uses a separately built matching `voidls` for G14.
The link regression failed with the baseline README. A controlled package
exit of 17 printed that status and left the interactive shell running.
`npm ci` reported six audit findings in the existing lockfile.
The Windows packaging run also passed with Node 24.19.0 and produced the
eight-file archive. The Windows installed-project fixture locates the
versioned editor CLI from the stock launcher and tests the package in
an isolated extension directory.
[Build and installation](../EDITOR_INTEGRATION.md#build-development-host-and-local-installation)

The focused CTest selector covered `project_sessions`, `project_protocol`,
`lsp_semantic_tokens`, `lsp_editor_recovery`, `lsp_stage10_intelligence`,
`lsp_stage10_source_formatting` and `numeric_enum_lsp`. The editor used
VS Code 1.141.0 and its dedicated recovery, semantic, constructor and
installed project hosts. The project host exercised Check, Build and
Run in each compiler profile and passed 36 checks per Linux run.

| Native configuration | CTest | VS Code hosts |
| --- | --- | --- |
| PikaOS 4 x64 Debug | 7/7, 17.70 s | Recovery, semantic, constructor and installed project passed |
| PikaOS 4 x64 Release | 7/7, 2.81 s | Recovery, semantic, constructor and installed project passed |
| Windows 10 Home x64, 10.0.19045.6466 Debug | 7/7, 44.04 s | Recovery, semantic, constructor and installed project passed |
| Windows 10 Home x64, 10.0.19045.6466 Release | 7/7, 14.65 s | Recovery, semantic, constructor and installed project passed |

The installed Windows project host also passed 36 checks per profile.
All eight extension unit checks passed against both compiler profiles
on both hosts.

PikaOS strict RelWithDebInfo passed all seven CTests in 23.69 seconds with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
The first strict run had used an object compiled before the typed-prefix
case was implemented and failed that case. Rebuilding the affected
translation unit and relinking produced the final 7/7 result.
The existing Linux Debug LLDB DAP host also passed with extension 0.4.1
and pinned `lldb-dap`. It exercised missing-adapter rejection, a source
breakpoint, a step, local inspection and normal exit.

The Windows SDK bootstrap initially stalled in the system archive reader
before extraction. Python extracted the same SHA-256-verified LLVM 23.1.4
SDK successfully. Both native Windows profiles used the pinned provider
receipts and private test roots.

From a configured native build, set `build_dir` to its path.

```sh
ctest --test-dir "$build_dir" --no-tests=error --output-on-failure \
  -R '^(project_sessions|project_protocol|lsp_semantic_tokens|lsp_editor_recovery|lsp_stage10_intelligence|lsp_stage10_source_formatting|numeric_enum_lsp)$'
```

From `editors/vscode`, set `VOIDLS_PATH` and `VSCODE_EXECUTABLE_PATH` to the
matching native server and editor. Build the extension and run
`node out/test/runSemanticHost.js --recovery` with a free local
`VOID_TEST_CDP_PORT`. Omit the switch for the semantic suite or pass
`--constructor` for the constructor suite. To test the installed archive,
set `VOID_TEST_VSIX` to the packaged VSIX and `VOID_TEST_PROJECT_TASKS` to
the selected profile, then run `node out/test/runProjectHost.js`.

The disposable workspaces and transfer files were removed after
qualification. Free space in the Windows guest rose from 5,537,140,736
to 13,675,732,992 bytes. The final generic VSIX is retained in the
extension's ignored `dist` directory.

### Changed-file inventory

- Compiler and session — `include/void/frontend.hpp`, `include/void/tooling.hpp`, `src/frontend/compiler.cpp`, `src/frontend/project_selection.inc`, `src/frontend/tooling.cpp`, `src/lsp/main.cpp`.
- Compiler verification — `CMakeLists.txt`, `tests/lsp/editor_recovery.py`, `tests/lsp/fixtures/editor_recovery/app.void`, `tests/lsp/fixtures/editor_recovery/main.void`, `tests/lsp/fixtures/editor_recovery/void.project.toml`, `tests/project/sessions.cpp`.
- VS Code verification — `editors/vscode/src/test/config.test.ts`, `editors/vscode/src/test/projectHost.ts`, `editors/vscode/src/test/recoveryHost.ts`, `editors/vscode/src/test/runProjectHost.ts`, `editors/vscode/src/test/runSemanticHost.ts`.
- Documentation — `docs/EDITOR_INTEGRATION.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `editors/vscode/README.md`.

## G04 bounded subprocess capture, deadlines and cancellation

The review worktree is based on `131db64368751f9fdac5608aba11f3241dd9ba6d`.
The baseline `process.start` already enforced an explicit image, literal
arguments, a complete environment and explicit cwd. Its inherited-stdio
child resource owns wait, close and direct-child reap. VOIDUP's manager uses
that sequence. G04 adds a one-shot capture owner while retaining the
existing launch and error translation.
[Baseline caller](../../apps/voidup/manager.void) · [Public process contract](../process/API.md)

The capture owner drains both readers in bounded increments and reaps
the direct child before publishing. Linux uses close-on-exec pipes and
spawn descriptor closure. Windows uses private named pipes and an
explicit child inheritance list. A checked launch error is distinct
from a launched child's outcome. EOF flags, partial bytes, original exit
status and separate native I/O and cleanup codes remain available after
an abort. Async capture copies launch inputs and runs on an existing
bounded native worker. The owner activation stays available while that
worker drains. Task cancellation joins native cleanup and propagates the
scheduler's cancellation outcome. Captured bytes become managed values
only on the owner thread. Descendant writers can postpone EOF within
the same deadline and budgets. Abort targets the direct child.
[Runtime ownership](../ASYNC.md#ownership-and-validation)

The capture fixture covers native byte patterns with embedded NUL,
concurrent writers, single-stream and aggregate limits, zero-byte EOF,
launch error, literal Unicode and empty arguments, environment
replacement, explicit cwd, normal and abnormal status, unrelated handle
inheritance, deadlines and direct-child reap. It verifies cancellation
with quiet and active writers, descendant survival and repeated ownership
cleanup. The Linux probe also injects a read error after positive reads
and checks the partial output and native error. Source tests exercise
normal and optimized JIT, independently linked AOT, actual async worker
progress, group drain and cancellation propagation. [Native and source fixture](../../tests/host_capabilities/process_capture.py)

The first Linux Debug capture attempt exposed a fixture race. The child
could publish its PID before both pipes had been drained, so an immediate
cancellation could legitimately leave an empty prefix. The cancellation
fixture now writes past each pipe's capacity before publishing the PID.
Two initial Windows configure attempts failed because the disposable source
snapshot omitted the two approved curl review diffs required by provenance
verification. Restoring both exact tracked inputs completed the original
verification step. Windows also rejected the initial temporary build and
staging locations. The task workspace inherited an effective
Authenticated Users mutation grant, and the first VOIDUP build still used
the ordinary temporary root. Protecting the task-owned ancestor and
selecting the existing per-profile private root before compilation
resolved both setup failures.

The first Windows ownership probe counted handles before native process
startup and Win32 error formatting had initialized their process-wide
caches. A bounded snapshot showed first-launch semaphores, tracing and
registry handles, `\Device\CNG`, and the
`KernelBase.dll.mui` message-resource file after the first launch error.
The count stayed fixed through deadline, cancellation and descendant
controls. No additional child process, thread or capture pipe handle
remained in the snapshot. The fixture now verifies one successful and
one missing-image launch before establishing its Windows baseline.
Repeated launches still require exact handle-count equality.

The next Debug run reached independently linked AOT and returned fixture
status 14 on a short-lived overflowing child. A bounded replay reproduced
it in both normal and optimized AOT. The initial Release build also
reproduced status 14 in optimized JIT. The failing executions returned the correct
`output_limit` outcome and four stdout bytes with native I/O code zero,
but reported cleanup code 5. The child had begun its natural exit before
`TerminateProcess` and had yet to signal at the immediate zero-time poll.
The Windows close path now confirms exit with the owned handle before
resolving that access-denied race. A wait failure or other termination
error is still reported. The native fixture repeats the small-output
overflow case alongside the large backpressure control.
[Win32 termination and process-handle wait](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess)

From a configured build, set `build_dir` to its path and run the shared
selection. Add `|host_capabilities_native` inside the selector on Linux.

```sh
ctest --test-dir "$build_dir" --no-tests=error --output-on-failure \
  -R '^(process_capture|host_capabilities_portable|host_capabilities_portable_source|voidup_smoke|native_workers|io_native_jobs|async_external|async_lifecycle)$'
```

Windows uses the existing private native test-root helper. Strict Linux
uses `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
The new `process_capture` registration has a 300-second timeout. Existing
test assertions and timeouts remain as recorded in the baseline.

PikaOS 4 x64 ran kernel `7.2.11-pikaos` with glibc 2.44. The Windows
guest reported Windows 10 Home x64, build `19045.6466`. Both used the
pinned LLVM/Clang/LLD 23.1.4 and verified provider inputs.

| Native configuration | Focused CTests | Capture |
| --- | --- | --- |
| PikaOS 4 x64 Debug | 9/9, 178.19 s | 51.90 s |
| PikaOS 4 x64 Release | 9/9, 52.20 s | 18.02 s |
| PikaOS 4 x64 strict RelWithDebInfo | 9/9, 169.02 s | 50.33 s |
| Windows 10 Home x64 Debug | 8/8, 271.07 s across two invocations | 82.03 s |
| Windows 10 Home x64 Release | 8/8, 82.98 s across two invocations | 28.59 s |

The final capture-only replay also passed in each PikaOS profile after
the Windows fix and the added failure diagnostic. Capture ran in
51.21 s in Debug, 18.92 s in Release and 52.41 s in strict RelWithDebInfo.
The strict focused run and replay reported no sanitizer or leak failure.
Both Windows profiles passed the final capture registration followed by
all seven compatibility registrations. Registration validation found
active commands and the expected timeouts for all focused tests on
both hosts, with no skip properties. The inherited-stream process
controls and VOIDUP smoke passed in every configuration.

The task-owned Windows workspace, Linux scratch directory and transfer
directory were removed after verification. Windows free space rose from
7,279,407,104 to 13,656,858,624 bytes. Available space on the Linux
checkout filesystem rose from 368,835,530,752 to 369,085,435,904 bytes.
The three maintained Linux build trees remain available for replay.

[Runnable example](../../examples/process/capture.void) · [Archive and retention policy](../projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)

### Changed-file inventory

- Public API and example — `stdlib/process.void`, `stdlib/README.md`, `docs/process/API.md`, `examples/process/capture.void`.
- Compiler and ABI — `include/void/host_abi.hpp`, `include/void/host_ops.hpp`, `src/frontend/host.inc`, `src/ir/host.inc`, `src/backend/llvm/host.inc`, `src/backend/llvm/backend.cpp`.
- Runtime — `include/void/runtime.hpp`, `src/runtime/coordination.cpp`, `src/runtime/host_internal.hpp`, `src/runtime/host_abi.cpp`, `src/runtime/host_process.cpp`, `src/runtime/host_process_windows.cpp`.
- Verification — `CMakeLists.txt`, `tests/host_capabilities/process_capture.cpp`, `tests/host_capabilities/process_capture.py`, `tests/host_capabilities/process_capture_sync.void`, `tests/host_capabilities/process_capture_async.void`.
- Engineering documentation — `docs/ASYNC.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`.

## G07 native TCP, UDP and DNS

The review worktree starts at `77986362acf8b40d52aacbbdb324665b4484155b`.
The public modules provide numeric address parsing and formatting, scoped
IPv6, typed endpoints, bounded DNS and native TCP and UDP resources. TCP
supports sequential hostname fallback, independent accepted streams,
bounded reads, EOF, write progress and write half-close. UDP preserves
message boundaries, including empty datagrams. An oversized receive
consumes one message and returns checked sender and size evidence.
[Addresses](../net/address/API.md) · [TCP](../net/tcp/API.md) · [UDP](../net/udp/API.md) · [Errors](../net/error/API.md)

One Runtime-owned native service drains a bounded command queue. It
owns nonblocking sockets, uses `poll` or `WSAPoll`, and feeds readiness
to one c-ares channel per DNS lookup. The existing HTTP provider lease
owns process initialization. Source async code uses external completion
tickets and can run its peers with one source worker. Native operations
charge capacity before snapshotting bytes. Socket quota survives through
terminal-wrapper retirement, and native buffer quota survives through
result publication. Acquisition abandonment and admitted TCP data
cancellation close the owned socket on the provider. UDP cancellation
can leave one whole-message effect. [Reviewed architecture](../net/TCP_UDP_HANDOFF.md#batch-4-contract-review) · [Async ownership](../ASYNC.md#ownership-and-validation)

The transport implementation admits Linux x64 and Windows x64. macOS
transport remains outside this batch. The resolver uses the already
pinned c-ares 1.34.9. Installed source modules use the existing standard
library directory and native packages use the existing provider link
inputs. The API has no public raw handles, multicast membership, TLS
server or HTTP server facilities. The proposed throughput and idle-CPU
measurement campaign was outside the focused qualification.

The native fixture uses existing local DNS infrastructure for A and
AAAA answers, NXDOMAIN, no data, a stalled query, TCP DNS fallback,
the endpoint-count limit and cancellation isolation. It exercises both
TCP families, connect refusal, timed partial writes with reduced native
socket buffers, reset, independent stream ownership, maximum IPv4 UDP payload,
noninheritance, repeated close and shutdown with pending accepts and
receives. The source fixtures test normal and optimized JIT and
independently linked AOT. One-worker source execution overlaps pending
socket operations with file work and a local HTTP request. The LSP check
queries public definitions and hover at UTF-16 positions after non-BMP
text. The echo server and client run as independently linked loopback
applications. [Maintained tests](../../tests/net/socket) · [Examples](../../examples/net_sockets)

Early development checks rejected private-field construction, direct
numeric-enum extraction, a fixture name that shadowed `udp.datagram`,
and ambiguous native helper names. The source now uses a private value
initializer and explicit family conversion. The fixture uses distinct
names and includes the Runtime definition for its one-worker adapter.
The first corrected Linux Debug run passed both new registrations.
An intermediate Linux Debug run built the Runtime archive again after
its native-link manifest had been generated. Both socket and G04 source
tests rejected the mismatch before linking. The subsequent qualification
build checks the Runtime archive digest against the final generated
manifest before executing the focused selection.

The first Windows Debug run stopped the refusal control at its own
two-second deadline. A direct Winsock comparison reproduced the native
refusal after 2.03–2.06 seconds, for both a closed listener and a bound
port with no listener. `WSAPoll` reported `0x13` and `SO_ERROR` returned
`10061` in all four controls. The new fixture allows four seconds to
observe the same required native refusal. The public timeout and Winsock
operation are unchanged. [Winsock readiness](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll)

The same run could not start two existing HTTP fixtures because their
test-only certificate executable was absent. The maintained recipe first
reported a missing Perl prerequisite. Using the Windows guest's installed
Git Perl with the pinned inputs produced the verified certificate tool
and loopback HTTP/2 wheels in the disposable workspace.

The first Windows one-worker JIT replay used all-exports emission and
exceeded the 90-second fixture subprocess bound.
A disposable traced run reached the end of the normal source execution
with HTTP and socket operations complete. The independently linked
fixture completed in 1.44 seconds on the same Windows Debug host.
The one-worker networking mode uses the CLI's entry-only emission and
still executes normal and optimized JIT. An overlapping diagnostic
held the old executable open during the first relink. The next build
waited for that process to retire.

The focused selector for a configured native build is

```sh
ctest --test-dir "$build_dir" --no-tests=error --output-on-failure \
  -R '^(net_sockets|net_sockets_lsp|process_capture|host_capabilities_portable|native_workers|io_native_jobs|async_external|runtime_invariants|http_native|http_dns|http_timeouts|http_wire|http_lsp)$'
```

The new socket and LSP timeouts are 300 and 180 seconds. Strict Linux
uses `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. Windows
uses the existing private-root staging helper before source builds and
tests. [Retention policy](../projects/VOID_ARCHIVE_AND_RETENTION_POLICY.md)

PikaOS 4 x64 ran kernel `7.2.11-pikaos`. The Windows guest reported
Windows 10 Home x64 build `19045.6466`. Both used LLVM, Clang and LLD
23.1.4. All thirteen selected registrations had active commands and
no skip properties. The final native-link inventory matched the
Runtime archive digest in every profile.

| Host and profile | Focused CTests | Final socket fixture |
| --- | --- | --- |
| PikaOS 4 Debug | 13/13 in 206.29 s | 71.44 s |
| PikaOS 4 Release | 13/13 in 85.48 s | 22.90 s |
| PikaOS 4 strict RelWithDebInfo | 13/13 in 215.34 s | 77.30 s |
| Windows 10 Debug | 13/13 across the initial run and targeted replays | 115.21 s |
| Windows 10 Release | 13/13 in 101.19 s | 33.62 s on replay |

The final Linux socket replay included the revised fixture and entry
emission. The strict selection and replay reported no ASan, UBSan or
leak failure. The Windows Debug HTTP replay passed both checks in
22.66 seconds. Existing G04 capture, native-resource ownership,
external completion and HTTP compatibility passed in all five
profiles.

The additional native source controls passed on both Windows profiles
and all three Linux profiles. They exercised IPv6 UDP loopback,
exclusive TCP bind, public hostname connect and refusal of a numeric
literal supplied to the hostname API with a smaller per-candidate
budget.

The Windows workspace, Linux scratch outputs and the transfer directory
were removed. Windows free space rose from 6,702,731,264 to 13,652,996,096
bytes. Available space on the Linux checkout filesystem rose from
368,589,406,208 to 368,779,608,064 bytes. The maintained Linux build trees
and the original pinned provider inputs remain available for replay.

### G07 changed-file inventory

- Compiler and private ABI — `include/void/host_abi.hpp`, `include/void/host_ops.hpp`, `src/frontend/host.inc`, `src/ir/host.inc`, `src/backend/llvm/host.inc`, `src/backend/llvm/backend.cpp`.
- Native Runtime — `include/void/runtime.hpp`, `src/runtime/array.cpp`, `src/runtime/host_abi.cpp`, `src/runtime/network.hpp`, `src/runtime/network_address.cpp`, `src/runtime/network.cpp`, `src/runtime/network_abi.cpp`.
- Standard library — `stdlib/net/address.void`, `stdlib/net/error.void`, `stdlib/net/tcp.void`, `stdlib/net/udp.void`, `stdlib/README.md`.
- Verification — `CMakeLists.txt`, `tests/io/runner.cpp`, `tests/net/socket/native.cpp`, `tests/net/socket/run.py`, `tests/net/socket/lsp.py`, `tests/net/socket/source_sync.void`, `tests/net/socket/source_async.void`.
- Examples — `examples/net_sockets/addresses.void`, `examples/net_sockets/tcp_echo_server.void`, `examples/net_sockets/tcp_echo_client.void`, `examples/net_sockets/udp_loopback.void`.
- Documentation — `docs/ASYNC.md`, `docs/net/TCP_UDP_HANDOFF.md`, `docs/net/address/API.md`, `docs/net/error/API.md`, `docs/net/tcp/API.md`, `docs/net/udp/API.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`.

## G15 native qualification

The original qualification used the unstaged G15 patch on
`bf5e6dc0c335b82e11df81cb540a6364110d3523`. The 16 compiler, build
and fixture files in the Windows overlay matched the local files byte
for byte. Both hosts used LLVM, Clang and LLD 23.1.4 with the existing
verified provider profiles. The guest OS reported Windows 10 Home x64
build 19045.6466.

`multiple_returns` compares uncached JIT and independently linked native
executables at O0 and O2, including output and exit status. It exercises
both return forms, mixed scalar and user-defined types, cross-module calls,
generic specialization, value-receiver mutation, virtual and interface
dispatch, ordered evaluation, deferred cleanup and GC reachability. The
TestGame fixture covers successful validation, parse failure, range
rejection and usage. The negative cases cover declaration and call arity,
visibility, readonly authority, partial binding and unsupported call
categories. Parenthesized single results, ordinary `result` and an
indirect single-result closure have positive controls. [Execution fixture](../../tests/frontend/multiple_returns.void) · [TestGame](../../examples/multiple_returns/test_game.void) · [Harness](../../tests/frontend/multiple_returns.py)

| Host and profile | `multiple_returns` | `lsp_multiple_returns` | Focused CTests |
| --- | ---: | ---: | --- |
| PikaOS 4 x64 Debug | PASS, 50.63 s | PASS, 0.66 s | 15/16, 119.79 s |
| PikaOS 4 x64 Release | PASS, 12.08 s | PASS, 0.33 s | 15/16, 41.77 s |
| PikaOS 4 x64 strict ASan/UBSan with leaks | PASS, 52.01 s | PASS, 1.04 s | 15/16, 148.07 s |
| Windows 10 x64 Debug | PASS, 124.01 s | PASS, 2.20 s | 11/12, 280.25 s |
| Windows 10 x64 Release | PASS, 22.09 s | PASS, 1.02 s | 11/12, 58.10 s |

The sole failure in each original selection was the baseline `frontend` tab-rejection assertion
at `tests/frontend/tests.cpp:51`. The retained G07 compiler accepted the
exact source in a separate check. Tab indentation was admitted in
`301afc03e4a613cb10dce9107f6ebe2f0c02aa63`. The initial G15 patch left the
lexer and that assertion intact. The strict run used fatal ASan and
UBSan with leak detection and reported no sanitizer finding.

The common selection includes `generics_source`, `project_sessions`,
`project_protocol`, `lsp_editor_recovery`, `lsp_semantic_tokens`,
`lsp_stage10_intelligence`, `net_sockets`, `net_sockets_lsp`,
`process_capture`, `frontend` and the two new tests. Linux also runs
`generics_hint_execution`, `generics_hint_lsp`, `inheritance_source`
and `interfaces_source` under their existing registration conditions.
The Windows G15 fixture itself executes both virtual and interface calls.
The new CTest limits are 240 s for execution and 180 s for LSP.
All selected tests had active commands and no skip properties.

Native VS Code 1.141.0 with Node 24.19.0 passed the semantic host suite
on PikaOS and Windows 10. The workbench used the real VOID extension and
Debug `voidls`. Checks covered both signature labels, named-label
highlighting, typed local completion, hover, unsaved return metadata,
UTF-16, failed-analysis recovery and project switching. Linux also passed
the constructor suite and the G14 workbench recovery suite with visible
popup and color checks. [Native LSP tests](../../tests/lsp/multiple_returns.py) · [VS Code host](../../editors/vscode/src/test/semanticHost.ts)

Replay the G15 tests in each configured profile with
`ctest --test-dir BUILD --output-on-failure -R '^(multiple_returns|lsp_multiple_returns)$'`.
The execution harness uses the profile's generated `void-native.txt` to
invoke the matching Clang and LLD separately after `voidc emit`. The strict
run sets `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. The Windows
tests use a private root verified by `void_windows_native_staging`.
For the real editor, run `npm run build` and
`node out/test/runSemanticHost.js` from `editors/vscode` with
`VOIDLS_PATH` and `VSCODE_EXECUTABLE_PATH` set to the native executables.

The disposable guest workspace, source transfers, editor profiles and local
scratch logs were removed. Guest free space rose from 5,737,869,312 to
13,768,192,000 bytes. Linux scratch filesystem free space rose from
16,200,609,792 to 16,574,337,024 bytes. The maintained Linux build profiles
and existing pinned provider inputs remain available for replay.

### G15 final acceptance correction

The frontend suite now runs the same nested program with consistent tabs
and consistent spaces through normal and O2 JIT. It checks the primary
diagnostic and exact location for a mixed prefix (2:2), a style change in
the same block (3:1), and a dedent to a nonexistent level (3:3). This
corrects the baseline test mismatch with the accepted indentation behavior.
[Frontend regression](../../tests/frontend/tests.cpp#L50)

The correction uses the original focused CTest selections, timeouts and
LLVM/Clang/LLD 23.1.4 profiles. Each selection includes `frontend`.

| Host and profile | `frontend` | `multiple_returns` | `lsp_multiple_returns` | Focused CTests |
| --- | ---: | ---: | ---: | --- |
| PikaOS 4 x64 Debug | PASS, 10.92 s | PASS, 50.52 s | PASS, 0.58 s | 16/16, 112.96 s |
| PikaOS 4 x64 Release | PASS, 4.71 s | PASS, 12.18 s | PASS, 0.25 s | 16/16, 35.50 s |
| PikaOS 4 x64 strict ASan/UBSan with leaks | PASS, 13.59 s | PASS, 51.55 s | PASS, 1.00 s | 16/16, 123.91 s |
| Windows 10 x64 Debug | PASS, 19.40 s | PASS, 88.97 s | PASS, 1.36 s | 12/12, 218.37 s |
| Windows 10 x64 Release | PASS, 7.69 s | PASS, 22.26 s | PASS, 1.02 s | 12/12, 59.68 s |

The strict rerun reported no ASan, UBSan or leak findings. The Windows
reruns used Windows 10 Home x64 build 19045.6466. The disposable guest
workspace and transfer and scratch files were removed. Guest free space
rose from 7,084,748,800 to 13,770,194,944 bytes. Linux scratch filesystem
free space rose from 16,566,788,096 to 16,574,095,360 bytes.

### G15 changed-file inventory

- Compiler and typed IR — `include/void/frontend.hpp`, `include/void/ir.hpp`, `src/frontend/compiler.cpp`, `src/frontend/capabilities.inc`, `src/frontend/generic_declarations.inc`, `src/frontend/generic_tooling.inc`, `src/frontend/tooling.cpp`, `src/ir/module.cpp`, `src/lsp/main.cpp`.
- Verification — `CMakeLists.txt`, `editors/vscode/src/test/semanticHost.ts`, `tests/frontend/multiple_returns.py`, `tests/frontend/multiple_returns.void`, `tests/frontend/multiple_returns_helper.void`, `tests/frontend/tests.cpp`, `tests/lsp/multiple_returns.py`.
- Example — `examples/multiple_returns/test_game.void`.
- Documentation — `docs/LANGUAGE_BIBLE.md`, `docs/ADVANCED_LANGUAGE.md`, `docs/DOCUMENTATION_HOVER.md`, `docs/EDITOR_INTEGRATION.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`.

## G05 dependency intelligence and guarded maintenance

Baseline `1e988f00e41f3d3433859e756737acc25d3bf35c`. Inspection and
`resolve --locked` share the existing local graph capture and lock comparison.
The report projects one record per package and one record per declared
relationship from that capture. The selected root, manifest inputs and lock
are rechecked before the graph is released. JSON schema 1 preserves the
resolver's diagnostic on failure. [CLI contract](../CLI.md#inspect-project-dependencies)

The local fixtures compare file bytes, modes and modification times before
and after successful and failed inspection. The Git fixtures inspect exact
locked revisions and both parents of a shared transitive pin. The HTTPS
fixture checks the live server's request count and repeats inspection after
shutdown. Existing explicit fetch and transitive authorization controls
remain active. [Local fixtures](../../tests/project/dependency_explain.py) · [Git fixtures](../../tests/project/git_dependencies.py) · [HTTPS fixtures](../../tests/project/git_https.py)

The compiler attaches an exact primary-span replacement to the literal
obsolete loop diagnostic. The server validates the edit against the current
manifest project before offering it and again on invocation. The VS Code
command uses the current editor model version and rechecks the URI, project
root and connection generation. Import and alias diagnostics provide no
proven uniquely authorized replacement in this increment. [Protocol fixture](../../tests/lsp/quick_fixes.py) · [Workbench fixture](../../editors/vscode/src/test/quickFixHost.ts)

Inspection is limited to verified local source inputs. Dependency acquisition
and lock refresh remain explicit operations. Registry search, upgrades,
authenticated private Git, destructive cleanup and Vim/Neovim manifest-mode
parity stay outside the G05 implementation.

The first JSON build exposed an explicit LLVM string conversion requirement.
The output now uses the SDK's UTF-8 repair routine for diagnostic bytes.
A disposable POSIX path with an invalid byte verifies that the failed
inspection still returns parseable JSON. The first cycle fixture used a
disallowed `.` dependency path and correctly failed path validation. The
fixture now uses two mutually dependent packages. The first quick-fix
control was withheld by the ordinary reselection gate during failed analysis.
The final gate compares the validated repair against the existing selection.

### G05 focused native qualification

The tested source is the baseline above plus the working-tree changes listed
below. All profiles used LLVM/Clang/LLD 23.1.4 and the pinned provider
inputs. The Windows guest reported Windows 10 Home x64 build 19045.6466.
Its tests used a private root admitted by `void_windows_native_staging`.
The strict PikaOS build used `RelWithDebInfo` with `VOID_SANITIZERS=ON`.

| Host and profile | `project_dependency_explain` | `lsp_quick_fixes` | Focused CTests |
| --- | ---: | ---: | ---: |
| PikaOS 4 x64 Debug | 0.63 s | 0.29 s | 18/18, 113.41 s |
| PikaOS 4 x64 Release | 0.40 s | 0.19 s | 18/18, 40.84 s |
| PikaOS 4 x64 strict ASan/UBSan with leaks | 1.70 s | 0.51 s | 18/18, 132.37 s |
| Windows 10 x64 Debug | 2.39 s | 0.80 s | 17/17, 267.37 s |
| Windows 10 x64 Release | 1.94 s | 0.76 s | 17/17, 89.35 s |

The common selection is `project_dependency_explain`, `project_dependencies`,
`project_packages`, `project_git_dependencies`, `project_git_https`,
`project_sessions`, `project_protocol`, `project_profiles`, `lsp_quick_fixes`,
`lsp_editor_recovery`, `lsp_semantic_tokens`, `frontend`, `multiple_returns`,
`lsp_multiple_returns`, `net_sockets`, `net_sockets_lsp` and `process_capture`.
Linux also runs `project_managed_native_resolution`. Each selection has
active commands and no skip properties. The two new CTests each have a 90 s
limit. Existing tests retain their timeouts.

Replay from the repository root. On Windows, use the same anchored selector
without `project_managed_native_resolution` and pass the configured native
Debug or Release build directory to `ctest --test-dir`.

```sh
common='project_dependency_explain|project_dependencies|project_packages|project_git_dependencies|project_git_https|project_sessions|project_protocol|project_profiles|lsp_quick_fixes|lsp_editor_recovery|lsp_semantic_tokens|frontend|multiple_returns|lsp_multiple_returns|net_sockets|net_sockets_lsp|process_capture'
selection="^($common|project_managed_native_resolution)$"
ctest --test-dir build/llvm23-debug -j 2 --output-on-failure -R "$selection"
ctest --test-dir build/llvm23-release -j 2 --output-on-failure -R "$selection"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86 \
  ctest --test-dir build/llvm23-strict -j 2 --output-on-failure -R "$selection"
```

The strict run reported no ASan, UBSan or leak findings.

Native VS Code 1.141.0 with Node 24.19.0 and the Debug `voidls` passed the
guarded source correction on PikaOS and Windows. The workbench confirmed
application to the current unsaved buffer, rejection after a document
version change and project switch, and unchanged disk bytes. PikaOS also
passed the G14 workbench recovery suite. Replay from `editors/vscode` with
`VOIDLS_PATH` and `VSCODE_EXECUTABLE_PATH` set to the native executables.
Run `npm run build` then `node out/test/runSemanticHost.js --quick-fix`.
Use `--recovery` for the adjacent G14 suite.

The disposable Windows build, editor, provider and test workspace, source
transfers, and Linux scratch files were removed. Guest free space rose
from 5,734,891,520 to 13,771,386,880 bytes. Linux `/tmp` free space rose
from 16,197,185,536 to 16,570,511,360 bytes. The maintained Linux build
profiles and verified provider inputs remain available for replay.

### G05 changed-file inventory

- Compiler and CLI — `include/void/diagnostic.hpp`, `include/void/project_resolution.hpp`, `include/void/tooling.hpp`, `src/frontend/compiler.cpp`, `src/frontend/project_resolution.cpp`, `src/frontend/tooling.cpp`, `src/lsp/main.cpp`, `src/main.cpp`.
- VS Code — `editors/vscode/src/extension.ts`, `editors/vscode/src/test/quickFixHost.ts`, `editors/vscode/src/test/runSemanticHost.ts`.
- Verification — `CMakeLists.txt`, `tests/lsp/quick_fixes.py`, `tests/project/dependencies.py`, `tests/project/dependency_explain.py`, `tests/project/git_dependencies.py`, `tests/project/git_https.py`.
- Documentation — `docs/CLI.md`, `docs/EDITOR_INTEGRATION.md`, `docs/projects/PROJECT_DEPENDENCY_RESOLUTION_DESIGN.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`.

## G16 enum completion and semantic highlighting

Baseline `7af58a3ad4ebee4e0bc6c9a97c23f356ea2bcd4c`. The native LSP and
workbench reproduced the report against a disposable copy of the existing
TestGame project. The original tab-indented `main.void` had SHA-256
`3236545d6fcfa64e6ab0850ca60829e50e60e9c0df33f04212299371fdb705ca` before
and after qualification. Its saved entry was valid. The maintained fixture
uses the pasted source and a minimal `app.void` with the same property access.
[Maintained source](../../tests/lsp/fixtures/enum_recovery/main.void)

| Dirty TestGame snapshot | Baseline items | Baseline result |
| --- | --- | --- |
| `process_type.` in a statement arm, without its colon | Empty | `VOID1102`, no semantic tokens |
| `process_type.i` and `process_type.in`, without a colon | Empty | `VOID1101` and `VOID1102`, no semantic tokens |
| `process_type.` with an existing colon | `none`, `incorrect`, `unknown` | `VOID1102`, no semantic tokens |
| `var value = process_type.` | `none`, `incorrect`, `unknown` | `VOID1102`, no semantic tokens |
| `return process_type.` before the second G15 value was typed | Empty | `VOID1102`, no semantic tokens |
| Completed `process_type.incorrect:` | `incorrect` at the end of the name | Ready. `incorrect` had `enumMember`. The type qualifier had no token. |

The failing statement-arm cursor was byte 366, zero-based LSP line 21,
UTF-16 column 16 in TestGame. The pasted-space fixture reproduces at byte
408, line 21, column 25. On TestGame, the equivalent ordinary-expression
probe used byte 183, line 14, column 26. The return probe used byte 912,
line 41, column 21. Every dirty baseline row reported project state `error`.
The existing cursor probe reached enum lookup in value expressions and in
arms with a colon. Without the colon, parsing stopped before semantic
checking could reach the sentinel. In a return, G15's arity check stopped
the probe before it examined the first expression. G14 presentation had no
enum-hole recovery path. Baseline completion could also fall through a
rejected shadowing declaration to the outer enum.

The repair admits a bounded editor-only arm with a missing colon and checks
the written receiver against the actual match subject. A constructor pattern
uses the same enum candidates when its payload parentheses are already
present. A partially typed value or first return component can retain a
validated enum type in the current recovery snapshot. An unbound prefix
contributes no member reference or exhaustive case. Ordinary analysis owns
diagnostics, and the recovery analysis cannot lower to IR. The checker also
rejects enum candidates after an earlier error that makes lookup uncertain.

Tagged and numeric member expressions and payload patterns now retain their
written type span and optional module qualifier in the compiler index. Type
references have the `enum` role and no declaration modifiers. Member
references have `enumMember`, `readonly` and `static`. Declarations retain
their declaration and definition flags. The LSP legend is unchanged and the
extension consumes these roles through its existing client. [Protocol regression](../../tests/lsp/enum_recovery.py) · [Workbench regression](../../editors/vscode/src/test/enumRecoveryHost.ts)

During development, syntax-only recovery still dropped tokens for an
unknown member prefix with no parser error and for a return arity error.
Only compiler-marked enum prefixes and eligible first-component returns now
enter that recovery path. A rejected local declaration initially left no
binding and let completion reach the enum, which the earlier-error guard
now prevents. Fixture corrections included expecting the whole `none`
replacement range when querying the closed document's saved source.

### G16 workbench evidence

Native VS Code 1.141.0 with Node 24.19.0 ran the real extension against
the Debug server and an unchanged copy of TestGame. On PikaOS the baseline
dot keystroke produced no popup or enum items. The corrected workbench
opened the popup on the actual `.` keystroke and showed all three members.
It narrowed the popup for `i` and `in` and restored the completed member
token after `incorrect:` cleared the diagnostic. The workbench verified
that the source on disk retained its original bytes. The adjacent G14
recovery and numeric-enum workbench suites also passed on PikaOS.

The Windows 10 workbench passed the same keystroke, prefix and token checks
with the copied TestGame source. The extension host briefly reported that it
was unresponsive during startup, recovered in the same session, and exited
with code 0. Its stock Dark 2026 profile rendered distinct type and member
colors.

The isolated Linux profile was checked with the stock Dark 2026 theme and
with the Dark+ theme configured in Boss's VS Code user profile. The Dark+
baseline had no type token and rendered the type and member as
`rgb(102, 136, 204)`. The corrected Dark+ replay returned `enum` and
`enumMember`. The rendered type was `rgb(255, 238, 187)` and the member
stayed `rgb(102, 136, 204)`. These are observations of the theme. The compiler
and extension have no color rules for this feature.

From `editors/vscode`, run `npm run build` then
`node out/test/runSemanticHost.js --enum-recovery` with `VOIDLS_PATH` and
`VSCODE_EXECUTABLE_PATH` set to native executables and `VOID_TEST_CDP_PORT`
set to an unused loopback port. The default fixture is the maintained source.
`VOID_ENUM_FIXTURE` can point to a disposable copy of TestGame. For a baseline
replay, point to the baseline server and set `VOID_ENUM_BASELINE=1`.

### G16 focused native qualification

The selection includes the new LSP regression, existing G14 recovery,
semantic tokens and Stage 10 intelligence, G15 execution and LSP, numeric
enum IR/process/LSP, pattern source and CLI coverage, frontend diagnostics,
project selection and project sessions. The pattern CLI test uses the
maintained prelinked normal and O2 targets. The regression also checks
non-BMP UTF-16 edit ranges, dirty buffer revisions, closure, project
switching, visibility, missing imports, and ordinary CLI rejection of
incomplete or invalid source. Payload signatures describe their declared
arguments. Variant-level visibility modifiers remain unsupported by the
existing language, so the visibility negative uses a private imported enum.
Recovery depends on a visible enum and a current, resolvable subject for
match arms. Malformed payload patterns and invalid imports can still
withhold assistance until the source is repaired.

A disposable legacy LSP control also recovered the incomplete TestGame arm,
withheld formatting and rename, and withheld tokens after the recovery budget
was exhausted. `console.color.green` carried `defaultLibrary` on both the
type and member, with `readonly` and `static` on the member. The first control
used `color` for a local and triggered the existing shadowing diagnostic.
Renaming that fixture local to `shade` exercised the intended token path.

| Host and profile | `lsp_enum_recovery` | Focused CTests |
| --- | ---: | ---: |
| PikaOS 4 x64 Debug | 19.10 s | 14/14, 97.30 s |
| PikaOS 4 x64 Release | 3.62 s | 14/14, 23.37 s |
| PikaOS 4 x64 strict ASan/UBSan with leaks | 14.99 s | 14/14, 75.56 s |
| Windows 10 x64 Debug | 31.72 s | 14/14, 201.99 s |
| Windows 10 x64 Release | 7.83 s | 14/14, 56.37 s |

The separate PikaOS Debug preservation selection passed `net_sockets`,
`net_sockets_lsp` and `process_capture` in 69.56 s. All qualification builds
used LLVM/Clang/LLD 23.1.4 and verified provider inputs. Windows reported
Windows 10 Home x64 build 19045.6466. The test driver admitted a private
temporary root with `void_windows_native_staging` for each Windows profile.

```sh
selection='^(lsp_enum_recovery|lsp_editor_recovery|lsp_semantic_tokens|lsp_stage10_intelligence|multiple_returns|lsp_multiple_returns|patterns_source|patterns_cli|numeric_enum_ir|numeric_enum_process|numeric_enum_lsp|project_selection|project_sessions|frontend)$'
ctest --test-dir BUILD -j 2 --output-on-failure -R "$selection"
```

All five configurations registered the exact same 14 tests with active
commands and no skip properties. The new `lsp_enum_recovery` registration
has a 180 s timeout. Existing registrations and timeouts are preserved.
The strict Linux profile is
`RelWithDebInfo` with `VOID_SANITIZERS=ON` and uses
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`. It reported
no ASan, UBSan or leak findings.

The disposable guest workspace, source transfers, workbench profiles and
scratch logs were removed. Guest free space rose from 5,565,751,296 to
15,006,392,320 bytes. Linux `/tmp` free space rose from 16,041,676,800 to
16,632,401,920 bytes. The maintained Linux build profiles retain their
verified inputs and prelinked regression targets for replay.

### G16 changed-file inventory

- Compiler — `include/void/diagnostic.hpp`, `src/frontend/compiler.cpp`, `src/frontend/pattern_parser.inc`, `src/frontend/pattern_semantics.inc`, `src/frontend/tooling.cpp`.
- Qualification — `CMakeLists.txt`, `editors/vscode/src/test/enumRecoveryHost.ts`, `editors/vscode/src/test/runSemanticHost.ts`, `tests/lsp/enum_recovery.py`, `tests/lsp/fixtures/enum_recovery/app.void`, `tests/lsp/fixtures/enum_recovery/main.void`, `tests/lsp/fixtures/enum_recovery/void.project.toml`.
- Documentation — `docs/ADVANCED_LANGUAGE.md`, `docs/EDITOR_INTEGRATION.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`.

## G06 atomic application saves

Baseline `6b28c57ff50256734fb804387f7c0ffca828d51f`. Before changing the
Runtime, the normal and O2 JIT baseline saved a small UTF-8 game-progress
record using ordinary `io.write_text`. The native fixture limited each write
to three bytes and then failed after eight bytes. The returned error reported
eight bytes transferred. Reopening the destination yielded `level=12`, which
had replaced the previous 19-byte record. The unmodified ordinary save also
loaded and verified its full output when the fault was removed.
[Ordinary save fixture](../../tests/io/ordinary_save.void) · [Fault driver](../../tests/io/atomic_save.cpp)

The existing private-directory API can exclusively create a held file,
acquire a native writer lock, and publish the file after synchronization.
Its file publication checks the source identity. The available public
removal is path-based, and the existing publication operation does not pin
and verify the destination against the save's initial observation. A pure
source wrapper could therefore neither reclaim an abandoned stage with held
directory authority nor enforce the replacement contract. The bounded
implementation adds one private synchronous host operation and two public
methods on the existing capability. It reuses the ordinary transfer loop,
private directory admission, native lock, native rename and Runtime ledger.
[Public signatures and outcomes](../io/API.md#atomic-application-saves) · [Full contract](../io/CONTRACT.md#atomic-application-saves)

The stage is exclusive, capped at 16 MiB, and always in the held directory.
An existing destination is inspected again before replacement. An initially
absent destination uses native no-replace rename. The result sets `published`
immediately after rename, then records directory synchronization and cleanup
independently. The host retires native handles and removes only the checked
uncommitted stage. If cleanup fails, at most one reserved stage remains and
future saves refuse to adopt it. Async save remains deferred until a native
commit boundary can report the outcome even when the owning task cancels.

The executable example writes and replaces a UTF-8 configuration, refuses an
exclusive collision and loads a binary checkpoint using the existing numeric
codec. The first independent Python oracle expected `00 00 00 ff` for
`4278190208`. The application and a direct byte read yielded the correct
little-endian `80 00 00 ff`. The oracle was corrected. Initial source probes
also caught fixture locals that shadowed `os.args` and `path.root`. The
examples use distinct local names.
[Executable TestGame example](../../examples/atomic_save/test_game.void) · [Independent JIT/AOT fixture](../../tests/io/atomic_save.py)

The first Windows Debug native run reported a handle-count increase from
117 to 118. A disposable handle snapshot identified the added handle as
`KernelBase.dll.mui`. It appeared on the first missing-destination error
lookup and stayed constant through the fault cases and repeated writes. The
stage was absent. The native fixture now initializes Windows error messages
before taking its baseline and still requires the exact same handle count
after repeated saves. The first Windows independent AOT link exited zero
but the MSVC-target driver warned that `--ld-path` was unused. A single
`--ld-path` produced the same warning on replay. The fixture now selects
LLD with `-B` and `-fuse-ld=lld`, and verifies that `-###` selects the exact
SDK `lld-link` executable. The first launch-plan check expected the
`.exe` suffix and failed because Clang omits it. The check now accepts
either spelling, resolves the same SDK executable, and checks its version.
A separate default-driver control also selected the SDK's LLD for the
existing Windows build tests.

Windows file-symlink creation returned error 1314 in the native control.
The save fixture exercised refusal of a real NTFS junction in that guest.
The existing `io_execution` and `io_async_execution` fixtures both reached
their mandatory symlink creation check and returned check 91. Their
assertions remain in place. Linux qualified the file-symlink rejection.

### G06 native qualification

The native save fixture checks the exact old bytes after each injected
pre-publication error. It separately checks committed bytes and flags after
injected directory-sync and close errors. Other controls cover empty and
maximum payloads, oversize admission, permission and hard-link refusal,
exclusive collisions, parent identity changes, native reader behavior, lock
contention, handle counts and managed ledger retirement. The separate source
fixture verifies UTF-8 and binary data in normal/O2 JIT and independently
linked normal/O2 AOT. It also exercises the public byte snapshot and text
encoding limits before mutation. [Native fixture](../../tests/io/atomic_save.cpp) · [Source fixture](../../tests/io/atomic_save.py)

PikaOS 4 x64 qualified tmpfs and an additional Debug replay on local XFS.
An open Linux reader retained the original file and new opens saw only
complete old or new payloads. Windows 10 Home x64 build 19045.6466 used a
verified private NTFS test root. NTFS returned code 5 before replacement
with either a reader that denied delete sharing or a reader that granted it.
Uncontended saves reported both native synchronization barriers complete.
The ordinary cross-volume move test used XFS and tmpfs on Linux and the
guest's NTFS volume and a disposable VBox shared filesystem on Windows.
Windows file-symlink creation remains unqualified in this guest. The junction
refusal and ordinary cross-volume move results apply to the tested providers.

| Host and profile | Focused CTests | Initial selection wall time | G06 native / source on final fixtures |
| --- | ---: | ---: | --- |
| PikaOS 4 x64 Debug | 26/26 | 383.43 s | 31.64 s / 29.17 s |
| PikaOS 4 x64 Release | 26/26 | 122.17 s | 12.39 s / 9.51 s |
| PikaOS 4 x64 strict ASan/UBSan with leaks | 26/26 | 334.22 s | 31.59 s / 28.82 s |
| Windows 10 Home x64 Debug | 23/25 after targeted replays | 531.35 s | 65.54 s / 54.99 s |
| Windows 10 Home x64 Release | 23/25 | 145.54 s | 17.85 s / 14.65 s |

All profiles used LLVM/Clang/LLD 23.1.4 and verified provider inputs. The
strict Linux profile used `RelWithDebInfo`, `VOID_SANITIZERS=ON`,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.
The original strict selection and final G06 replays reported zero sanitizer
findings. The original Windows Debug selection passed 21/25. The two G06
fixtures passed on targeted replays. The Windows Release selection passed
both G06 fixtures on its first run. The two existing I/O suites retained
the file-symlink prerequisite failure in both Windows profiles.

```sh
common='io_atomic_save_native|io_atomic_save_source|io_ownership|io_execution|io_async_execution|io_native_close_failure|io_native_shutdown|io_native_opening_shutdown|io_native_pending_close|io_native_cancelled|io_kernel|io_cross_volume|io_native_jobs|host_capabilities_portable|host_capabilities_portable_source|lsp_io|lsp_enum_recovery|multiple_returns|lsp_multiple_returns|project_dependency_explain|project_profiles|project_application|net_sockets|net_sockets_lsp|process_capture'
ctest --test-dir BUILD -j 2 --output-on-failure -R "^($common|host_capabilities_native)$"
```

On Windows, run `ctest --test-dir BUILD -j 2 --output-on-failure -R
"^($common)$"` with the same test names and a private native temporary root.
Set `VOID_IO_SECOND_VOLUME` to a distinct filesystem for `io_cross_volume`.
The maintained `io_execution` and `io_async_execution` suites require a
Windows session that can create file symlinks. The new save tests each use
180 s. The CTest registrations had active commands and the same timeouts
for all common tests across the five profiles. The existing cross-volume
skip condition stayed in place and the test executed on each profile.

To run the example, pass the absolute path of a new private directory
beneath an admitted parent.

```sh
voidc run examples/atomic_save/test_game.void --no-cache -- "$PRIVATE_PARENT/new-save"
```

Removed the disposable Windows workspace, Linux scratch roots and host
share after qualification. Available guest space rose from 7,295,012,864
to 15,007,399,936 bytes. Host tmpfs space rose from 16,417,361,920 to
16,622,710,784 bytes, and host XFS space rose from 367,337,570,304 to
367,337,689,088 bytes. The active Linux builds and pinned inputs remain in
their existing locations. This record retains the failure observations and
replay commands.

### G06 changed-file inventory

- Runtime and ABI — `include/void/detail/filesystem_hooks.hpp`, `include/void/filesystem.hpp`, `include/void/host_ops.hpp`, `src/runtime/filesystem.cpp`, `src/runtime/host_abi.cpp`, `src/runtime/host_directory.cpp`, `src/runtime/host_directory_windows.cpp`, `src/runtime/host_internal.hpp`, `src/runtime/host_save.hpp`, `stdlib/io.void`.
- Verification and example — `CMakeLists.txt`, `examples/atomic_save/test_game.void`, `tests/io/atomic_save.cpp`, `tests/io/atomic_save.py`, `tests/io/atomic_save_fault.void`, `tests/io/atomic_save_limits.void`, `tests/io/ordinary_save.void`.
- Documentation — `docs/io/API.md`, `docs/io/CONTRACT.md`, `docs/roadmap/FEATURE_COMPLETENESS_AND_DEVELOPER_EXPERIENCE.md`, `stdlib/README.md`.
