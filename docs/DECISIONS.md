# Architectural decisions

## Foundation phase, 10 October 2026

### C++20 and LLVM 19.1.7

The compiler uses the LLVM C++ API, with an exact CMake SDK version requirement.
ORC `LLJIT`, resource trackers, `IRBuilder`, `DIBuilder`, `PassBuilder` and
`TargetMachine` were compiled and exercised on the installed 19.1.7 SDK.
This settles the compiler implementation choice for this phase. LLVM remains
an implementation dependency, not a language specification.

The original Bible/Strategy design snapshots are preserved. Their discussion of
an open implementation-language choice records the state before this phase.
The user's larger cohesive phases supersede the snapshots' numbered milestone
process; language semantics are unchanged.

### Keep the proof IR small and independent

`void_core` owns source locations and a validated IR without LLVM headers.
The proof represents i32 arguments/constants/arithmetic, comparisons, calls,
branches and returns. Functions currently have one fixed `i32(i32)` signature.
Values are local to a basic block; there are no PHIs in this proof IR. Integer
arithmetic uses bitwise wrapping backend operations for the experiment only;
VOID overflow semantics remain open. No new user syntax is introduced.

AOT and JIT use the same lowering, verification and optional LLVM O2 pipeline.
The CLI is explicitly an architecture harness. AOT generates an object; the
integration driver links it using Clang. The compiler does not shell out with
unescaped user input. Source `.void` parsing is the next frontend phase.

### Generation leases for native replacement

Stable identity is the canonical module identity plus function name, independent
of the native address. Each compiled module has distinct versioned symbols and
one ORC resource tracker. Replacement publishes one entry for a whole dependent
module generation. Internal calls stay within that generation. The compatibility
contract includes signature, object layout, captures and constants.

A `ReloadSlot` uses a short mutex to acquire/publish shared code ownership. It
never holds the mutex while native code runs or a resource tracker is removed.
Active calls keep their generation and execution session alive. Tracker removal
occurs only after the last lease is released. Code can outlive its `Jit` host.
No address is exposed through the callable public API. The bind API is trusted
compiler-host machinery and requires an exact native ABI declaration.

This is a conservative, measured correctness proof. It does not implement file
watching, compiler-derived compatibility fingerprints, unrelated multi-slot
transactions, async suspension or foreign-thread lifetime management. Metadata
is supplied by the host, not derived from parsed source. Compilation and symbol
binding are serialized by the host; callers may invoke leases concurrently.

### Explicit precise roots first

The experimental heap uses non-moving mark/sweep, exact reference-field offsets,
zero-initialized payloads, iterative graph tracing and explicit root slots.
Descriptors are copied into allocation records, so callers need not retain
metadata objects. Embedded value references are flattened into descriptor offsets.
Bad roots abort tracing before sweeping and reset temporary mark bits.

The generated root probe registers the address of a stack slot, stores its live
managed reference there, calls native safepoints in a loop, reloads the slot,
and unregisters it on normal exit. External calls intentionally retain ordinary
memory side-effect semantics. LLVM O2 cannot discard a registered live slot.
No pointer is hidden from LLVM through integer casts or blanket volatile loads.

This establishes a basic reference contract, not a production collector. The heap
is single-threaded. There is no allocator cache, concurrent collection, interior
pointer support, weak references or unwind cleanup. Stack/register discovery,
`gc.statepoint` and stack-map decoding remain deferred. LLVM 19.1.7's
`IR/Statepoint.h`, `IR/GCStrategy.h`, and IRBuilder statepoint API were reviewed;
they require a GC strategy, root/base-pointer liveness, target metadata decoding
and runtime coordination that this explicit-root experiment does not provide.
Do not promote this phase to the strategy's full GC integration gate.

### Trusted C bridge proof

A real C translation unit provides representative functions, an opaque resource
and retained callback registration. Generated native code calls the fixture
through ORC absolute symbols. UTF-8 bridge conversion validates scalar encoding,
rejects embedded NUL for terminated APIs, and owns a temporary terminated copy.
This is a trusted bridge helper, not a second VOID string type.

The callback has a stable C trampoline and an explicit context whose registered
root keeps a captured managed object alive. The context uses the reload slot,
so it follows replacement. The fixture unregisters before context/root teardown;
RAII releases the native resource exactly once. Foreign threads, concurrent
unregister, retained string APIs and exception translation remain future work.

### Cross-platform CI and local-only work

Use standard GitHub-hosted Linux, Windows and macOS runners, read-only repository
permissions, one build system and complete official LLVM 19.1.7 SDK archives.
Avoid Windows compiler-only installers that omit development libraries. SDK
versions are checked at configuration, not silently substituted. CI results
remain unverified until the user creates/pushes the repository and jobs execute.
The user owns repository creation and initial publication.

References: [LLVM ORC design](https://releases.llvm.org/19.1.0/docs/ORCv2.html),
[official 19.1.7 releases](https://github.com/llvm/llvm-project/releases/tag/llvmorg-19.1.7).
