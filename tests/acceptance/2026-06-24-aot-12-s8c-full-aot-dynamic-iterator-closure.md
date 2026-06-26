# 2026-06-24 AOT 12-S8C Full-AOT Dynamic Iterator Closure

## Scope

12-S8C extends the full-AOT closure guard to dynamic iterator runtime boundaries:

- Default hybrid AOT C output can still emit iterator runtime helper calls.
- `requireFullAot` rejects dynamic iterator init/move-next paths that would need a runtime fallback boundary.
- The writer returns failure and removes the partial generated C file.

This does not cover reflection, dynamic generic instantiation, manifest-driven closure, or a complete mark-and-sweep
closure report.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  expands the full-AOT dynamic-deopt pre-emission scan to iterator opcodes.
- The guard treats explicit `DYN_ITER_INIT`, `DYN_ITER_MOVE_NEXT`, and
  `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE` bytecode plus SemIR `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT` as dynamic
  iterator closure failures in full-AOT mode.
- `tests/parser/test_aot_c_iterator_shared_library_smoke.c`
  keeps the default hybrid iterator helper smoke and adds a full-AOT rejection fixture.

## RED / GREEN

RED:

- The new full-AOT dynamic iterator fixture failed because `ZrParser_Writer_WriteAotCFileWithOptions()` returned
  success.

GREEN:

- Default hybrid iterator generation still emits and compiles the iterator runtime helper path.
- Full-AOT dynamic iterator generation returns `ZR_FALSE`.
- The rejected full-AOT path leaves no generated C artifact behind.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_iterator_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_iterator_shared_library_smoke_test'`
  - 2 tests, 0 failures

## Acceptance Decision

Accepted as 12-S8C only. Dynamic iterator runtime fallback is no longer allowed in full-AOT mode, but reflection,
dynamic generic instance closure, and full mark-and-sweep closure diagnostics remain open.
