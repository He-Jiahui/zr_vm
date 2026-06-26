# 2026-06-24 AOT 09-S1B GC Descriptor Module Table

## Scope

09-S1B publishes generated value-type GC descriptors through the public AOT module descriptor:

- Public ABI exposes `SZrAotGcDescriptor`.
- `ZrAotCompiledModule` exposes `gcDescriptors` and `gcDescriptorCount`.
- Generated C emits a sparse `zr_aot_gc_descriptors[]` table indexed by `typeLayoutId`.
- Modules without GC-bearing value-type layouts publish no descriptor table.

This closes 09-S1 together with 09-S1A. It does not implement precise AOT stack roots, safepoints, write barriers,
boxing/FFI pinning, or runtime token/layout hydration.

## Implementation

- `zr_vm_common/include/zr_vm_common/zr_aot_abi.h`
  bumps `ZR_VM_AOT_ABI_VERSION` to `6u`, adds `SZrAotGcDescriptor`, and adds module descriptor fields for the
  GC descriptor table.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.{h,c}`
  emits public `SZrAotGcDescriptor` records and writes the sparse module-level descriptor table.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  computes descriptor index space, emits the descriptor table, and publishes it from `zr_aot_module`.
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`
  compiles and loads a generated string-field struct module and reads the descriptor through
  `ZrVm_GetAotCompiledModule()`.
- `tests/parser/test_aot_c_shared_library_smoke.c`
  verifies a no-GC generated module publishes `NULL`/`0` descriptor fields.
- Source contract tests lock the public ABI and emitter/table-generation shape.

## RED / GREEN

RED:

- The new value-type smoke failed to compile because `SZrAotGcDescriptor` did not exist.
- `ZrAotCompiledModule` had no `gcDescriptors` or `gcDescriptorCount` fields.

GREEN:

- Generated string-field struct modules emit `ZrGcOffsets_<id>[]`, public `ZrGcDescriptor_<id>`, and
  `zr_aot_gc_descriptors[]`.
- The loaded module descriptor exposes a non-null descriptor table; the first non-null descriptor has
  `typeLayoutId` matching its table index, `gcFieldCount == 1`, and non-null `gcFieldOffsets`.
- POD/no-GC modules emit no descriptor table and publish `NULL`/`0`.

## Validation

- `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_type_layout_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_descriptor_diagnostics_test -j2"`
  - targets built successfully
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_value_type_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test -j2"`
  - targets built successfully
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
  - 19 tests, 0 failures
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_type_layout_contracts_test"`
  - 1 test, 0 failures
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test"`
  - 1 test, 0 failures
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_descriptor_diagnostics_test"`
  - 1 test, 0 failures
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"`
  - 2 tests, 0 failures
- `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test"`
  - 8 tests, 0 failures

## Acceptance Decision

Accepted as 09-S1B. Combined with 09-S1A, the GC descriptor offset-list slice now has generated descriptor emission,
core metadata offset scanning, and AOT module metadata publication.
