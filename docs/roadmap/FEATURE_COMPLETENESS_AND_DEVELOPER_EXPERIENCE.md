# VOID.Language feature completeness and developer experience

**Milestone 1 · Source audit and roadmap · Golden Wrench review**

Baseline `4d39a2b121b8ba6fb6cfa3b60d02d8e048b27b59` on `main`. The audit used tracked source, maintained test registrations and recorded results at their stated revisions. LLVM/Clang/LLD **23.1.4** remains the active toolchain. This roadmap proposes work for Boss's subsequent authorization.

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
| Functions, calls and initialization | **IMPLEMENTED.** Ordinary and receiver calls use an ordered positional prefix followed by named arguments. Pure defaults and concrete default-based parameter inference work within the current declaration rules. Explicit value/managed `init`, named-field construction, function values and closures work. **MISSING** multiple-return/destructuring syntax. **DEFERRED** fallible or overloaded constructors. Use a returned value struct or a result-returning factory with today's contracts. | [Parameter parser](../../src/frontend/compiler.cpp#L803), [constructor binding](../../src/frontend/compiler.cpp#L2223), [initialization regression](../../tests/frontend/initialization.cpp), [function defaults and inference](../STAGE8_CALLS_NUMERIC.md), [advanced language](../ADVANCED_LANGUAGE.md) |
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
| [archive](../../stdlib/archive.void) | **PARTIAL.** Bounded read-only ZIP/ZIP64 stored/deflate stream with entry, expansion, CRC and close checks. Caller controls extraction. | No ZIP writer or general unpack command. [Native archive](../../src/runtime/host_archive.cpp) and [host capability fixtures](../../tests/host_capabilities). G12. |
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
| [io](../../stdlib/io.void) | **IMPLEMENTED.** Bounded whole-file text/bytes, file handles, cursor and numeric/Pascal wire methods, directory operations, links, sync/async ordinary file operations and private-directory publication primitives. | **PARTIAL** for ordinary recoverable atomic-save ergonomics. General writes/copies may leave progress, and private publication has a stricter ownership contract. [Filesystem Runtime](../../src/runtime/filesystem.cpp), [host Windows publication](../../src/runtime/host_directory_windows.cpp), [I/O contract](../io/CONTRACT.md), [I/O tests](../../tests/io). G06. |
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
| [time](../../stdlib/time.void) | **IMPLEMENTED.** Gregorian construction, instants/durations, offsets and IANA zones, ambiguity handling, checked arithmetic, formatting and UTC/monotonic clocks. | **PARTIAL** for parsing and scheduling convenience. No general inverse formatted-date parser or public timer task. [Temporal core](../../src/temporal.cpp), [ABI](../../src/runtime/temporal_abi.cpp), [temporal tests](../../tests/stage10), [installed API guide](../../stdlib/README.md). G09/G10. |
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
| 7. Integrate native libraries | **IMPLEMENTED** manual native binding and verified D4 catalogs for the admitted Linux/Windows lanes, including approval, lock, provider and adapter checks. | The managed catalog selects one local provider with exact target/version/ABI. macOS managed-native execution is unsupported. Windows managed retained callbacks and managed raylib packaging lack equivalent qualification. [Catalog code](../../src/frontend/project_native.cpp), [D4](../projects/DEPENDENCY_D4_CLOSEOUT.md), [project reconciliation](../projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md). G11. |
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

### P1 — G03 · Debug information and source debugging

