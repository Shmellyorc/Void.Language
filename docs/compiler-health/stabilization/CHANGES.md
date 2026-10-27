# Changed-file inventory

The initial workspace already contained `docs/KNOWN_ISSUES.md` research edits
and the untracked `docs/compiler-health/` research package. Their original patch
and documents are frozen separately. The protected root `README.md` is unchanged.
No standard-library source or Runtime implementation/header was modified.

The production change groups are shared frontend hints/inference and recursive
frame size; compact existing JSON generation; entry-only native emission after
full checking; pointer-home/descriptor setup; and automatic use of the existing
native-object store. The existing cache storage/security implementation is
unchanged. C++ embedding source defaults and raw emit/export behavior remain;
clients must rebuild against the changed C++ constructor/lower signatures.

The tests extend optimized cache/fresh-Runtime/reload coverage, shared inference
and LSP behavior, exact JSON metering, and directory-safe source/provenance
enumeration. The growable compound negative now mutates actual capture stores;
independent GC layout proofs and the IR validator remain unchanged. The stack
probe fixture is wider to continue exceeding a Windows stack page after packing.

## Tracked-file diff

- [CMakeLists.txt](../../../CMakeLists.txt)
- [docs/KNOWN_ISSUES.md](../../../docs/KNOWN_ISSUES.md)
- [docs/README.md](../../../docs/README.md)
- [docs/STAGE5_CACHE_PROPOSAL.md](../../../docs/STAGE5_CACHE_PROPOSAL.md)
- [docs/WRENCH_CLANKER.md](../../../docs/WRENCH_CLANKER.md)
- [include/void/backend.hpp](../../../include/void/backend.hpp)
- [include/void/cache.hpp](../../../include/void/cache.hpp)
- [include/void/frontend.hpp](../../../include/void/frontend.hpp)
- [include/void/ir.hpp](../../../include/void/ir.hpp)
- [src/backend/llvm/backend.cpp](../../../src/backend/llvm/backend.cpp)
- [src/frontend/compiler.cpp](../../../src/frontend/compiler.cpp)
- [src/frontend/generic_inference.inc](../../../src/frontend/generic_inference.inc)
- [src/frontend/json.inc](../../../src/frontend/json.inc)
- [src/ir/module.cpp](../../../src/ir/module.cpp)
- [src/main.cpp](../../../src/main.cpp)
- [src/native_build.cpp](../../../src/native_build.cpp)
- [tests/async/tests.cpp](../../../tests/async/tests.cpp)
- [tests/cache/processes.py](../../../tests/cache/processes.py)
- [tests/cache/tests.cpp](../../../tests/cache/tests.cpp)
- [tests/generics/tests.cpp](../../../tests/generics/tests.cpp)
- [tests/json/acceptance.py](../../../tests/json/acceptance.py)
- [tests/json/execution.py](../../../tests/json/execution.py)
- [tests/json/harness.py](../../../tests/json/harness.py)
- [tests/json/host.py](../../../tests/json/host.py)
- [tests/json/schema.py](../../../tests/json/schema.py)
- [tests/math/bootstrap.py](../../../tests/math/bootstrap.py)
- [tests/native/integrated.cpp](../../../tests/native/integrated.cpp)
- [tests/native/processes.py](../../../tests/native/processes.py)
- [tests/native/tests.cpp](../../../tests/native/tests.cpp)
- [tests/random/preservation.py](../../../tests/random/preservation.py)
- [tests/stage10/operator_compounds.cpp](../../../tests/stage10/operator_compounds.cpp)
- [tests/stage10/operator_reload.cpp](../../../tests/stage10/operator_reload.cpp)
- [tests/stage11/globals.cpp](../../../tests/stage11/globals.cpp)
- [tests/stage11/globals.py](../../../tests/stage11/globals.py)
- [tests/text_encoding/crash.py](../../../tests/text_encoding/crash.py)

## New permanent regression inputs and drivers

