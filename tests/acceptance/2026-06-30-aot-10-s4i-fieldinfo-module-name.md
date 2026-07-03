---
acceptance: aot-10-s4i-fieldinfo-module-name
date: 2026-06-30 19:26:41 +08:00
plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
scope:
  - 10-S4I
  - 11-S4W
  - 12-S5 support
status: completed
---

# AOT 10-S4I FieldInfo Module Name

## Scope

This slice extends the minimum FieldDef token `FieldInfo` object with a stable `moduleName` string carrier sourced from
the attached metadata runtime module. It does not build a full module reflection object, add module reflection cache
behavior, change metadata rows, or change the code-registration ABI.

## Completed

- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now reads `runtime->module->moduleName`, with `fullPath` as fallback.
- The generated `FieldInfo` object now exposes `moduleName`.
- The focused FieldDef token test gives the attached module a synthetic name and asserts `moduleName == "geometry"`.

## RED/GREEN

- RED: after adding the focused `moduleName` assertion, WSL GCC built the focused target but
  `zr_vm_reflection_token_resolve_test` failed because the field was still missing.
- GREEN: after filling `moduleName` from the attached runtime module, WSL GCC
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
