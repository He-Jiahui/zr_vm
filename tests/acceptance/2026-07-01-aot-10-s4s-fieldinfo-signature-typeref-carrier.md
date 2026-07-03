# AOT 10-S4S FieldInfo Bound TypeRef Signature Carrier

## Scope
- Changed the minimum public `FieldInfo` object for a FieldDef token so a direct bound `TYPE_REF` field signature node can expose the TypeRef token/layout/size carrier and materialize the target TypeDef name/object.
- Affected layers: core reflection, metadata runtime TypeRef layout consumer path, focused module tests, AOT 10/11/12 plan records.

## Baseline
- 11-S4S / 10-S5N already let `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` resolve an attached bound `TYPE_REF` token to a current-runtime target `TYPE_DEF` layout when identity checks pass.
- 10-S4Q and 10-S4R exposed direct local `TYPE_DEF` field signature identity as token/layout/size and then as `fieldTypeSignatureTypeName` plus `fieldTypeSignatureType`.
- Before this slice, the FieldInfo TypeRef signature path could match the TypeRef token and layout, but did not read the target TypeDef row name, so the signature-derived type name/object remained missing.
- Repository-level broad validation remains outside this focused slice; the known existing clang warning in `reflection.c` for `callerName` remains unrelated.

## Test Inventory
- `tests/module/test_reflection_token_resolve.c`
  - Added `test_reflection_builds_field_info_signature_typeref_carrier`.
  - The fixture builds `FIELD_SIG(TYPE_REF(object, 23))`, a matching attached module `TYPE_REF` token record, a paired TypeRef signature blob, and a target local `TYPE_DEF` layout binding.
  - The test asserts `fieldTypeSignatureTypeToken == TEST_TYPE_REF_TOKEN`, `fieldTypeSignatureTypeLayoutId == 42`, `fieldTypeSignatureTypeSize == 16`, `fieldTypeSignatureTypeName == "int"`, and `fieldTypeSignatureType.kind/name/qualifiedName == type/int/int`.
- Boundary and negative coverage:
  - Existing TypeDef and primitive FieldInfo coverage remains intact.
  - This slice deliberately does not claim cross-module provider loading, recursive wrapper/generic type-node objects, field type consistency checks, or field read/write behavior.

## Tooling Evidence
- WSL GCC RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
  - Observed failure: `test_reflection_builds_field_info_signature_typeref_carrier` expected `fieldTypeSignatureTypeName` as a string but saw null because the TypeRef path did not read the target TypeDef name.
- WSL GCC GREEN and focused regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test && ctest --test-dir build-wsl-gcc -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure'`
- WSL Clang focused regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_typespec_layout_test && ctest --test-dir build-wsl-clang -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure'`
- Windows MSVC Debug focused regression:
  - `cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test --parallel 8`
  - Followed by the three Debug test binaries and focused CTest with the same regex.

## Results
- WSL GCC:
  - `zr_vm_reflection_token_resolve_test`: 10 tests, 0 failures.
  - `zr_vm_metadata_runtime_query_test`: 24 tests, 0 failures.
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17 tests, 0 failures.
  - Focused CTest: 3/3 passed.
- WSL Clang:
  - Same three test binaries passed 10/0, 24/0, and 17/0.
  - Focused CTest: 3/3 passed.
  - Existing `reflection.c` `callerName` unused warning remains.
- Windows MSVC Debug:
  - Same three test binaries passed 10/0, 24/0, and 17/0.
  - Focused CTest: 3/3 passed.

## Acceptance Decision
- Accepted for 10-S4S / 11-S4AG / 12-S5 support.
- The slice closes current-runtime bound `TYPE_REF` field signature identity for minimum FieldDef token `FieldInfo`, including token/layout/size and target TypeDef name/object materialization.
- Remaining work: cross-module TypeRef/provider signature binding, recursive type-node reflection objects, field type consistency checks, field value read/write, complete `FieldInfo` methods, `@dynamically_accessed` dataflow, DESCRIPTION promotion, complete trim analyzer, and full metadata sweep.
