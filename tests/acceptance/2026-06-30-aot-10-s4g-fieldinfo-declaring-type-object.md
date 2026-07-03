---
acceptance: aot-10-s4g-fieldinfo-declaring-type-object
date: 2026-06-30 18:58:39 +08:00
plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
scope:
  - 10-S4G
  - 11-S4U
  - 12-S5 support
status: completed
---

# AOT 10-S4G FieldInfo Declaring Type Object

## Scope

This slice extends the minimum FieldDef token `FieldInfo` object from 10-S4F/11-S4T. It does not add a new metadata row or
code-registration ABI. The public object now carries owner/declaring type names and a nested declaring type literal sourced
from the owner TypeDef row's zrp string pool.

## Completed

- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now sets `ownerTypeName` and `declaringTypeName`.
- The same builder creates a nested `declaringType` reflection object with `kind = "type"`, `name = ownerTypeName`, and
  `qualifiedName = ownerTypeName`.
- The focused FieldDef token test now also asserts the existing field `type` nested object shape.
- No field value read/write marshaling, owner/module full reflection object link, cache policy, cross-module FieldRef/TypeRef,
  dataflow analysis, DESCRIPTION promotion, or metadata sweep behavior is claimed by this slice.

## RED/GREEN

- RED: after adding focused assertions for `ownerTypeName`, `declaringTypeName`, and `declaringType`, WSL GCC built the
  focused target but `zr_vm_reflection_token_resolve_test` failed because the new fields were still null.
- GREEN: after updating the FieldInfo builder, WSL GCC `zr_vm_reflection_token_resolve_test` passed 8/0.

## Verification

- Initial focused GREEN:
  - WSL GCC `zr_vm_reflection_token_resolve_test`: 8 tests, 0 failures.

- Full focused verification:
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_reflection_token_resolve_test`: 8/0.
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_metadata_runtime_query_test`: 24/0.
  - WSL GCC, WSL Clang, and Windows MSVC Debug `zr_vm_metadata_runtime_typespec_layout_test`: 17/0.
  - Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: 3/3 on all three
    environments.
