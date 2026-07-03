---
acceptance: aot-10-s4k-fieldinfo-fielddef-signature-blob
date: 2026-06-30 19:54:58 +08:00
plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
scope:
  - 10-S4K
  - 11-S4Y
  - 12-S5 support
status: completed
---

# AOT 10-S4K FieldInfo FieldDef Signature Blob Coordinates

## Scope

This slice extends the minimum FieldDef token `FieldInfo` object with raw `signatureBlobOffset` and
`signatureBlobLength` integer carriers sourced from the attached zrp `SZrZrpMetadataFieldDefRow`. It does not validate
the referenced signature blob slice, parse field signatures, bind semantic field types from the blob, or change the
code-registration ABI.

## Completed

- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now writes `FieldInfo.signatureBlobOffset` and
  `FieldInfo.signatureBlobLength` from the resolved FieldDef row.
- The focused FieldDef token fixture assigns non-zero signature blob coordinates.
- The focused FieldInfo token test asserts the public object exposes the same raw offset and length.

## RED/GREEN

- RED: after adding the focused `signatureBlobOffset` and `signatureBlobLength` assertions, WSL GCC built the focused
  target but `zr_vm_reflection_token_resolve_test` failed because the fields were still missing.
- GREEN: after filling both fields from the FieldDef row, WSL GCC `zr_vm_reflection_token_resolve_test` passed 8/0.

## Verification

- Initial focused GREEN:
  - WSL GCC `zr_vm_reflection_token_resolve_test`: 8 tests, 0 failures.

- Full focused verification:
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_reflection_token_resolve_test`: 8/0.
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_metadata_runtime_query_test`: 24/0.
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_metadata_runtime_typespec_layout_test`: 17/0.
  - Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: 3/3 on all three
    environments.

## Acceptance Decision

Accepted for the raw FieldDef signature blob coordinate carrier. Remaining work includes validated field signature blob
views, field signature semantic parsing, field type binding from signatures, field value read/write marshaling,
cross-module FieldRef/TypeRef handling, dataflow analysis, DESCRIPTION promotion, and complete metadata sweep.
