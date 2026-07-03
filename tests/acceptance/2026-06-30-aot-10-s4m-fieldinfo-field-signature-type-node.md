# AOT 10-S4M FieldInfo Field Signature Type Node Carrier

## Scope
- Changed the minimum FieldDef token `FieldInfo` reflection object to expose the validated field signature type-node summary from the FieldDef `FIELD_SIG` blob.
- Affected layers: runtime reflection consumer, metadata runtime signature view consumer, module focused tests, AOT 10/11/12 plan documentation.
- This slice consumes existing 11-S3I `ZrCore_MetadataRuntime_ReadSignatureTypeNode()` support. It does not add metadata rows, change code-registration ABI, construct recursive type-node reflection objects, or bind the signature node to a semantic field type.

## Baseline
- 10-S4L / 11-S4Z already exposed the validated `FIELD_SIG` header as `signatureRootNode`, `signatureFlags`, and `fieldTypeBlobOffset`.
- The same FieldInfo object still did not expose the type-node at `fieldTypeBlobOffset`, so consumers could see the header but not the primitive/type/ref/generic node summary.
- Repository-wide unrelated changes and untracked files already exist in this checkout; this acceptance only covers the focused FieldInfo/signature view path.

## Test Inventory
- `tests/module/test_reflection_token_resolve.c`
  - Adds assertions for `fieldTypeSignatureNode`, `fieldTypeSignatureBlobOffset`, `fieldTypeSignatureNextBlobOffset`, `fieldTypeSignaturePayload0`, `fieldTypeSignaturePayload1`, `fieldTypeSignatureBaseTypeBlobOffset`, `fieldTypeSignatureChildCount`, and `fieldTypeSignatureChildListBlobOffset`.
  - The fixture uses a valid `FIELD_SIG` blob whose field type node is `PRIMITIVE(BOOL)` at blob offset `2`.
- Existing supporting tests retained:
  - `tests/module/test_metadata_runtime_query.c` validates signature blob/header/type-node views at the metadata runtime layer.
  - `tests/module/test_metadata_runtime_typespec_layout.c` guards adjacent TypeSpec/layout metadata runtime paths.
- Boundary and failure behavior:
  - Missing, invalid, or non-`FIELD_SIG` signatures are represented by zero-valued FieldInfo type-node summary fields.
  - Full semantic field-type binding remains out of scope.

## Tooling Evidence
- RED command:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'
```

- RED result: build succeeded, then `test_reflection_builds_field_info_object_from_fielddef_token` failed with `Expected Non-NULL`; total `8 Tests 1 Failures 0 Ignored`.
- GREEN and regression commands:

```bash
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test && ctest --test-dir build-wsl-gcc -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_typespec_layout_test && ctest --test-dir build-wsl-clang -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure'
cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test --parallel 8; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; .\build-msvc\bin\Debug\zr_vm_reflection_token_resolve_test.exe; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; .\build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; .\build-msvc\bin\Debug\zr_vm_metadata_runtime_typespec_layout_test.exe; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; ctest --test-dir build-msvc -C Debug -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure
```

## Results
- WSL GCC:
  - `zr_vm_reflection_token_resolve_test`: `8 Tests 0 Failures 0 Ignored`
  - `zr_vm_metadata_runtime_query_test`: `24 Tests 0 Failures 0 Ignored`
  - `zr_vm_metadata_runtime_typespec_layout_test`: `17 Tests 0 Failures 0 Ignored`
  - Focused CTest: `3/3` passed
- WSL Clang:
  - Same test counts and focused CTest `3/3` passed
  - Existing `callerName` unused warning in `reflection.c` remains pre-existing and unrelated to this field signature type-node carrier.
- Windows MSVC Debug:
  - Same test counts and focused CTest `3/3` passed
  - An initial MSVC local uninitialized-view warning was fixed by zero-initializing the signature views before the final accepted run.

## Acceptance Decision
- Accepted for 10-S4M / 11-S4AA / 12-S5 support.
- The FieldInfo object now exposes the validated field signature type-node summary while preserving zero fallback for absent or invalid views.
- Remaining risks and follow-ups: signature-derived semantic field type binding, recursive type-node reflection objects, field value read/write, complete FieldInfo method surface, cross-module FieldRef/TypeRef handling, `@dynamically_accessed` dataflow, DESCRIPTION promotion, full trim analyzer, and complete metadata sweep/pruning.
