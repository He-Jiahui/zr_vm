# AOT 07-S6 reference-local local-address root frame

Timestamp: 2026-07-06 06:54:43 +08:00

## Scope

This slice turns the generated reference-local frame from the previous slice into a GC-visible local-address root frame:

- file-scope `zr_aot_ref_root_slots_<flatIndex>[]` entries describe `zr_aot_ref_locals.oN` fields with `ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS`
- file-scope `zr_aot_ref_root_map_<flatIndex>` maps group those local-address roots separately from the VM stack byte root map
- each generated function with reference locals declares `zr_aot_ref_gc_root_frame` and pushes it with base `(TZrStackValuePointer)(void *)&zr_aot_ref_locals`
- cleanup pops the reference-local root frame before the ordinary VM-stack byte root frame cleanup

The existing `methodInfo.gcRootMap` remains tied to ordinary VM stack `FRAME_BYTE_OFFSET` roots only. This slice intentionally does not mix local-address roots into `zr_aot_gc_root_map_<flatIndex>`, because that map is pushed with `zr_aot_slot_base` as its frame base.

## Baseline

- Previous generated C emitted `SZrAotReferenceLocals_<flatIndex>` and `zr_aot_ref_locals`, but no generated `LOCAL_ADDRESS` root map or local-address root frame.
- Runtime `LOCAL_ADDRESS` support already existed in the GC root-frame marker and forwarding path from the 07-S6 / 09-S2 runtime slice.
- The generated normal AOT root frame still uses the VM stack slot base, so it can safely contain `FRAME_BYTE_OFFSET` roots but not C-local address roots.

## RED

The focused generated-C smokes and source contracts were strengthened before production changes to require:

- `static const SZrAotGcRootSlot zr_aot_ref_root_slots_0[] = {`
- `.frameByteOffset = (TZrUInt32)offsetof(SZrAotReferenceLocals_0, o2),`
- `.locationKind = (TZrUInt8)ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS,`
- `static const SZrAotGcRootMap zr_aot_ref_root_map_0 = {`
- `SZrAotGcRootFrame zr_aot_ref_gc_root_frame;`
- `/* zr_aot_reference_local_root_frame_push */`
- push base `(TZrStackValuePointer)(void *)&zr_aot_ref_locals`
- pop via `ZrCore_Gc_AotRootFramePop(state, &zr_aot_ref_gc_root_frame);`
- no `.gcRootMap = &zr_aot_ref_root_map_0`
- source-level helper declaration for `backend_aot_c_reference_locals_has_locals(`

Initial WSL GCC direct results after rebuilding the updated tests:

- `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8 tests / 2 failures
- `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9 tests / 2 failures
- `zr_vm_aot_c_source_contracts_test`: 24 tests / 1 failure

## GREEN

`backend_aot_c_reference_locals.c/.h` now owns the generated reference-local root artifacts:

- `backend_aot_write_c_reference_local_root_maps()` emits one `LOCAL_ADDRESS` root slot per unique `TO_STRING` / `TO_OBJECT` destination
- `backend_aot_write_c_reference_local_root_frame_declaration()` declares the generated frame and boolean guard
- `backend_aot_write_c_reference_local_root_frame_push()` pushes the frame after `zr_aot_ref_locals` exists
- `backend_aot_write_c_reference_local_root_frame_cleanup()` pops the frame on function exit

The top-level emitter writes the reference-local root maps after the reference-local typedefs. Function-body generation pushes the local-address frame before value SemIR setup and dispatch, then pops it before ordinary VM-stack root cleanup.

## Verification

- WSL GCC:
  - focused builds passed after one combined build timed out and the targets were rebuilt individually
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8/0
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9/0
  - `zr_vm_aot_c_source_contracts_test`: 24/0
  - `zr_vm_aot_c_logical_contracts_test`: 4/0
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0
- Generated C inspection:
  - `build-wsl-gcc/tests_generated/aot_c_shared_library/generic_logical_not_dynamic_string_slot_project/bin/aot_c/src/main.c`
  - `build-wsl-gcc/tests_generated/aot_c_shared_library/generic_jump_if_dynamic_string_slot_project/bin/aot_c/src/main.c`
  - confirmed `zr_aot_ref_root_slots_0[]`, `offsetof(SZrAotReferenceLocals_0, o2)`, `ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS`, `zr_aot_ref_root_map_0`, `zr_aot_ref_gc_root_frame`, push base `&zr_aot_ref_locals`, and balanced pop
- WSL Clang:
  - focused build passed
  - logical-not smoke 8/0, jump-if smoke 9/0, logical contracts 4/0, source contracts 24/0, frame setup contracts 1/0
  - existing `const char *` to `TZrNativeString` warnings remain in the two smoke helpers
- Windows MSVC Debug:
  - focused build passed
  - logical-not smoke: 8 expected Unix-only ignores / 0 failures
  - jump-if smoke: 9 expected Unix-only ignores / 0 failures
  - logical contracts 4/0, source contracts 24/0, frame setup contracts 1/0

## Acceptance Decision

Accepted for this narrow slice.

## Open Items

- Broader GC pressure/root-correctness stress for generated reference locals.
- Exports/frame cleanup, in/out writeback, performance counters, complete 07-S6 acceptance, and broader 07~12 completion.

## Large-File Note

`backend_aot_c_function_body.c` and `backend_aot_c_emitter.c` are already oversized. This slice keeps the new root-map and root-frame logic in the small `backend_aot_c_reference_locals.c` helper and limits the large-file edits to orchestration calls.
