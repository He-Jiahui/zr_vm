# 2026-06-24 AOT 09-S1A GC Descriptor Offsets

## Scope

09-S1A covers the first verifiable GC descriptor slice for AOT value-type layouts:

- AOT C emits an offset-list descriptor for inline structs that contain GC-visible `SZrTypeValue` fields.
- Blittable/POD inline structs with no GC fields do not emit a descriptor.
- `ZrCore_TypeLayout_VisitGcValues()` can scan a struct through `gcFieldOffsets` metadata instead of re-walking
  the field table.

This did not close full 09-S1 at the time of 09-S1A. The generated descriptor still needed registration through the
AOT method/module metadata path; that follow-up is tracked separately by 09-S1B.

## Implementation

- `zr_vm_core/src/zr_vm_core/type_layout.c`
  adds the non-union struct metadata offset-table path in `ZrCore_TypeLayout_VisitGcValues()`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c`
  emits `ZrGcOffsets_<id>[]` and `ZrGcDescriptor_<id>` for inline struct layouts with `gcFieldCount > 0`.
- The emitter validates each GC offset against the layout byte size and `SZrTypeValue` width.
- `tests/parser/test_value_type_runtime.c`
  adds metadata-offset scan coverage.
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`
  adds generated-C descriptor coverage for a string-field struct and a POD int-only struct.
- `tests/parser/test_aot_c_type_layout_contracts.c`
  locks the source-level emitter contract.

## RED / GREEN

RED:

- AOT C only emitted `ZrLayout_*` typedefs and static size/align/field-offset asserts.
- `ZrCore_TypeLayout_VisitGcValues()` ignored `layout->gcFieldOffsets` and re-derived visits from fields.

GREEN:

- Metadata-offset scan visits the two offsets listed by `gcFieldOffsets`.
- Generated C for a struct containing `string` emits `zr_aot_gc_descriptor_offsets`, `ZrGcOffsets_`, and
  `ZrGcDescriptor_`.
- Generated C for an int-only POD struct emits no `ZrGcOffsets_` or `ZrGcDescriptor_`.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_value_type_runtime_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_value_type_runtime_test`
  - 14 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_value_type_shared_library_smoke_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test`
  - 2 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_type_layout_contracts_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_type_layout_contracts_test`
  - 1 test, 0 failures

## Acceptance Decision

Accepted as 09-S1A only. The offset-list descriptor is emitted and the core scan path can consume descriptor metadata.
The remaining module metadata publication gap is closed by
`tests/acceptance/2026-06-24-aot-09-s1b-gc-descriptor-module-table.md`.
