# OS sprint file inventory

[evidence/inventory.json](evidence/inventory.json) is the exact delivered-file
inventory with SHA-256, initial/preserved origin, final Git status and final
compiler/Runtime/tool hashes. It includes all evidence descendants, excluding
only the inventory's own self-referential hash and ignored Python bytecode.
Build outputs/logs live under ignored `build/`; their selected hashes/configuration
are recorded by acceptance receipts. The separately appearing
`docs/VOID_STDLIB_BACKLOG_2026-10-27.md` is unrelated preserved work.

## Modified tracked files

- `CMakeLists.txt`: Runtime objects, narrow Apple language/dependencies and existing CTest registrations.
- `cmake/NativeBuild.cmake`: native Windows shell/COM/UUID and Apple Foundation/objc flags.
- `include/void/ir.hpp`: closed OS query opcode.
- `src/frontend/compiler.cpp`: canonical semantic selection, shared lowering and Runtime coordination.
- `src/frontend/capabilities.inc`: explicit owner-side effect.
- `src/ir/module.cpp`: independent query validation and transitive worker prohibition.
- `src/backend/llvm/backend.cpp`: shared lowering and JIT ABI registration.
- `stdlib/README.md`: public module usage/examples.
- `docs/README.md`: current implementation/index, preserving other existing content.

## New implementation and examples

- `include/void/os.hpp`
- `include/void/os_abi.hpp`
- `include/void/detail/os_integer.hpp`
- `src/frontend/os.inc`
- `src/ir/os.inc`
- `src/backend/llvm/os.inc`
- `src/runtime/os.cpp`
- `src/runtime/os_abi.cpp`
- `src/runtime/os_apple.mm`
- `stdlib/os.void`
- `examples/os/startup.void`
- `examples/os/save.void`
- `examples/os/resources.void`

## New focused tests

All files under `tests/os/` are inventoried individually in the JSON:
`acceptance.py`, `cache.py`, `check_smoke.py`, `edge_execution.py`, `errors.py`,
`examples.py`, `exec_host.py`, `execution.py`, `frontend.py`, `host.cpp`, `host.py`,
`install.py`, `locations.void`, `lsp.py`, `measure.py`, `owner.py`,
`owner_async.void`, `packaging.py`, `run_smoke.py`, `smoke.void`, `support.py`.
They use CTest, existing io/async/managed probes, the shared crash recorder and
existing LSP Client, without a second test framework or mutable production hook.

## Documentation and evidence

New delivery documents: `API.md`, `BENCHMARKS.md`, `FILES.md`, `HANDOFF.md` and
`IMPLEMENTATION.md` under `docs/os/`, plus the complete `evidence/` descendants.
The original `RESEARCH.md`, `DESIGN.md` and `ROADMAP.md` are preserved byte-for-byte,
with initial and final hashes. The protected root README and HEAD are unchanged.

No Git staging, commit, branch, push, hosted CI, native staging implementation,
editor-client implementation or historical crash record was changed.