**PARTIAL** emission, **MISSING** a supported source debugging workflow. Explicit `line` and `full` requests only warn today. Define the concrete object/executable symbol contract for the first native AOT lane and decide which requests remain unsupported for JIT. Carry the approved mode through provenance and compilation, preserve current implicit quiet defaults and native resource admission, and use existing platform debugger interfaces for launch/attach. Verify emitted sections and linked symbols with the pinned SDK, then demonstrate breakpoints, stepping, a representative local value, and attach/normal termination on the claimed Linux/Windows hosts. Treat optimized-out values and relocated paths as explicit checks. [Existing profile fixture](../../tests/project/profiles.py) · [LLVM metadata](../../src/backend/llvm/backend.cpp) · [IDE/debugger proposal](../PROJECT_ARCHITECTURE_ROADMAP.md#proposed-ide-and-public-usability-milestones)

### P1 — G04 · Bounded child output and wait

**IMPLEMENTED.** `process.capture` and `process.capture_async` collect bounded
binary prefixes from selected output streams with independent EOF flags and
direct-child status. They share native launch and resource handling with the
existing inherited-stream workflow. The async operation uses checked native
completion and structured cancellation. [Public process API](../process/API.md) · [G04 implementation and qualification](#g04-bounded-subprocess-capture-deadlines-and-cancellation)

### P2 — G05 · Maintenance commands and safer editor fixes

**PARTIAL.** Developers can edit pins, fetch and refresh, but have no dedicated read-only graph/change explanation. Vim/Neovim adapters still use legacy selection. Code actions and wider rename lack guarded contracts. Start with a read-only lock/provenance explanation and bounded diagnostic actions with version checks. Review manifest-mode adapters separately using the existing schema and session guard. Keep registry search, private Git credentials and destructive package/cache management outside this increment. Verify offline graphs, stale lock refusal, no writes/network from inspect, rejected stale edits and actual editor switches. [Resolver](../../src/frontend/project_resolution.cpp) · [Project package tests](../../tests/project/packages.py) · [Language server](../../src/lsp/main.cpp) · [Editor fixtures](../../tests/editors)

### P2 — G06 · Application save/replace workflow

**PARTIAL.** Ordinary file writes can expose partial data on failure. Private-directory publication already supplies narrower capability-based primitives. Measure a real save/config example before choosing a documented helper or new public API. Reuse held-file identity, same-filesystem staging and explicit publication/durability outcomes. Verify old content on pre-publication failure, collision and alias rejection, reported post-publication failure, and concurrent readers on each claimed filesystem/host. [I/O API](../io/API.md) · [Existing publish implementation](../../src/runtime/host_directory.cpp) · [Windows publication](../../src/runtime/host_directory_windows.cpp) · [Filesystem tests](../../tests/io)

### P2 — G07 · TCP/UDP and server applications

**MISSING.** HTTP clients and URL helpers do not cover local listeners, datagrams or standalone name resolution. The proposed socket work is important for a service or game networking lane. Review endpoints, ownership, bounded queues and cancellation before adding public facades. Reuse external completion and provider initialization. Verify loopback client/listener and UDP, refusal, IPv4/IPv6 capability, bounded transfers, cancellation and shutdown using real Linux and Windows sockets. [Architecture proposal](../net/TCP_UDP_HANDOFF.md) · [External completion](../../src/runtime/async.cpp) · [Existing HTTP provider](../../src/runtime/http_provider.cpp)

### P2 — G08 · Measurable edit/build performance and Windows cache

**PARTIAL.** Cache policy and prior compiler improvements already exist. Full graph work affects edit/check time, and Windows intentionally falls back without native persistent disk caching. Measure edit latency and cold/warm check/JIT/AOT on one frozen representative project per host. Use measured hotspots to choose bounded reuse. Treat Windows persistence as a separate owner/private-file and publish-integrity design. Verify hits/misses, revision invalidation, malformed artifacts, root identity and unchanged output. Keep the historical FILE200 target advisory and retain unfavorable results. [Tooling analysis](../../src/frontend/tooling.cpp) · [Cache implementation](../../src/backend/llvm/native_cache.cpp) · [Cache tests](../../tests/cache) · [Performance record](../compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md)

### P2 — G09 · Data and text conveniences backed by application examples

**PARTIAL.** File and memory wire data and JSON DTOs work. Large incremental JSON input, optional/default-field policies, formatted-date parsing, and normalization or grapheme-level editing are separate gaps. Collection callback type arguments can be awkward in generic code. Choose only the operation demonstrated by an application, such as a bounded configuration reader with precise field errors. Reuse the existing JSON/resource counters, scalar offsets, strict wire codecs and typed diagnostics. Verify malformed UTF-8, boundaries and round-trip behavior with the existing text/JSON/time suites. Preserve the pinned Unicode and tzdata identities. [JSON API](../json/API.md) · [Collections API](../collections/API.md) · [Text/encoding API](../text-encoding/API.md) · [Temporal implementation](../../src/temporal.cpp)

### P2 — G10 · Async deadlines and coordination convenience

**PARTIAL.** Task groups, cancellation and provider-specific deadlines are usable. General timer/deadline composition and an async queue with backpressure need a source contract. Consider only after a concrete workflow needs them. Reuse current scheduler, scope cancellation/drain and monotonic clocks. Verify completion order, cancellation before/after work admission, scope exit and retained roots, including shutdown under concurrent I/O. [Async contract](../ASYNC.md) · [Runtime scheduler](../../src/runtime/async.cpp) · [Lifecycle tests](../../tests/async)

### P2 — G11 · Native platform extensions and qualification

**DEFERRED/UNVERIFIED.** macOS managed-native and D5 need a separately reviewed loader/package lane if selected. Mach-O identity, loader resolution, bundle paths and signing policy are part of that scope. Windows managed raylib packaging and retained managed callbacks need direct qualification before any feature expansion. Reuse the recorded Windows synthetic and conventional results, then qualify real managed provider inputs, relocation and callback drain on the claimed host. Static raylib is optional. Signed App Sandbox remains unverified. [Platform and gate reconciliation](../projects/PROJECT_APPLICATION_ACCEPTANCE_RECONCILIATION.md) · [Native package tests](../../tests/project/managed_native.py)

### P3 — G12 · Optional language, FFI, crypto and archive extensions

**DEFERRED/MISSING.** Advanced source constructs, generalized FFI/reload, broader crypto and archive writing require a demonstrated use case and reviewed observable semantics. Existing applications can use explicit factories, typed C descriptors, owned handles and read-only ZIP. New operators require consistent constant evaluation, typed IR validation and LLVM lowering, including edge-value tests. Any approved crypto addition needs provider vectors and misuse tests. An archive writer needs round-trip/CRC, path and resource-limit negatives. New native signatures or reload scope need ABI and owner/generation lifetime negatives in JIT and independently linked AOT where applicable. [Language boundaries](../LANGUAGE_BIBLE.md) · [Native bridge](../../include/void/c_primitive.hpp) · [Host tests](../../tests/host_capabilities) · [FFI controls](../../tests/ffi)

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
