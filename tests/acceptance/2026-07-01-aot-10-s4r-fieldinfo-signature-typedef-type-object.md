# AOT 10-S4R FieldInfo Direct TypeDef Signature Type Object

## Scope
- Changed the minimum public `FieldInfo` object for a FieldDef token so a direct local `TYPE_DEF` field signature node can materialize `fieldTypeSignatureType` as a type reflection object.
- Affected layers: core reflection, metadata runtime consumers, focused module tests, AOT 10/11/12 plan records.

## Baseline
- 10-S4Q / 11-S4AE already exposed direct local `TYPE_DEF` signature identity as `fieldTypeSignatureTypeToken`, `fieldTypeSignatureTypeLayoutId`, and `fieldTypeSignatureTypeSize`.
- Before this slice, `fieldTypeSignatureType` was only built for primitive signature nodes. The direct `TYPE_DEF` path still left that object field null.
- Repository-level broad validation remains outside this focused slice; the known existing clang warning in `reflection.c` for `callerName` remains unrelated.

## Test Inventory
- `tests/module/test_reflection_token_resolve.c`
  - `test_reflection_builds_field_info_signature_typedef_carrier` now asserts `fieldTypeSignatureTypeName == "int"`.
  - The same test asserts `fieldTypeSignatureType` is a reflection object with `kind == "type"`, `name == "int"`, and `qualifiedName == "int"`.
- Boundary and negative coverage:
  - Existing primitive FieldInfo coverage still asserts primitive signature type object materialization as `bool`.
  - This slice deliberately does not claim `TYPE_REF`, cross-module provider lookup, recursive wrappers, generic-inst type objects, or field read/write behavior.

## Tooling Evidence
- WSL GCC RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
  - Observed failure: `test_reflection_builds_field_info_signature_typedef_carrier` expected object value type but saw null for `fieldTypeSignatureType`.
- WSL GCC GREEN and focused regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test && ctest --test-dir build-wsl-gcc -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure'`
- WSL Clang focused regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_typespec_layout_test && ctest --test-dir build-wsl-clang -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure'`
- Windows MSVC Debug focused regression:
  - `cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test --parallel 8`
  - Followed by the three Debug test binaries and focused CTest with the same regex.

## Results
- WSL GCC:
  - `zr_vm_reflection_token_resolve_test`: 9 tests, 0 failures.
  - `zr_vm_metadata_runtime_query_test`: 24 tests, 0 failures.
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17 tests, 0 failures.
  - Focused CTest: 3/3 passed.
- WSL Clang:
  - Same three test binaries passed 9/0, 24/0, and 17/0.
  - Focused CTest: 3/3 passed.
  - Existing `reflection.c` `callerName` unused warning remains.
- Windows MSVC Debug:
  - Same three test binaries passed 9/0, 24/0, and 17/0.
  - Focused CTest: 3/3 passed.

## Acceptance Decision
- Accepted for 10-S4R / 11-S4AF / 12-S5 support.
- The slice closes direct local `TYPE_DEF` signature type object materialization for minimum FieldDef token `FieldInfo`.
- Remaining work: `TYPE_REF` and cross-module signature binding, recursive type-node reflection objects, field type consistency checks, field value read/write, complete `FieldInfo` methods, `@dynamically_accessed` dataflow, DESCRIPTION promotion, complete trim analyzer, and full metadata sweep.
