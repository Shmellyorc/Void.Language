# Private fixed-array call experiment (Order 2)

Optional same-build native fixture, enabled by `VOID_LLVM_RESEARCH=ON`.
It remains separate from ordinary source/JIT/AOT entrypoints. Order 3 now
implements production numeric fixed arrays independently of this fixture.

The generated caller makes eight independent Runtime-buffer snapshots, mutating
the source after each one. It passes eight root-home pointers plus result,
layout, quota and failure directly to an internal LLVM function. LLVM handles
register/stack argument transport. The callee loads handles into its own precise
Frame/Slot homes before a collection, then copies the selected argument into the
registered result. Caller and callee cleanup run on every helper failure.

Removed: Envelope, CallBinding, identity/profile/epoch comparisons, duplicated
argument/context copies, expected-Frame authorization, repeated root-validation
calls, synthetic ownership-wrapper validation, the store-identity protocol
verifier, and the custom linked-byte/ABI delivery gate. The fixture uses the
same LLVM verifier, O2 pipeline and object writer as ordinary AOT. The separate
Order 1 storage auditor and its negative tests remain available unchanged.

One static scalar layout/extent check remains at the embedding boundary. The
existing Runtime data helpers continue to check root registration, live buffer
layout, leases, complete snapshots, safe polling and transactional publication.
No managed payload is transported on the native stack.

`generated.cpp` emits the closed fixture and rejects invalid typed/body inputs
before modifying an existing destination. `generated_runtime.cpp` tests ordered,
independent snapshots; caller/callee roots across collection and after return;
independent result storage; invalid roots/layout/quota; alias rejection;
preserved failures; pre/post-collection failures; failed return publication;
and collection of all temporary buffers. Its hooks only observe or inject test
failures. Identical generated objects also run with empty hooks, without an
authorization registry or metadata witness.

Normal/O2 tests select arguments 0 and 3 with extent 2, and argument 7 with extent
16384 (128 KiB). This exercises ordinary native stack arguments and large heap
payloads without a 64 KiB policy. A finite test shape is not a production limit.

```sh
cmake -S . -B build/ci-debug -DVOID_LLVM_RESEARCH=ON
python3 tests/research/call_envelope/run.py --build build/ci-debug
```

The historical directory name is retained for continuity. [PILOT.md](PILOT.md),
[INTEGRATION.md](INTEGRATION.md) and [OWNERSHIP.md](OWNERSHIP.md) describe superseded
experiments, not current code or acceptance requirements. Their reports remain
historical evidence; their removed protocol/certificate drivers are not needed
for these behavioral tests. This is a closed scalar fixture, not a general
array lowerer, public FFI, JIT-array feature, or cross-platform certification.
