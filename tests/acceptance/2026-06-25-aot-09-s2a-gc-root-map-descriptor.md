# AOT 09-S2A GC root map descriptor ABI

Date: 2026-06-25 07:10:49 +08:00

Status: 09-S2 sub-slice complete; full 09-S2 remains open.

## Scope

- Added public AOT ABI types for root-map publication:
  `EZrAotGcRootLocationKind`, `SZrAotGcRootSlot`, and `SZrAotGcRootMap`.
- Generated AOT C method metadata now emits `zr_aot_gc_root_slots_<flatIndex>[]`
  and `zr_aot_gc_root_map_<flatIndex>` for inline-struct frame slots whose
  resolved `SZrTypeLayout` has GC fields.
- `SZrAotMethodInfo.gcRootMap` points at the generated map only when roots
  exist; POD/blittable structs and pure scalar functions keep `ZR_NULL`.

## RED

- `zr_vm_aot_c_value_type_shared_library_smoke_test` failed to build generated C
  because the generated code referenced `SZrAotGcRootMap` and
  `ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET` before the public ABI existed.

## GREEN

- Generated C for a struct containing a `string` field publishes a non-null
  method `gcRootMap`, with frame-byte-offset root slots bound to the emitted
  GC descriptor's `typeLayoutId` and GC field offset.
- Generated C for a POD struct does not emit root-map symbols and keeps
  `.gcRootMap = ZR_NULL`.

## Verification

- WSL gcc direct:
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0
- WSL gcc CTest:
  - `aot_c_type_layout_contracts`
  - `aot_c_generic_monomorphization`
  - `aot_c_generic_reference_sharing`
  - `aot_c_generic_call_typed`
  - Result: 4/4 passed
- WSL clang direct:
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test` 2/0
- WSL clang CTest:
  - same focused CTest group as gcc
  - Result: 4/4 passed
- Windows MSVC Debug:
  - `zr_vm_aot_c_frame_setup_contracts_test.exe` 1/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test.exe` 2/0, with the
    Unix shared-library execution branch ignored as designed
  - CTest `aot_c_type_layout_contracts|aot_c_generic_reference_sharing|aot_c_generic_call_typed`
    3/3 passed

## Not Closed

- Runtime root-stack push/pop for generated functions.
- Stress-GC validation that AOT stack references survive collection.
- Moving-GC pointer rewrite through the published root map.
- Safepoint insertion and polling.
- Reference write barriers and compile-time barrier elimination.
- Boxing / FFI pinning work from 09-S5.
