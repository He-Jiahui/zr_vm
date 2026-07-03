# AOT 10-S4B Public TypeSpec Generic Argument Reflection Carrier

## Scope

This acceptance record covers one narrow 10-S4 support slice: public reflection token resolution now exposes enough TypeSpec generic metadata for later generic reflection object materialization.

Affected layers:

- `zr_vm_core/include/zr_vm_core/reflection.h`
  - Extends `SZrReflectionResolvedToken` with TypeSpec generic signature/base/argument-count fields.
  - Adds `SZrReflectionResolvedGenericArgument`.
  - Adds `ZrCore_Reflection_ResolveTypeSpecGenericArgument(...)`.
- `zr_vm_core/src/zr_vm_core/reflection_token_resolve.c`
  - Copies TypeSpec generic binding facts from the existing metadata runtime layout binding view.
  - Exposes indexed generic argument facts through `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()`.
- `tests/module/test_reflection_token_resolve.c`
  - Adds focused TypeSpec generic base and argument coverage.

## Baseline

Before this slice, 10-S3A exposed a public token resolver carrier, but TypeSpec results only returned the TypeSpec row and registry-backed layout. The lower metadata runtime already had `ReadTypeSpecGenericBindingView()` and `ReadTypeSpecGenericArgumentView()`, but reflection consumers could not access base token, argument count, or indexed argument token/signature facts through the public reflection API.

Initial RED was the focused reflection token resolver test compiling against the intended public API. The build failed because the following did not exist:

- `SZrReflectionResolvedGenericArgument`
- `SZrReflectionResolvedToken.genericBaseToken`
- `SZrReflectionResolvedToken.genericBaseRecord`
- `SZrReflectionResolvedToken.genericArgumentCount`
- `SZrReflectionResolvedToken.genericSignatureToken`
- `SZrReflectionResolvedToken.genericSignatureHash`
- `ZrCore_Reflection_ResolveTypeSpecGenericArgument(...)`

## Test Inventory

Focused test:

- `tests/module/test_reflection_token_resolve.c`
  - Resolves TypeDef, FieldDef, and MethodDef as covered by 10-S3A.
  - Resolves a TypeSpec generic instance with a TypeRef base.
  - Verifies TypeSpec carrier fields: base token/record, signature token/hash, argument count, and registry-backed layout.
  - Resolves generic argument index 0 as a primitive i64 signature argument with no metadata token.
  - Resolves generic argument index 1 as a direct TypeRef argument token/record.
  - Rejects out-of-range argument index 2.

Adjacent tests:

- `metadata_runtime_query`
- `metadata_runtime_type_layout`
- `metadata_runtime_typespec_layout`

Boundary coverage:

- Null and invalid `ResolveToken` inputs remain covered by the same focused test.
- Primitive generic arguments are represented by signature node kind/payload rather than a token.
- Direct TypeRef/TypeDef generic arguments can expose argument token/record.
- Out-of-range argument index returns false.

## Tooling Evidence

Focused RED:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j2"
```

Focused GREEN:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j2 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test"
```

GCC adjacent validation:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test zr_vm_metadata_runtime_typespec_layout_test -j2 && ctest --test-dir build-wsl-gcc -R 'reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout' --output-on-failure"
```

Clang adjacent validation:

```text
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test zr_vm_metadata_runtime_typespec_layout_test -j2 && ctest --test-dir build-wsl-clang -R 'reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout' --output-on-failure"
```

Windows MSVC Debug validation:

```text
. C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1
cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_type_layout_test zr_vm_metadata_runtime_typespec_layout_test -- /m
ctest --test-dir build-msvc -C Debug -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_type_layout|metadata_runtime_typespec_layout" --output-on-failure
```

## Results

Focused RED:

- WSL GCC failed at compile time on the missing TypeSpec generic carrier fields and indexed argument API.

Focused GREEN:

- WSL GCC `zr_vm_reflection_token_resolve_test`: 2/0.

Adjacent validation:

- WSL GCC CTest: `metadata_runtime_query`, `reflection_token_resolve`, `metadata_runtime_typespec_layout`, and `metadata_runtime_type_layout` all passed, 4/4.
- WSL Clang CTest: the same set passed, 4/4. Clang still reports existing GNU label-as-value and unused-code warnings.
- Windows MSVC Debug CTest: the same set passed, 4/4. MSVC still reports existing execution-dispatch/object warnings.

## Acceptance Decision

Accepted for the narrow 10-S4B carrier slice.

Remaining work:

- Public generic reflection objects are not materialized yet.
- `MakeGenericType` and runtime generic instance construction remain open.
- Recursive generic argument semantic binding remains lower-layer 11 work.
- Cross-module token publication/rewrite remains open.
- Annotation-driven retention and trim diagnostics remain open.
