# AOT 10-S4Z11 / 11-S4AW / 12-S5 support · FieldInfo primitive POD float32 NaN guard

- Completed at: 2026-07-01 10:11:20 +08:00
- Status: done for the focused FieldInfo primitive POD float32 NaN raw inline write guard.
- Scope: `ZrCore_Reflection_WriteFieldInfoTokenValue()` now rejects NaN `DOUBLE` sources before casting to `TZrFloat32` and before copying bytes into raw primitive POD float32 field storage.
- RED: Windows MSVC Debug focused `zr_vm_reflection_token_resolve_test` built and ran 19 tests, then failed the new NaN write case with `Expected FALSE Was TRUE`.
- GREEN: Windows MSVC Debug focused `zr_vm_reflection_token_resolve_test` passed 19/0 after the guard.
- Validation: WSL GCC, WSL Clang, and Windows MSVC Debug direct runs passed `zr_vm_reflection_token_resolve_test` 19/0, `zr_vm_metadata_runtime_query_test` 24/0, and `zr_vm_metadata_runtime_typespec_layout_test` 17/0.
- CTest: focused `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed 3/3 on WSL GCC, WSL Clang, and Windows MSVC Debug.
- Still open: float32 precision/narrowing policy, nested inline field marshaling, object-level FieldInfo methods, cross-module provider compatibility, complete signature-derived field type binding, `@dynamically_accessed` dataflow, DESCRIPTION promotion, complete trim analyzer, and full metadata sweep/pruning.
