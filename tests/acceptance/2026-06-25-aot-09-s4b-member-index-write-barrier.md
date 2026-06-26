# AOT 09-S4B Member/Index Write Barrier Acceptance

## Scope

09-S4B covers the current generated-C member/index heap-store path:

- generated C emits explicit AOT runtime boundaries for member, cached-member, dynamic index, and super-array writes
- the AOT runtime helpers delegate actual heap stores to the core object layer
- the core object heap-store sites now use the public `ZrCore_Gc_WriteBarrier(state, ownerObject, value)` boundary

This is still a partial 09-S4 result. Compile-time elimination for owners proven newly allocated in the current generated function remains 09-S4C.

## Baseline

The RED source contract was added to `tests/parser/test_aot_c_global_contracts.c` and failed before implementation:

- generated-C member/index boundaries existed, but the object heap-store key/value paths still used `ZrCore_Value_Barrier(state, ...)`
- the required public `ZrCore_Gc_WriteBarrier(state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &pair->key/value)` source markers were missing

The old behavior was semantically equivalent at the remembered-set level, but it bypassed the public GC write-barrier boundary required by the AOT memory-management plan.

## Implementation Summary

- `zr_vm_core/src/zr_vm_core/object/object.c` now uses `ZrCore_Gc_WriteBarrier` for object key/value heap-store barriers.
- `zr_vm_core/src/zr_vm_core/object/object_internal.h` now uses `ZrCore_Gc_WriteBarrier` for the existing string-pair fast-path value barrier.
- `tests/parser/test_aot_c_global_contracts.c` now verifies:
  - generated C member/index store opcodes lower through AOT runtime boundaries
  - AOT runtime setter helpers route to core object setters
  - core object heap-store sites use the public GC write-barrier API
  - old object heap-store `ZrCore_Value_Barrier(state, ...)` calls are absent from those sites
- `zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c` was synchronized with the main runtime source for the closure/capture write-barrier calls, avoiding a stale AOT runtime copy with old direct value-barrier calls.

No new object-storage helper or responsibility was added to `object.c`; the oversized file received only same-site barrier entry replacement.

## Tooling Evidence

WSL gcc direct checks:

- `zr_vm_aot_gc_root_frame_test`: 4/0
- `zr_vm_aot_c_global_contracts_test`: 8/0
- `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0
- `zr_vm_aot_c_super_array_contracts_test`: 1/0
- `zr_vm_aot_c_super_array_shared_library_smoke_test`: 1/0
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0
- `zr_vm_aot_c_constant_contracts_test`: 5/0

WSL clang direct checks:

- `zr_vm_aot_gc_root_frame_test`: 4/0
- `zr_vm_aot_c_global_contracts_test`: 8/0
- `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0
- `zr_vm_aot_c_super_array_contracts_test`: 1/0
- `zr_vm_aot_c_super_array_shared_library_smoke_test`: 1/0
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0
- `zr_vm_aot_c_constant_contracts_test`: 5/0

Windows MSVC Debug direct checks:

- `zr_vm_aot_gc_root_frame_test`: 4/0
- `zr_vm_aot_c_global_contracts_test`: 8/0
- `zr_vm_aot_c_global_shared_library_smoke_test`: 10/0, with 10 Unix shared-library branches ignored
- `zr_vm_aot_c_super_array_contracts_test`: 1/0
- `zr_vm_aot_c_super_array_shared_library_smoke_test`: 1/0, with 1 Unix shared-library branch ignored
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0, with 1 Unix shared-library branch ignored
- `zr_vm_aot_c_constant_contracts_test`: 5/0

## Boundary Cases

- Stack inline struct stores remain barrier-free; this remains covered by the value-type generated-C smoke.
- Member/index/super-array writes are not raw generated-C heap stores in the current backend; generated C calls runtime boundaries, and the resolved heap owner is known only in the core object setter.
- Old-to-young remembered-set behavior remains covered by `test_gc_write_barrier_records_old_to_young_value`.
- New-owner compile-time barrier elimination is not covered by this slice and remains open.

## Acceptance Decision

Accepted as 09-S4B partial completion. Full 09-S4 remains open for 09-S4C compile-time write-barrier elimination for owners proven newly allocated in the current function.
