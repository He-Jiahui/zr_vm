# AOT 10-S4Y FieldInfo Signature TypeDef Child Node Semantic Binding

## Scope

- Plan slices: `10-S4Y / 11-S4AM / 12-S5 support`.
- Goal: let recursive `FieldInfo.fieldTypeSignatureNodeObject` direct `TYPE_DEF` base/child nodes carry semantic token, layout, size, and name data.
- Non-goals: direct `TYPE_REF` child binding, cross-module provider loading, full recursive semantic field type materialization, field value read/write, complete `FieldInfo` methods, trim analyzer policy, and metadata sweep completion.

## RED

- Test: `tests/module/test_reflection_token_resolve.c`.
- Fixture: extended the generic FieldInfo signature from `GENERIC_INST(TYPE_DEF(object, 17), int64)` to `GENERIC_INST(TYPE_DEF(object, 17), int64, TYPE_DEF(object, 17))`, with a matching standalone direct TypeDef signature blob.
- New assertions: `baseTypeNodeObject` and `childNodeObjects[1]` must expose `TEST_FIELD_TYPE_DEF_TOKEN`, layout id `42`, size `16`, and type name `int`.
- WSL GCC failure before implementation: `zr_vm_reflection_token_resolve_test` failed 1/11 with `Expected 33554434 Was 0`, proving the nested direct TypeDef node still had `typeToken == 0`.

## GREEN

- `reflection_build_signature_type_node_object_internal()` now keeps the metadata runtime context for recursive signature node objects.
- Recursive direct `TYPE_DEF` nodes match existing signature token records, resolve the matched TypeDef token through `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()`, and read the TypeDef row name through `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()`.
- Primitive child semantic naming from 10-S4X remains intact; direct TypeDef base and child nodes now carry the same token/layout/name fields without changing the top-level generic node carrier.

## Verification

- WSL GCC: `zr_vm_reflection_token_resolve_test` 11/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0, focused CTest 3/3.
- WSL Clang: `zr_vm_reflection_token_resolve_test` 11/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0, focused CTest 3/3.
- Windows MSVC Debug: `zr_vm_reflection_token_resolve_test` 11/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0, focused CTest 3/3.
- Known residual warning: WSL Clang still reports the existing `reflection.c` `callerName` unused warning.

## Status

- Completed at `2026-07-01 02:47:33 +08:00`.
- Recorded in `docs/plans/aot/10-reflection.md`, `docs/plans/aot/11-metadata.md`, `docs/plans/aot/12-code-stripping.md`, `docs/plans/aot/index.md`, and `docs/module-system/typed-module-metadata.md`.
