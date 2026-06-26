# 2026-06-24 AOT 10-S1A Reflection Metadata Level

## Scope

10-S1A covers the first concrete reflection-tier carrier in the AOT ABI:

- AOT MethodInfo can state whether a method has no reflection metadata, runtime mapping metadata, or full description
  metadata.
- Generated MethodInfo records default to `RUNTIME_MAPPING`, matching the current dynamic-call/token-mapping needs
  without claiming full member enumeration metadata.
- ABI mismatch diagnostics use the current `ZR_VM_AOT_ABI_VERSION` value.

This does not close full 10-S1. Entity reachability, default-minimal type/member metadata, size reduction measurement,
and `DESCRIPTION` promotion through annotations or manifest retention remain future 10/12 work.

## Implementation

- `zr_vm_common/include/zr_vm_common/zr_aot_abi.h`
  bumps the ABI to version 5, adds `EZrAotReflectionMetadataLevel`, and adds `reflectionMetadataLevel` plus reserved
  bytes to `SZrAotMethodInfo`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  emits `.reflectionMetadataLevel = ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING` for generated method info.
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  locks the ABI version, enum values, MethodInfo field, and generated default.
- `tests/parser/test_aot_c_shared_library_smoke.c`
  asserts the exported module descriptor reports `RUNTIME_MAPPING` for the generated method.
- `tests/parser/test_aot_c_descriptor_diagnostics.c`
  checks ABI mismatch diagnostics against the current header constant instead of a stale literal.

## RED / GREEN

RED:

- `SZrAotMethodInfo` had no reflection metadata level.
- Generated AOT descriptors could not express the planned `NONE` / `RUNTIME_MAPPING` / `DESCRIPTION` distinction.
- The descriptor diagnostic test expected an old hard-coded ABI version.

GREEN:

- Source contracts find the reflection-level enum, all three enum values, the MethodInfo field, and the generated
  `RUNTIME_MAPPING` initializer.
- The shared-library smoke reads the generated module descriptor and verifies the method reflection level at runtime.
- Descriptor ABI mismatch diagnostics report the current expected version.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test`
  - 1 test, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_shared_library_smoke_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test`
  - 8 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_descriptor_diagnostics_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_descriptor_diagnostics_test`
  - 1 test, 0 failures

## Acceptance Decision

Accepted as 10-S1A only. The AOT ABI and generated MethodInfo now carry the reflection-tier state needed for later
runtime mapping and description policies, but the planned reachability-driven metadata trimming is not implemented yet.
