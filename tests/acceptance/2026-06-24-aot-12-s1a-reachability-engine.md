# 2026-06-24 AOT 12-S1A Reachability Engine

## Scope

12-S1A covers the first independent mark-and-sweep reachability core for AOT code stripping:

- A per-entity state machine: unmarked, marked-pending, processed.
- Dependency reasons for roots, manifest preservation, direct calls, field access, virtual calls, reflection, and generic
  instances.
- BFS over an explicit edge list with first reason and predecessor recording.

This does not close full 12-S1. The engine is not yet connected to SemIR/bytecode scanning, virtual/interface closure,
generic-instance closure, or AOT function-table filtering.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.h`
  defines state, reason, edge, and mark types plus `backend_aot_reachability_compute()`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_reachability.c`
  implements validation and BFS marking over caller-owned mark/queue buffers.
- `tests/parser/test_aot_reachability.c`
  validates root propagation, direct-call/field-access propagation, disconnected dead nodes, root reason preservation,
  and invalid graph rejection.
- `tests/CMakeLists.txt`
  registers `zr_vm_aot_reachability_test` and CTest `aot_reachability`.

## RED / GREEN

RED:

- After CMake regeneration, the new reachability test failed to compile because `backend_aot_reachability.h` did not
  exist.

GREEN:

- Root 0 marks direct-call and field-access dependencies transitively.
- Disconnected nodes remain unmarked.
- A manifest root keeps its manifest reason even if it is also reached through a direct-call edge.
- Queue-capacity and edge-target validation reject invalid graphs.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_reachability_test`
  - 2 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'ctest --test-dir build-wsl-gcc -R "aot_reachability" --output-on-failure'`
  - 1 test, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test`
  - 19 tests, 0 failures

## Acceptance Decision

Accepted as 12-S1A only. The mark engine is in place and testable, but dead functions will not be removed from AOT
output until a later slice connects the engine to function-table construction and emission.
