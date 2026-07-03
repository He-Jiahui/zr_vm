# AOT 10-S4C FieldDef Public Reflection Carrier

## Scope

This acceptance record covers one narrow 10-S4 / 11-S4 consumer slice: public FieldDef token resolution now carries enough owner/type identity for later public `FieldInfo` materialization.

Affected layers:

- `zr_vm_core/include/zr_vm_core/reflection.h`
  - Extends `SZrReflectionResolvedToken` FieldDef results with owner type record/row and field type token/record fields.
- `zr_vm_core/src/zr_vm_core/reflection_token_resolve.c`
  - Copies owner type metadata from `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView()`.
  - Resolves the field type token through `ZrCore_MetadataRuntime_ResolveTypeLayoutToken()`.
- `tests/module/test_reflection_token_resolve.c`
  - Extends the synthetic FieldDef metadata fixture with a field-type TypeDef row and record.

## Baseline

Before this slice, `ZrCore_Reflection_ResolveToken()` could resolve FieldDef tokens to a field row, owner token, byte offset, field type layout id, owner layout id, and registry-backed layouts. It did not expose the owner type record/row through the public carrier, and it did not map the field type layout id back to a TypeDef/TypeSpec token and record.

Initial RED was the focused reflection token resolver test compiling against the intended public carrier. The build failed because the following `SZrReflectionResolvedToken` fields did not exist:

- `ownerTypeRecord`
- `ownerTypeDefRow`
- `fieldTypeToken`
- `fieldTypeRecord`

## Test Inventory

Focused test:

- `tests/module/test_reflection_token_resolve.c`
  - Resolves TypeDef, FieldDef, and MethodDef tokens.
  - Verifies FieldDef owner token, owner type record, owner TypeDef row, field byte offset, field type layout, owner layout, field type token, and field type record.
  - Keeps TypeSpec generic base/argument coverage from 10-S4B.
  - Keeps invalid/null `ResolveToken` input coverage.

Adjacent tests:

- `metadata_runtime_query`
- `metadata_runtime_type_layout`
- `metadata_runtime_typespec_layout`

Boundary coverage:

- Field type token resolution uses layout id -> TypeDef/TypeSpec token lookup and does not add a reflection-layer token table.
- Null runtime, zero token, null output, and unsupported token cases still clear/return false through the existing resolver path.
- TypeSpec generic argument out-of-range rejection remains covered in the same focused test.

## Tooling Evidence

Focused RED:

```text
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8"
```

Focused GREEN:

```text
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test"
```

GCC adjacent validation:

```text
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R 'reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout' --output-on-failure --parallel 4"
```

Clang adjacent validation:

```text
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test -j 8 && ctest --test-dir build-wsl-clang -R 'reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout' --output-on-failure --parallel 4"
```

Windows MSVC Debug validation:

```text
. C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1
cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test --parallel 8
ctest --test-dir build-msvc -C Debug -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout" --output-on-failure --parallel 4
```

## Results

Focused RED:

- WSL GCC failed at compile time on missing `ownerTypeRecord`, `ownerTypeDefRow`, `fieldTypeToken`, and `fieldTypeRecord`.

Focused GREEN:

- WSL GCC `zr_vm_reflection_token_resolve_test`: 2/0.

Adjacent validation:

- WSL GCC CTest: `metadata_runtime_query`, `reflection_token_resolve`, `metadata_runtime_typespec_layout`, and `metadata_runtime_type_layout` all passed, 4/4.
- WSL Clang CTest: the same set passed, 4/4. Existing GNU label-as-value and unused-code warnings remain.
- Windows MSVC Debug CTest: the same set passed, 4/4. Existing execution-dispatch/object warnings remain.

## Acceptance Decision

Accepted for the narrow 10-S4C FieldDef public carrier slice.

Remaining work:

- Public `FieldInfo` objects are not materialized yet.
- Field value get/set reflection marshaling remains open.
- Union owner offsets remain open.
- Cross-module token publication/rewrite remains open.
- Annotation-driven retention and trim diagnostics remain open.
