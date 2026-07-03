# AOT 10-S4Z12 / 11-S4AX / 12-S5 support · FieldInfo primitive POD float32 precision guard

- Completed at: 2026-07-01 10:21:28 +08:00
- Status: done for the focused FieldInfo primitive POD float32 precision/no-loss raw inline write guard.
- Scope: `ZrCore_Reflection_WriteFieldInfoTokenValue()` now rejects `DOUBLE` sources that cannot round-trip losslessly through `TZrFloat32` before casting and before copying bytes into raw primitive POD float32 field storage.
- RED: Windows MSVC Debug focused `zr_vm_reflection_token_resolve_test` built and ran 20 tests, then failed the new precision-loss write case with `Expected FALSE Was TRUE`.
- GREEN: Windows MSVC Debug focused `zr_vm_reflection_token_resolve_test` passed 20/0 after the guard.
- Validation: WSL GCC, WSL Clang, and Windows MSVC Debug direct runs passed `zr_vm_reflection_token_resolve_test` 20/0, `zr_vm_metadata_runtime_query_test` 24/0, and `zr_vm_metadata_runtime_typespec_layout_test` 17/0.
- CTest: focused `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed 3/3 on WSL GCC, WSL Clang, and Windows MSVC Debug.
- Still open: nested inline field marshaling, object-level FieldInfo methods, cross-module provider compatibility, complete signature-derived field type binding, `@dynamically_accessed` dataflow, DESCRIPTION promotion, complete trim analyzer, and full metadata sweep/pruning.
