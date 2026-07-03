# AOT 10-S4Z2 FieldInfo Signature TypeRef Child Node Semantic Binding

## Scope

- Plan slices: `10-S4Z2 / 11-S4AN / 12-S5 support`.
- Goal: let recursive `FieldInfo.fieldTypeSignatureNodeObject` direct `TYPE_REF` child nodes carry semantic token, layout, size, and name data.
- Non-goals: cross-module provider loading, provider version compatibility, full recursive semantic field type materialization, field value read/write, complete `FieldInfo` methods, trim analyzer policy, and metadata sweep completion.

## RED

- Test: `tests/module/test_reflection_token_resolve.c`.
- Fixture: extended the generic FieldInfo signature to `GENERIC_INST(TYPE_DEF(object, 17), int64, TYPE_DEF(object, 17), TYPE_REF(object, 23))`, with a matching module TypeRef signature blob and target TypeDef layout binding.
- New assertions: `childNodeObjects[2]` must expose `TEST_TYPE_REF_TOKEN`, layout id `42`, size `16`, and type name `int`.
- WSL GCC failure before implementation: `zr_vm_reflection_token_resolve_test` failed 1/11 with `Expected 83886081 Was 0`, proving the nested TypeRef node still had `typeToken == 0`.

## GREEN

- `reflection_build_signature_type_node_object_internal()` now handles recursive `TYPE_REF` nodes through the existing signature token record matcher.
- Recursive TypeRef nodes resolve layout through `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()`, preserving TypeRef token identity while using the bound target TypeDef layout.
- Recursive TypeRef nodes read the target TypeDef row name through `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` and expose it as `typeName`.

## Verification

- WSL GCC: `zr_vm_reflection_token_resolve_test` 11/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0, focused CTest 3/3.
- WSL Clang: `zr_vm_reflection_token_resolve_test` 11/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0, focused CTest 3/3.
- Windows MSVC Debug: `zr_vm_reflection_token_resolve_test` 11/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0, focused CTest 3/3.
- Known residual warning: WSL Clang still reports the existing `reflection.c` `callerName` unused warning.

## Status

- Completed at `2026-07-01 03:04:36 +08:00`.
- Recorded in `docs/plans/aot/10-reflection.md`, `docs/plans/aot/11-metadata.md`, `docs/plans/aot/12-code-stripping.md`, `docs/plans/aot/index.md`, and `docs/module-system/typed-module-metadata.md`.
