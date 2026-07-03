# AOT 10-S5L / 12-S5K Dynamic Dependency Field Token Layout Roots

Completed: 2026-06-30 17:31:53 +08:00

## Scope

This slice accepts a narrow current-module carrier for preserving generated type-layout metadata during opt-in AOT C code stripping.

`dynamicDependencyFieldToken` in function decorator metadata is interpreted as a uint32 metadata token. Only current-module `MEMBER_DEF` FieldDef tokens are accepted. The token is resolved through embedded zrp FieldDef rows, then checked against the owner TypeDef row and that owner's field range. The owner TypeDef `typeLayoutId` and the FieldDef `typeLayoutId` join the same dynamic dependency type-layout root set used by `dynamicDependencyTypeLayoutId` and `dynamicDependencyTypeToken`.

Out of scope: FieldInfo object materialization, field value read/write, TypeRef and cross-module type tokens, cross-module dependency rules, `@dynamically_accessed` dataflow, warning policy, DESCRIPTION promotion, and complete metadata sweep.

## RED

Added `test_aot_c_code_stripping_preserves_dynamic_dependency_field_token_layout_metadata`.

Before implementation, WSL GCC focused `zr_vm_aot_c_code_stripping_test` ran with the new FieldDef token fixture failing `Expected Non-NULL` because the field value layout was removed together with the otherwise unreachable function that originally referenced it.

## Implementation

- Added `backend_aot_c_type_layout_metadata_roots.{h,c}` to own metadata-token to type-layout-root scanning.
- Moved TypeDef/TypeSpec dynamic dependency token lookup into the helper module and kept `backend_aot_c_type_layouts.c` focused on root collection orchestration.
- Added FieldDef handling for `dynamicDependencyFieldToken`, accepting only `MEMBER_DEF` tokens, matching embedded zrp FieldDef rows, verifying the owner TypeDef row and owner field range, and returning owner plus field type-layout roots.
- The collector verifies that each returned root has a generated layout resolver before appending it to the retained type-layout root set.

## Verification

- WSL GCC: code stripping 9/0, source contracts 24/0, type-layout contracts 1/0.
- WSL GCC smoke: global shared-library 10/0, call shared-library 5/0, dynamic deopt bridge 7/0.
- WSL GCC CTest: `aot_c_code_stripping|aot_c_type_layout_contracts` matched 2 tests, passed 2/2.
- WSL Clang: code stripping 9/0, source contracts 24/0, type-layout contracts 1/0.
- WSL Clang smoke: global shared-library 10/0, call shared-library 5/0, dynamic deopt bridge 7/0.
- WSL Clang CTest: `aot_c_code_stripping|aot_c_type_layout_contracts` matched 2 tests, passed 2/2.
- Windows MSVC Debug: code stripping 9/0, source contracts 24/0, type-layout contracts 1/0.
- Windows MSVC Debug CTest: `aot_c_code_stripping|aot_c_type_layout_contracts` matched 2 tests, passed 2/2.
- Windows smoke binaries returned OK with Unix-only assertions ignored: global 10 ignored, call 5 ignored, dynamic deopt bridge 7 ignored, 0 failures.
- `git diff --check` exited 0 with only LF/CRLF conversion warnings.

## Decision

Accepted for the current-module FieldDef token-to-owner/field-typeLayoutId dynamic dependency carrier.

This does not close full 10-S5 / 12-S5 annotation dependency handling. FieldInfo object materialization, field value read/write, TypeRef/cross-module type tokens, cross-module rules, dataflow annotations, warning policy, DESCRIPTION promotion, and the complete trim analyzer remain open.
