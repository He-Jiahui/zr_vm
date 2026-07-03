# AOT 10-S3A Public Reflection Token Resolver Carrier

## Scope

This acceptance record covers one narrow 10-S3 support slice: public token-driven reflection resolution now has a runtime carrier that can be consumed before full reflection object materialization exists.

Affected layers:

- `zr_vm_core/include/zr_vm_core/reflection.h`
  - Exposes `EZrReflectionResolvedTokenKind`.
  - Exposes `SZrReflectionResolvedToken`.
  - Exposes `ZrCore_Reflection_ResolveToken(SZrMetadataRuntime *, TZrMetadataToken, SZrReflectionResolvedToken *)`.
- `zr_vm_core/src/zr_vm_core/reflection_token_resolve.c`
  - Routes TypeDef tokens through `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`.
  - Routes TypeSpec tokens through `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView()`.
  - Routes TypeRef tokens through `ZrCore_MetadataRuntime_ResolveTypeRecord()`.
  - Routes FieldDef tokens through `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView()`.
  - Routes method MemberDef/MemberRef tokens through `ZrCore_MetadataRuntime_ResolveMethodRecord()`.
- `tests/module/test_reflection_token_resolve.c`
  - Provides focused coverage for TypeDef, FieldDef, MethodDef, and invalid inputs.

## Baseline

Before this slice, `docs/plans/aot/10-reflection.md` required token-driven reflection resolution, and the lower 11-S4 metadata binding views already exposed the necessary type and field layout facts. The missing layer was a public reflection-side carrier that joined those metadata views into one API surface.

Initial RED was the new focused test compiling against the intended public API. The build failed because the following symbols did not exist yet:

- `SZrReflectionResolvedToken`
- `ZrCore_Reflection_ResolveToken`
- `ZR_REFLECTION_RESOLVED_TOKEN_TYPE`
- `ZR_REFLECTION_RESOLVED_TOKEN_FIELD`
- `ZR_REFLECTION_RESOLVED_TOKEN_METHOD`

## Accepted Behavior

`ZrCore_Reflection_ResolveToken()` clears the output carrier before resolving and returns false for null runtime, zero token, or null output.

For type tokens:

- TypeDef resolves to kind `ZR_REFLECTION_RESOLVED_TOKEN_TYPE` with the token record, TypeDef row, type layout id, cTypeId, and registry-backed `SZrTypeLayout`.
- TypeSpec resolves to the same type kind through the TypeSpec layout binding view.
- TypeRef resolves as a record-only type entity until a later slice materializes full public reflection objects.

For field tokens:

- FieldDef resolves to kind `ZR_REFLECTION_RESOLVED_TOKEN_FIELD`.
- The carrier exposes the FieldDef row, owner type token, byte offset, field type layout id, owner type layout id, field layout pointer, and owner layout pointer.

For method tokens:

- MethodDef and MethodRef resolve to kind `ZR_REFLECTION_RESOLVED_TOKEN_METHOD` with the method token record.
- A MemberDef token first tries the FieldDef binding view, then falls back to method-record resolution.

## Tooling Evidence

Focused RED/GREEN command:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-gcc >/tmp/zr_config_reflection_token.log && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j2"
```

Focused GREEN executable:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j2 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test"
```

GCC adjacent validation:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test zr_vm_metadata_runtime_typespec_layout_test --clean-first -j2 && ctest --test-dir build-wsl-gcc -R 'reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout' --output-on-failure"
```

Clang adjacent validation:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-clang && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test zr_vm_metadata_runtime_typespec_layout_test --clean-first -j2 && ctest --test-dir build-wsl-clang -R 'reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout' --output-on-failure"
```

Windows MSVC Debug validation:

```text
. C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1
cmake -S . -B build-msvc -G "Visual Studio 17 2022"
cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test zr_vm_metadata_runtime_typespec_layout_test -- /m
ctest --test-dir build-msvc -C Debug -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout" --output-on-failure
```

## Results

Focused RED:

- WSL GCC failed at compile time because the intended reflection token carrier/API did not exist.

Focused GREEN:

- WSL GCC `zr_vm_reflection_token_resolve_test`: 1/0.

Adjacent validation:

- WSL GCC CTest: `metadata_runtime_query`, `reflection_token_resolve`, `metadata_runtime_typespec_layout`, and `metadata_runtime_type_layout` all passed, 4/4.
- WSL Clang CTest: the same set passed, 4/4. Clang still reports existing GNU label-as-value and unused-code warnings.
- Windows MSVC Debug CTest: the same set passed, 4/4. MSVC still reports existing execution-dispatch/object unreachable/unused warnings.

## Remaining Work

This slice does not close full 10-S3 or 10-S4:

- Name lookup is not yet rewritten into name-to-token-to-entity flow.
- Public reflection objects are not materialized from `SZrReflectionResolvedToken`.
- `Method.Invoke` does not yet consume the invoker registry through this token carrier.
- Trim warnings and annotation-driven reflection retention remain open.
- Full TypeSpec/generic layout materialization and cross-module token publication/rewrite remain later 11/12 work.
