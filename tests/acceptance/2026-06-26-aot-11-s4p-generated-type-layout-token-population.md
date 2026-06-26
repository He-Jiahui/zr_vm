# AOT 11-S4P - generated TypeDef-backed type layout token population

## Scope

11-S4P populates the reliable subset of generated `zr_aot_type_layout_tokens[]` entries. The table now carries real `TYPE_DEF` tokens for generated named layouts that can be uniquely matched to local TypeDef metadata.

Affected code:
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c`
- `tests/parser/test_aot_c_type_layout_contracts.c`
- `tests/parser/test_aot_c_source_contracts.c`
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`

## Baseline

11-S4O added the code-registration token-table carrier and runtime consumption path, but generated entries were still zero-filled. That proved ABI shape and loader/runtime behavior, not compiler-side token population.

This slice keeps the conservative fallback: missing metadata, ambiguous names, and TypeSpec/generic layouts still write `0u`.

## RED

Added a generated-C smoke case for a local union:

```zr
pub union Shape {
    Empty;
    Circle(radius: float);
}

pub identity(shape: Shape): Shape {
    var local: Shape = shape;
    return local;
}

var shape: Shape = Shape.Circle(1.0);
```

The test required a generated runtime type-layout descriptor, registry entry, token table, and a nonzero `TYPE_DEF` token entry `0x02000001u`.

RED result: `zr_vm_aot_c_value_type_shared_library_smoke_test` failed because generated C did not emit a `ZrTypeLayout_` descriptor for the union and therefore had no populated token-table entry.

## GREEN

Implementation:
- Split token-table population into `backend_aot_c_type_layout_tokens.c`.
- Exposed the generated table layout resolver from `backend_aot_c_type_layouts.c`.
- Emitted runtime `SZrTypeLayout` descriptors for generated union layouts.
- Populated `zr_aot_type_layout_tokens[]` with a `TYPE_DEF` token when a generated struct/union layout has a unique local TypeDef metadata record with the same type name.
- Preserved `0u` for missing, ambiguous, non-TypeDef, TypeSpec, and generic cases.

## Validation

WSL GCC:
- `zr_vm_metadata_runtime_typespec_layout_test`: `14 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_type_layout_contracts_test`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_source_contracts_test`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_frame_setup_contracts_test`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_shared_library_smoke_test`: `8 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: `3 Tests 0 Failures 0 Ignored`

WSL clang:
- `zr_vm_metadata_runtime_typespec_layout_test`: `14 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_type_layout_contracts_test`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_source_contracts_test`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_frame_setup_contracts_test`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_shared_library_smoke_test`: `8 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_value_type_shared_library_smoke_test`: `3 Tests 0 Failures 0 Ignored`

Windows MSVC Debug:
- `zr_vm_metadata_runtime_typespec_layout_test.exe`: `14 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_type_layout_contracts_test.exe`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_source_contracts_test.exe`: `19 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_frame_setup_contracts_test.exe`: `1 Tests 0 Failures 0 Ignored`
- `zr_vm_aot_c_shared_library_smoke_test.exe`: `0 Tests 0 Failures 8 Ignored`
- `zr_vm_aot_c_value_type_shared_library_smoke_test.exe`: `2 Tests 0 Failures 1 Ignored`

Existing warning observed during WSL clang validation remained non-fatal: generated C still warns on the known logical-not expression shape in an unrelated fixture.

## Acceptance Decision

Accepted for 11-S4P. Generated token tables now carry real TypeDef tokens for the reliable local named-layout subset. Full 11-S4 remains open for TypeSpec/generic token population, persistent cTypeId-to-token indexing, ownership-offset emission, runtime layout construction, and public reflection entity integration.
