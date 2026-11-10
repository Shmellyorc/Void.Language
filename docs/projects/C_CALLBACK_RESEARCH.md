# C-to-VOID callbacks: practical engineering recommendation

Research handoff, 9 November 2026. Linux x64 SysV and Windows x64 MSVC;
LLVM/Clang/LLD 23.1.4. Source/documentation inspection only: no implementation,
benchmark, native qualification, Git mutation or DLL-loader reinvestigation.

**Recommendation: GO with target-compiled C callback adapters, reusing VOID's
existing callback admission, roots, resource lifecycle and generation leases.**
Do not add libffi or hand-written executable trampolines for the current scope.
This supports Keyboard's ongoing implementation; it is not another research gate.

## Existing machinery to reuse

| Source inspected | Reusable behavior / limitation |
| --- | --- |
| `include/void/callback.hpp:6–45`, `src/runtime/callback.cpp:13–33` | Provider quiescence explicitly covers callbacks dispatched **before admission**. Admission attaches the thread, enters Managed, contains exceptions, counts active calls and rejects cross-Runtime entry. Close rejects self-drain and waits outside Managed. |
| `src/runtime/native_binding.cpp:40–99` | Existing fixture captures a `Runtime::Handle`, checked entry/function identity and `NativeCode::active_lease()`. Invocation installs `LeaseScope`; cleanup releases captures after unregister/drain. This is an implementation pattern, not general callback support by itself. |
| `src/runtime/coordination.cpp:64–105`, `include/void/execution.hpp:50–66` | Foreign-thread attachment/GC rendezvous and generation lease scope already exist. Neither grants access to owner-thread-only objects. |
| `src/backend/llvm/backend.cpp`, `native.callback` generation near line 1533 | Existing generated entry reconstructs a VOID callable and invokes its checked dispatcher; C does not receive the language closure representation directly. |
| `src/ir/c_primitive.cpp`, adapter generation near line 440 | Header/type/layout checks and target C compilation already exist. During this inspection Keyboard's working tree also acquired `vd_callback_context` / `vd_enter_*` scalar callback generation. This is ongoing, **not independently tested or accepted here**. |
| `tests/native/tests.cpp:35–67`, `tests/native/integrated.cpp:88–127` | Existing fixtures exercise foreign-thread GC, pre-admission shutdown, allocation failure, self-drain, ownership rejection and old-generation retention. Extend these patterns into general FFI tests. |

Line numbers describe the inspected working tree and may move during engineering.
The conventional-FFI report still says “no native callbacks”; it describes the
previous acceptance boundary, not proof that the live new code is absent.

## ABI and function-pointer architecture

1. **Emit a real C function with the vendor's exact callback prototype.** Compile
   against the approved header using the selected target, packing and ABI flags.
   Extend existing descriptor/header checks to callback typedefs, scalar widths,
   parameter order and return type. Convert into the checked internal VOID bridge
   using explicit scalar slots or typed storage; retain existing bit-preserving
   copies. Do not cast a VOID callable, hidden environment or checked-failure
   signature into a vendor function pointer.
