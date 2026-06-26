# AOT 11-S4O - code-registration type layout token carrier

## Scope

11-S4O adds an ABI carrier for the cTypeId/typeLayoutId -> metadata token side of the 11-S4 mapping.

Affected code:
- `zr_vm_common/include/zr_vm_common/zr_aot_abi.h`
- `zr_vm_core/include/zr_vm_core/metadata_runtime.h`
- `zr_vm_core/src/zr_vm_core/module/module.c`
- `zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c`
- `zr_vm_library/src/zr_vm_library/aot_runtime.c`
- `zr_vm_aot/zr_vm_library/src/zr_vm_library/aot_runtime.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_artifacts.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_module_prelude.c`
- `tests/module/test_metadata_runtime_typespec_layout.c`
- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_aot_c_frame_setup_contracts.c`
- `tests/parser/test_aot_c_shared_library_smoke.c`
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`

## Baseline

11-S4N exposed `ZrCore_MetadataRuntime_ResolveCTypeIdToken(runtime, cTypeId)`, but it still had to scan zrp TypeDef/TypeSpec rows when the bounded cache missed. The code-registration ABI had no direct token table indexed by cTypeId/typeLayoutId.

This slice adds the carrier and runtime consumption path. The generated table entries are intentionally zero-filled for now because the current emitter does not yet have a reliable TypeDef/TypeSpec token source for every emitted layout. It does not claim a fully populated persistent cTypeId-to-token index.

## RED

Added focused coverage for:
- generated/public ABI fields: `typeLayoutTokens` and `typeLayoutTokenCount`
- generated C token table emission and descriptor wiring
- runtime descriptor/codeRegistration token-table consistency validation
- metadata runtime resolution from a manually supplied code-registration token table
- rejection of non-type tokens and entries whose layout cannot be resolved

RED command:

```sh
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_typespec_layout_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_value_type_shared_library_smoke_test -j2"
```

RED result: build failed because `SZrAotCodeRegistration` had no `typeLayoutTokens` / `typeLayoutTokenCount`, and `SZrMetadataRuntime` had no `typeLayoutTokenCount`.

## GREEN

Implementation:
- Bumped `ZR_VM_AOT_ABI_VERSION` to `10u`.
- Added `typeLayoutTokens/typeLayoutTokenCount` to `SZrAotCodeRegistration` and `ZrAotCompiledModule`.
- Generated C now emits `zr_aot_type_layout_tokens[]` indexed by cTypeId/typeLayoutId and wires it into the module descriptor and code registration.
- Runtime loader validation rejects descriptor/codeRegistration token-table pointer/count mismatches and requires token table count to match `typeLayoutCount`.
- `SZrMetadataRuntime` mirrors `typeLayoutTokenCount` during module attach.
- `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()` and `ResolveCTypeIdToken()` check the code-registration token table before falling back to zrp row scans.
- Token-table entries are accepted only when the token is TypeDef or TypeSpec and the matching registry-backed layout resolves.

## Validation

WSL GCC:
- `zr_vm_metadata_runtime_typespec_layout_test`: `14 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_source_contracts_test`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_frame_setup_contracts_test`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_shared_library_smoke_test`: `8 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: `2 Tests 0 Failures 0 Ignored`

WSL clang:
- `zr_vm_metadata_runtime_typespec_layout_test`: `14 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_source_contracts_test`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_frame_setup_contracts_test`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_shared_library_smoke_test`: `8 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: `2 Tests 0 Failures 0 Ignored`

Windows MSVC Debug:
- `zr_vm_metadata_runtime_typespec_layout_test.exe`: `14 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_source_contracts_test.exe`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_frame_setup_contracts_test.exe`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_shared_library_smoke_test.exe`: `0 Tests 0 Failures 8 Ignored`
- `zr_vm_aot_c_value_type_shared_library_smoke_test.exe`: `1 Tests 0 Failures 1 Ignored`

CTest note: the focused CTest filter only registers `metadata_runtime_typespec_layout` in the current build trees; the AOT C contract and smoke targets above were validated by running their executables directly.

Existing warnings observed during validation remained non-fatal: generated dispatch label/unreachable-code warnings, existing const-qualifier warnings, parser unused-function/unused-parameter warnings, and the existing MSVC possible-uninitialized warning in `metadata_runtime.c`.

## Acceptance Decision

Accepted for 11-S4O. Code registration can now carry a type-layout token table and metadata runtime can consume valid entries before zrp scan fallback. Full 11-S4 remains open for real token population, TypeSpec/generic layout materialization, ownership offset emission, runtime layout construction, and any future cTypeId/typeLayoutId decoupling.
