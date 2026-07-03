---
acceptance: aot-10-s4j-fieldinfo-fielddef-flags
date: 2026-06-30 19:39:14 +08:00
plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
scope:
  - 10-S4J
  - 11-S4X
  - 12-S5 support
status: completed
---

# AOT 10-S4J FieldInfo FieldDef Flags

## Scope

This slice extends the minimum FieldDef token `FieldInfo` object with a raw `metadataFlags` integer carrier sourced from
the attached zrp `SZrZrpMetadataFieldDefRow.flags`. It does not interpret flag bits, change `isStatic`/`isConst`, add
new metadata rows, or change the code-registration ABI.

## Completed

- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now writes `FieldInfo.metadataFlags` from `resolved.fieldDefRow->flags`.
- The focused FieldDef token fixture assigns a non-zero FieldDef row flags value.
- The focused FieldInfo token test asserts the public object exposes the same raw value as `metadataFlags`.

## RED/GREEN

- RED: after adding the focused `metadataFlags` assertion, WSL GCC built the focused target but
  `zr_vm_reflection_token_resolve_test` failed because the field was still missing.
- GREEN: after filling `metadataFlags` from the FieldDef row, WSL GCC
  `zr_vm_reflection_token_resolve_test` passed 8/0.

## Verification

- Initial focused GREEN:
  - WSL GCC `zr_vm_reflection_token_resolve_test`: 8 tests, 0 failures.

- Full focused verification:
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_reflection_token_resolve_test`: 8/0.
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_metadata_runtime_query_test`: 24/0.
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_metadata_runtime_typespec_layout_test`: 17/0.
  - Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: 3/3 on all three
    environments.
