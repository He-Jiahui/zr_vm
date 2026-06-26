# 2026-06-24 AOT 12-S8D Full-AOT TYPEOF Reflection Closure

## Scope

12-S8D extends the full-AOT closure guard to the first reflection runtime contract:

- Default hybrid AOT C output can still emit `ZrLibrary_AotRuntime_TypeOf`.
- `requireFullAot` rejects unannotated `TYPEOF` because it requires runtime reflection support.
- The writer returns failure and removes the partial generated C file.

This does not cover invoker thunks, token-driven reflection, reflection annotations/dataflow, dynamic generic
instantiation, or a complete mark-and-sweep closure report.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  generalizes the full-AOT pre-emission scan from dynamic deopt closure to runtime closure.
- The guard treats bytecode and SemIR `TYPEOF` as a reflection runtime contract failure in full-AOT mode.
- `tests/parser/test_aot_c_global_shared_library_smoke.c`
  keeps the default hybrid TYPEOF runtime-boundary smoke and adds a full-AOT rejection fixture.

## RED / GREEN

RED:

- The new full-AOT TYPEOF fixture failed because `ZrParser_Writer_WriteAotCFileWithOptions()` returned success.

GREEN:

- Default hybrid TYPEOF generation still emits and compiles the runtime reflection helper path.
- Full-AOT TYPEOF generation returns `ZR_FALSE`.
- The rejected full-AOT path leaves no generated C artifact behind.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_global_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_global_shared_library_smoke_test'`
  - 10 tests, 0 failures

## Acceptance Decision

Accepted as 12-S8D only. Unannotated TYPEOF runtime reflection is no longer allowed in full-AOT mode, but invoker
thunks, token reflection, reflection annotations/dataflow, generic instance closure, and full mark-and-sweep closure
diagnostics remain open.
