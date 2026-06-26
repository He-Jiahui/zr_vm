# AOT 09-S4A Write Barrier API Acceptance

## Scope

09-S4A covers the public AOT write-barrier API and the existing closure/capture heap-owner write boundary in the AOT runtime. It also records the current elimination proof for stack inline struct field stores.

This is not the full 09-S4 milestone. Direct generated-C member/index heap-owner barrier insertion and compile-time elimination for provably new young owners remain 09-S4B.

## RED Baseline

- `tests/parser/test_aot_c_constant_contracts.c` failed before implementation because the public `ZrCore_Gc_WriteBarrier(state, ownerObject, value)` API and the AOT runtime source markers did not exist.
- `zr_vm_library/src/zr_vm_library/aot_runtime.c` still called `ZrCore_Value_Barrier(state, ...)` directly from value-based closure/capture heap-owner paths.

## Implementation Summary

- `zr_vm_core/include/zr_vm_core/gc.h` exposes `ZrCore_Gc_WriteBarrier(state, ownerObject, value)` for AOT-facing generated/runtime code.
- `zr_vm_core/src/zr_vm_core/gc/gc.c` implements the wrapper by delegating to `ZrCore_Value_Barrier(state, ownerObject, value)`.
- `zr_vm_library/src/zr_vm_library/aot_runtime.c` migrates the closure projection, native closure capture binding, and closure value setter value-based barriers to `ZrCore_Gc_WriteBarrier`.
- `tests/parser/test_aot_c_guardrail_contracts.c` treats `ZrCore_Gc_SafePoint` and `ZrCore_Gc_WriteBarrier` as explicit allowed GC runtime boundaries.
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c` verifies stack inline struct generated-C field stores do not emit `ZrCore_Gc_WriteBarrier(`.

The raw capture-owner pointer barrier remains on `ZrCore_RawObject_Barrier` because this 09-S4A wrapper is value-based and receives an `SZrTypeValue *`.

## Verification

WSL gcc:

- `zr_vm_aot_gc_root_frame_test`: 4/0
- `zr_vm_aot_c_constant_contracts_test`: 5/0
- `zr_vm_aot_c_guardrail_contracts_test`: 6/0
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0
- `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0

WSL clang:

- `zr_vm_aot_gc_root_frame_test`: 4/0
- `zr_vm_aot_c_constant_contracts_test`: 5/0
- `zr_vm_aot_c_guardrail_contracts_test`: 6/0
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0
- `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0

Windows MSVC Debug:

- `zr_vm_aot_gc_root_frame_test`: 4/0
- `zr_vm_aot_c_constant_contracts_test`: 5/0
- `zr_vm_aot_c_guardrail_contracts_test`: 6/0
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0, with 1 Unix execution branch ignored
- `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0, with 10 Unix shared-library branches ignored

## Boundary Cases

- The public wrapper currently preserves the existing runtime remembered-set semantics by delegating to `ZrCore_Value_Barrier`.
- Stack inline struct stores remain barrier-free because they write byte-frame/local storage rather than a heap owner.
- Closure/capture heap-owner value stores use the public GC wrapper.
- Direct generated-C member/index heap-owner stores are not covered by this slice.
- Compile-time elimination for owners proven newly allocated in the current function is not covered by this slice.

## Acceptance Decision

Accepted as 09-S4A partial completion. Full 09-S4 remains open for direct generated-C barrier insertion and young-owner elimination.
