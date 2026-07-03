---
acceptance: aot-10-s4l-fieldinfo-field-signature-header
date: 2026-06-30 20:13:27 +08:00
plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
scope:
  - 10-S4L
  - 11-S4Z
  - 12-S5 support
status: completed
---

# AOT 10-S4L FieldInfo Field Signature Header

## Scope

This slice extends the minimum FieldDef token `FieldInfo` object with validated field signature header carriers sourced
through `ZrCore_MetadataRuntime_ReadSignatureView()`. It keeps the existing raw FieldDef row
`signatureBlobOffset/signatureBlobLength` fields and adds public `signatureRootNode`, `signatureFlags`, and
`fieldTypeBlobOffset` values only when the paired signature record resolves to a valid `FIELD_SIG` blob.

It does not bind the field type semantically from the signature, materialize recursive type nodes as reflection objects,
implement field value read/write marshaling, or change metadata/code-registration ABI.

## Completed

- The FieldDef token fixture now includes a signature blob pool with a valid `FIELD_SIG` slice at raw coordinates `4/7`.
- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now reads the existing metadata runtime signature view for the
  FieldDef token and exposes:
  - `FieldInfo.signatureRootNode`
  - `FieldInfo.signatureFlags`
  - `FieldInfo.fieldTypeBlobOffset`
- Missing, invalid, or non-field signature views leave the public validated signature header fields at `0`.

## RED/GREEN

- RED: after adding the focused `signatureRootNode`, `signatureFlags`, and `fieldTypeBlobOffset` assertions, WSL GCC
  built the focused target but `zr_vm_reflection_token_resolve_test` failed because those fields were still missing.
- GREEN: after wiring `FieldInfo` to `ZrCore_MetadataRuntime_ReadSignatureView()`, WSL GCC
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

## Acceptance Decision

Accepted for the validated field signature header carrier on the minimum FieldDef token `FieldInfo` object. Remaining
work includes signature-derived field type binding, recursive type-node reflection objects, field value read/write
marshaling, complete `FieldInfo` methods, cross-module FieldRef/TypeRef handling, dataflow analysis, DESCRIPTION
promotion, and complete metadata sweep.