2. **Keep native argument classification in Clang.** Windows register/shadow-space,
   aggregate returns and Linux SysV classification differ. LLVM's calling-convention
   identifier alone does not perform Clang's C type classification. The pinned
   implementation has separate `X86_64ABIInfo::computeInfo` and
   `WinX86_64ABIInfo::computeInfo`, with separate `EmitVAArg` paths. Compile for
   `x86_64-*-linux-gnu` or `x86_64-pc-windows-msvc`; do not substitute an `ms_abi`
   attribute on Linux for native Windows qualification.
   [Clang 23.1.4 classifier](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.4/clang/lib/CodeGen/Targets/X86.cpp#L3027),
   [Clang attributes](https://clang.llvm.org/docs/AttributeReference.html#ms-abi),
   [Microsoft x64 ABI](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention).
3. **One typed C trampoline per declared signature is sufficient when the API
   returns userdata.** Give C a stable native registration context, not an
   unrooted managed address. The context owns the captured environment handle,
   exact checked entry, generation lease, provider/adapter lifetime and failure
   state. Finish initialization before registration: some libraries invoke inline.
4. **Use the same adapter in JIT and linked AOT.** In JIT, supply the materialized
   checked entry and hold its original generation throughout registration and
   drain. In AOT, supply the linked entry with the same internal ABI. Exporting a
   DLL symbol is unnecessary merely to pass its address. Do not serialize callback
   addresses or live contexts into caches; reconstruct them after loading.
   ORC supports code removal, so an obtained address is not a lifetime guarantee.
   [LLVM calling conventions](https://llvm.org/docs/LangRef.html#calling-conventions),
   [ORC resource removal](https://llvm.org/docs/ORCv2.html#how-to-remove-code).

For APIs **without userdata**, distinguish a genuinely global callback slot
(raylib) from many independently retained callback pointers. The former permits
one explicitly owned native adapter slot. The latter needs distinct generated
thunks/slots or a closure facility; do not silently route every registration to
the latest capture. Do not generalize raylib's singleton constraint to all FFI.

## Lifetime, threads and failure rules

Reuse this order: create rooted/leased context → register → admit/invoke → stop
new dispatch and drain the provider → drain admitted callbacks → release capture,
code and library ownership. Registration failure must undo any partially installed
callback before freeing its context.

The provider barrier is indispensable: the C trampoline dereferences its context
before `CallbackRegistration::run` can count it. A counter inside the callback
cannot protect a thread paused immediately before that first dereference.
Unregister must establish “no future entry and all prior dispatches returned,”
either directly or through documented producer stop/join. Never invent that
guarantee from a setter returning successfully.

On foreign threads, use existing admission/Attachment/Managed/LeaseScope. An
outbound operation that can callback must already be NativeSafe. Preserve current
owner-thread checks; reject disallowed access rather than silently relocating
execution. Concurrent callbacks require an explicit capture-access policy; precise
GC roots do not make mutable captures race-free. Initial owner-thread-only APIs
can reject foreign delivery; APIs promising foreign-thread callbacks must qualify
their permitted capture/effect subset. Do not create an implicit scheduler.

Contain checked failures and C++ exceptions at the bridge. Every non-void callback
needs a library-defined fallback result; numeric zero is not universally failure.
For void/retained callbacks, retain an owned, bounded failure for the registration
owner to observe. Do not use a caller's temporary failure pointer after return or
overwrite concurrent failures without a documented policy. Reject self-unregister
that would wait for itself. Do not hold provider locks while calling managed code.
Signals and hard-real-time callbacks are not qualified by ordinary thread entry.

## Established implementations: what to borrow

| Project | Concrete reusable pattern and limit |
| --- | --- |
| Go | `runtime/cgo.Handle` roots a language value behind an opaque handle; delete only after C stops using it. `runtime/cgocall.go` separates C ABI entry from Runtime admission before allocation/callback execution. Reuse the principle through VOID's existing Handle/Attachment, not Go's scheduler or stack switching. [Handle implementation](https://github.com/golang/go/blob/master/src/runtime/cgo/handle.go), [callback transition](https://github.com/golang/go/blob/master/src/runtime/cgocall.go#L34). |
| Rust | Typed `extern "C"` entry plus userdata; foreign-thread synchronization and a no-more-callbacks unregister guarantee are explicit binding responsibilities. No unwinding through ordinary C boundaries. [Rust FFI guide](https://doc.rust-lang.org/nomicon/ffi.html#callbacks-from-c-code-to-rust-functions). |
| Odin | Explicit `proc "c"` and explicit language context when entering from C. Its raylib binding declares a C callback type. **`contextless` is not synonymous with C ABI.** Borrow explicit ABI/context separation, not its target-sensitive `va_list` spelling. [Calling conventions](https://odin-lang.org/docs/overview/#calling-conventions), [raylib binding](https://github.com/odin-lang/Odin/blob/master/vendor/raylib/raylib.odin#L924) (current development binding, not a 6.0 qualification). |
| Zig | C-compatible function pointers/`callconv(.c)` and C imports keep native signatures explicit. These do not supply captured-state ownership automatically. [Language reference](https://ziglang.org/documentation/master/#Functions), [function-pointer tests](https://github.com/ziglang/zig/blob/master/test/behavior/fn.zig). |
| libffi | `ffi_closure_alloc` + `ffi_prep_closure_loc` provide ABI-specific callable closures with userdata. CIF/type metadata must outlive the closure; freeing requires no running or possible entrants. Unix64 and Win64 have distinct implementations. Valuable if truly runtime-defined signatures or many no-userdata captures become required; currently duplicates existing target adapter work and introduces executable-memory/dependency costs. [Closure contract](https://github.com/libffi/libffi/blob/master/doc/libffi.texi#L970), [Unix64](https://github.com/libffi/libffi/blob/master/src/x86/ffi64.c), [Win64](https://github.com/libffi/libffi/blob/master/src/x86/ffiw64.c). |

Upstream default-branch examples establish patterns, not version-pinned acceptance
for VOID. No new upstream dependency is recommended.

## Raylib 6.0: useful integration test, with explicit limits

The exact typedef is `void (*TraceLogCallback)(int, const char*, va_list)`.
This is a **fixed-prototype callback carrying `va_list`**, not a variadic callback.
Keep it in target-compiled C; normalize it into level + owned text before VOID.
Use `va_copy`/`va_end` and bounded `vsnprintf`, handle negative/truncated results,
and declare the text decoding policy. Never retain `va_list` or borrowed format/
argument pointers after return. Do not expose native format strings as an
untrusted printf entry point. [6.0 header](https://github.com/raysan5/raylib/blob/6.0/src/raylib.h#L966).

Raylib 6.0 `rcore.c:403,1878–1935` uses a plain global callback pointer; its setter
only assigns. It supplies neither userdata nor a concurrency/drain guarantee.
Install/change/remove only when all relevant producers are quiescent. One owned
registration per library instance is appropriate. The callback branch returns
before the normal `LOG_FATAL` exit: do not claim custom logging preserves that
exit automatically. [Exact 6.0 implementation](https://github.com/raysan5/raylib/blob/6.0/src/rcore.c#L1878).

The official example demonstrates C-side formatting. Raylib-go likewise formats
in C then copies into Go, but its inspected `utils_log.c` uses unbounded
`vsprintf` into 256 bytes: **borrow the boundary placement, not that unsafe
buffer handling or its unguarded global ownership**.
[Official example](https://github.com/raysan5/raylib/blob/6.0/examples/core/core_custom_logging.c),
[raylib-go C bridge](https://github.com/gen2brain/raylib-go/blob/master/raylib/utils_log.c),
[Go entry](https://github.com/gen2brain/raylib-go/blob/master/raylib/utils_log.go).

Use a headless test that registers, emits known integer/float/string messages,
checks the captured text, and unregisters with no concurrent producers. Raylib's
inspected TraceLog path does not require window creation. Use a fixed native
test helper to emit formatted messages; public VOID variadics are unnecessary.
This proves a real callback ABI and text adapter, not general concurrent drain.
Synthetic callbacks remain the right way to force that race reproducibly.

## One focused engineering assignment

Extend existing descriptor → C adapter → checked callback entry → Runtime
registration, together with lifecycle tests. Keep provider-specific formatting
and unregister policy in the binding adapter. Do not add a second callback engine.

Acceptance should cover:

- Actual Linux SysV and Windows MSVC normal/O2 JIT and independently linked AOT;
  C-originated mixed integer/floating calls exceeding register arguments, narrow
  signed/unsigned values, return values, captured closures and whatever records
  the admitted callback contract supports. Unsupported shapes diagnose early.
- Typed indirect calls under existing strict sanitizers; generated callback
  metadata must match the actual bridge signature, not reuse the fixture hash
  blindly. Real Windows execution validates its ABI, not a Linux cross-build.
- Synchronous/reentrant and foreign-thread entry; GC during invocation; capture
  and Runtime shutdown; pre-admission pause; double close and self-close;
  allocation/registration failure and callback error reporting; concurrent calls
  only where declared safe. Keep native stack use bounded and existing 1 MiB tests.
- Reload while a callback remains registered: old code stays leased until drain;
  later registrations use new code; caches reconstruct fresh registrations;
  adapter/provider unload cannot precede the last relevant callback.
- Raylib 6.0 formatted headless trace test on both native targets, including
  long/truncated and invalid-text handling under the chosen binding policy.

**Risks requiring explicit binding choices:** callback retention duration,
userdata position/absence, thread/reentrancy policy, borrowed argument extent,
failure return and observable retained failures, provider quiescence guarantee.
These are contract fields/adapter responsibilities, not reasons to postpone the
already-supported scalar+userdata implementation. No new native tests were run
by this research; portability and performance remain implementation evidence.