- [tests/cache/automatic.py](../../../tests/cache/automatic.py)
- [tests/generics/hints/anchor_chain_12.void](../../../tests/generics/hints/anchor_chain_12.void)
- [tests/generics/hints/anchor_chain_16.void](../../../tests/generics/hints/anchor_chain_16.void)
- [tests/generics/hints/anchor_chain_4.void](../../../tests/generics/hints/anchor_chain_4.void)
- [tests/generics/hints/anchor_chain_64.void](../../../tests/generics/hints/anchor_chain_64.void)
- [tests/generics/hints/anchor_chain_8.void](../../../tests/generics/hints/anchor_chain_8.void)
- [tests/generics/hints/anchor_chain_balanced20.void](../../../tests/generics/hints/anchor_chain_balanced20.void)
- [tests/generics/hints/anchor_chain_direct20.void](../../../tests/generics/hints/anchor_chain_direct20.void)
- [tests/generics/hints/anchor_chain_left18.void](../../../tests/generics/hints/anchor_chain_left18.void)
- [tests/generics/hints/anchor_chain_left20.void](../../../tests/generics/hints/anchor_chain_left20.void)
- [tests/generics/hints/async_call_argument.void](../../../tests/generics/hints/async_call_argument.void)
- [tests/generics/hints/async_library.void](../../../tests/generics/hints/async_library.void)
- [tests/generics/hints/async_methods.void](../../../tests/generics/hints/async_methods.void)
- [tests/generics/hints/async_qualified.void](../../../tests/generics/hints/async_qualified.void)
- [tests/generics/hints/await_direct_argument.void](../../../tests/generics/hints/await_direct_argument.void)
- [tests/generics/hints/await_task_explicit.void](../../../tests/generics/hints/await_task_explicit.void)
- [tests/generics/hints/await_task_local.void](../../../tests/generics/hints/await_task_local.void)
- [tests/generics/hints/chain_extracted20.void](../../../tests/generics/hints/chain_extracted20.void)
- [tests/generics/hints/choice_argument_explicit.void](../../../tests/generics/hints/choice_argument_explicit.void)
- [tests/generics/hints/choice_in_argument.void](../../../tests/generics/hints/choice_in_argument.void)
- [tests/generics/hints/choice_pattern.void](../../../tests/generics/hints/choice_pattern.void)
- [tests/generics/hints/dictionary_identity.void](../../../tests/generics/hints/dictionary_identity.void)
- [tests/generics/hints/double_explicit.void](../../../tests/generics/hints/double_explicit.void)
- [tests/generics/hints/double_propagate.void](../../../tests/generics/hints/double_propagate.void)
- [tests/generics/hints/nested_dictionary.void](../../../tests/generics/hints/nested_dictionary.void)
- [tests/generics/hints/nested_dictionary_explicit.void](../../../tests/generics/hints/nested_dictionary_explicit.void)
- [tests/generics/hints/nested_direct.void](../../../tests/generics/hints/nested_direct.void)
- [tests/generics/hints/nested_explicit.void](../../../tests/generics/hints/nested_explicit.void)
- [tests/generics/hints/nested_list.void](../../../tests/generics/hints/nested_list.void)
- [tests/generics/hints/nested_list_explicit.void](../../../tests/generics/hints/nested_list_explicit.void)
- [tests/generics/hints/nested_local.void](../../../tests/generics/hints/nested_local.void)
- [tests/generics/hints/order.void](../../../tests/generics/hints/order.void)
- [tests/generics/hints/readonly.void](../../../tests/generics/hints/readonly.void)
- [tests/generics/hints/spawn_argument.void](../../../tests/generics/hints/spawn_argument.void)
- [tests/generics/hints/spawn_await.void](../../../tests/generics/hints/spawn_await.void)
- [tests/generics/hints.py](../../../tests/generics/hints.py)
- [tests/generics/hints_lsp.py](../../../tests/generics/hints_lsp.py)
- [tests/json/metering.py](../../../tests/json/metering.py)
- [tests/json/metering.stdout](../../../tests/json/metering.stdout)
- [tests/json/metering.void](../../../tests/json/metering.void)
- [tests/json/nested_inference.void](../../../tests/json/nested_inference.void)

## Research records and generated evidence

- Existing research record updated with current disposition/link: [docs/compiler-health/BENCHMARKS.md](../../../docs/compiler-health/BENCHMARKS.md).
- Existing research record updated with current disposition/link: [docs/compiler-health/INFERENCE_REPAIR.md](../../../docs/compiler-health/INFERENCE_REPAIR.md).
- Existing research record updated with current disposition/link: [docs/compiler-health/ISSUES.md](../../../docs/compiler-health/ISSUES.md).
- Existing research record updated with current disposition/link: [docs/compiler-health/REPORT.md](../../../docs/compiler-health/REPORT.md).
- Existing research record updated with current disposition/link: [docs/compiler-health/ROADMAP.md](../../../docs/compiler-health/ROADMAP.md).

New stabilization documents are `HANDOFF.md`, `IMPLEMENTATION.md`, `CACHE.md`,
`BENCHMARKS.md`, `AUDIT.md`, `VERIFICATION.md`, `INCIDENTS.md`, this inventory and
`evidence/README.md`. The evidence manifest inventories source/build hashes,
portable source/measurement/IR/native/provenance archives and local frozen
compiler/failure images. Raw acceptance/incident receipts under
`docs/json/evidence/implementation/` and the incomplete controlled-stop directory
under `docs/text-encoding/evidence/crashes/` are preserved and classified.
The original research archives under `docs/compiler-health/evidence/` are unchanged.

Manual audit and reproduction drivers live under `build/compiler-stabilization/`;
their exact sources, commands and hashes are packaged with the portable evidence.
They add measurement instrumentation only, including a separate ORC backend copy;
production binaries were frozen before the controlled series and are reverified
afterwards. No timing assertion is added to routine CI.
