# AOT 10-S5K / 12-S5J Dynamic Dependency Type Token Root

Completed: 2026-06-30 16:58:17 +08:00

## Scope

This slice accepts a narrow current-module carrier for preserving generated type-layout metadata during opt-in AOT C code stripping.

`dynamicDependencyTypeToken` in function decorator metadata is interpreted as a uint32 metadata token. Only current-module `TYPE_DEF` and `TYPE_SPEC` tokens are accepted. The token is resolved through embedded zrp TypeDef/TypeSpec rows to a unique non-none `typeLayoutId`; that layout id then joins the same dynamic dependency type-layout root set used by `dynamicDependencyTypeLayoutId`.

Out of scope: FieldDef tokens, TypeRef and cross-module type tokens, field dependency semantics, `@dynamically_accessed` dataflow, warning policy, DESCRIPTION promotion, and complete metadata sweep.

## RED

Added `test_aot_c_code_stripping_preserves_dynamic_dependency_type_token_layout_metadata`.

Before implementation, WSL GCC focused `zr_vm_aot_c_code_stripping_test` ran with the new TypeDef token fixture failing `Expected Non-NULL` because `ZrTypeLayout_2` was removed together with the otherwise unreachable function that originally referenced it.

## Implementation

- `backend_aot_c_emitter.c` now passes the embedded metadata blob into dynamic type-layout root collection and type-layout token table emission.
- `backend_aot_c_type_layouts.{h,c}` now resolves `dynamicDependencyTypeToken` values through embedded zrp metadata, accepting only `TYPE_DEF` and `TYPE_SPEC` tables.
- TypeDef and TypeSpec resolution requires exactly one matching row with a non-none `typeLayoutId`, and the collector verifies a generated layout resolver exists before appending the root.
- `backend_aot_c_type_layout_tokens.c` now uses the metadata blob as a fallback for root-only layouts so `zr_aot_type_layout_tokens[]` can retain the originating `TYPE_DEF` or `TYPE_SPEC` token after trimming.

## Verification

- WSL GCC: code stripping 8/0, source contracts 24/0, type-layout contracts 1/0.
- WSL GCC smoke: global shared-library 10/0, call shared-library 5/0, dynamic deopt bridge 7/0.
- WSL GCC CTest: `aot_c_code_stripping|aot_c_type_layout_contracts` matched 2 tests, passed 2/2.
- WSL Clang: code stripping 8/0, source contracts 24/0, type-layout contracts 1/0.
- WSL Clang smoke: global shared-library 10/0, call shared-library 5/0, dynamic deopt bridge 7/0.
- WSL Clang CTest: `aot_c_code_stripping|aot_c_type_layout_contracts` matched 2 tests, passed 2/2.
- Windows MSVC Debug: code stripping 8/0, source contracts 24/0, type-layout contracts 1/0.
- Windows MSVC Debug CTest: `aot_c_code_stripping|aot_c_type_layout_contracts` matched 2 tests, passed 2/2.
- Windows smoke binaries returned OK with Unix-only assertions ignored: global 10 ignored, call 5 ignored, dynamic deopt bridge 7 ignored, 0 failures.
- `git diff --check` exited 0 with only LF/CRLF conversion warnings.

## Decision

Accepted for the current-module TypeDef/TypeSpec token-to-typeLayoutId dynamic dependency carrier.

This does not close full 10-S5 / 12-S5 annotation dependency handling. Field dependencies, TypeRef/cross-module type tokens, cross-module rules, dataflow annotations, warning policy, DESCRIPTION promotion, and the complete trim analyzer remain open.
