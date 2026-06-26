# AOT 09-S2B runtime root-frame stack

Date: 2026-06-25 08:14 +08:00

## Scope

- Added the runtime carrier for AOT precise stack roots: `SZrAotGcRootFrame` on `SZrState`.
- Added `ZrCore_Gc_AotRootFramePush`, `ZrCore_Gc_AotRootFramePop`, and `ZrCore_Gc_AotRootFrameDepth`.
- Wired GC mark and minor-GC rewrite over `SZrAotGcRootMap` frame-byte-offset roots.
- Bound `SZrAotMethodInfo` into `ZrAotGeneratedModuleContext`.
- Generated AOT C now pushes/pops a root frame only when the method has a non-null `gcRootMap`; POD/blittable output does not emit root-frame calls.

## RED

- `zr_vm_aot_gc_root_frame_test` was added first and failed to compile because `SZrAotGcRootFrame`, `SZrState.aotGcRootFrameStack`, and the `ZrCore_Gc_AotRootFrame*` APIs did not exist.

## GREEN

- WSL GCC:
  - `zr_vm_aot_gc_root_frame_test`: 2/0
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0
  - CTest `aot_gc_root_frame`: 1/1
  - `zr_vm_gc_test`: 66/0
- WSL Clang:
  - `zr_vm_aot_gc_root_frame_test`: 2/0
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test`: 2/0
  - CTest `aot_gc_root_frame`: 1/1
- Windows MSVC Debug:
  - `zr_vm_aot_gc_root_frame_test.exe`: 2/0
  - `zr_vm_aot_c_frame_setup_contracts_test.exe`: 1/0
  - `zr_vm_aot_c_value_type_shared_library_smoke_test.exe`: 2/0 with the Unix shared-library execution branch ignored
  - CTest `aot_gc_root_frame`: 1/1

## Remaining

- `ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS` is still an ABI-reserved location kind, not an enabled generated-C path.
- Safepoint insertion, write barriers, boxing, and FFI pinning remain later 09 slices.
