# AOT 10-S4W / 11-S4AK / 12-S5 Support Acceptance: FieldInfo Signature Child Type-Node Objects

## Scope
- Time: 2026-07-01 02:00:10 +08:00
- Plans: `docs/plans/aot/10-reflection.md`, `docs/plans/aot/11-metadata.md`, `docs/plans/aot/12-code-stripping.md`
- Scope: runtime reflection now exposes `fieldTypeSignatureNodeObject.childNodeObjects` for FieldInfo signature nodes
  with a structural child list, starting with generic field signatures.
- Layers: core runtime reflection, metadata runtime signature blob consumption, focused module tests, AOT plan docs.

## Baseline
- Previous slice 10-S4V / 11-S4AJ exposed `baseTypeNodeObject` for
  `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64))`.
- The generic argument node remained available only as raw `childCount` / `childListBlobOffset`; the public nested
  signature node object did not expose a child node object list.

## Test Inventory
- Focused subsystem test: `tests/module/test_reflection_token_resolve.c`
  `test_reflection_builds_field_info_signature_generic_base_type_node_object`.
- Boundary covered in this slice: one generic argument child read from `childListBlobOffset` after a direct base
  `TYPE_DEF` node.
- Negative/remaining cases: malformed child lists, recursive semantic token/layout binding, multiple children,
  nested generic argument semantic binding, cross-module TypeRef provider loading, field read/write, and complete
  FieldInfo methods remain later work.

## Tooling Evidence
- WSL GCC focused RED/GREEN command:
  `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
- RED result: 11 tests, 1 failure. The new child list assertion failed with `Expected Non-NULL` because
  `childNodeObjects` was missing.
- GREEN result: 11 tests, 0 failures. The generic FieldInfo fixture exposes one `childNodeObjects[0]`
  `signatureTypeNode` for `PRIMITIVE(INT64)` at blob offset 16.
- Final WSL GCC and WSL Clang focused matrix:
  `reflection_token_resolve` 11/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0, and focused
  CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3.
- Final Windows MSVC Debug focused matrix:
  `reflection_token_resolve` 11/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0, and focused
  CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3.
- Known residual warning: WSL Clang still reports the pre-existing `reflection.c` `callerName` unused warning.

## Results
- `reflection_build_signature_type_node_object_internal()` now creates `childNodeObjects` as an array and, when the
  signature node view has a child list, reads each child node from the same validated signature blob.
- Each child node is materialized as a structural `signatureTypeNode` object with node/blob/payload/base/child summary
  fields. The slice intentionally leaves child semantic token/layout/name fields empty.

## Acceptance Decision
- Accepted for the structural FieldInfo signature child type-node object-list carrier.
- Remaining risks are limited to later planned work: semantic generic argument binding, recursive field type binding,
  TypeRef/cross-module provider loading, field value read/write, complete FieldInfo methods, dataflow analysis,
  DESCRIPTION promotion, and full metadata sweep.
