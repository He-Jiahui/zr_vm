# 2026-06-24 AOT 11-S1E ZRP Definition Table Range Validation

## Scope

11-S1E extends zrp definition-table validation from token/table tags to basic RID and child-range consistency:

- MethodDef and FieldDef owner TypeDef tokens must point at an existing TypeDef RID.
- GenericParam owner tokens must point at an existing TypeDef or member-definition RID.
- GenericParamConstraint rows must point at an existing generic parameter index.
- TypeDef method, field, and generic-parameter ranges must stay within the corresponding section counts.

This is still format-layer validation. It does not resolve symbols across modules or build runtime metadata caches.

## Implementation

- `zr_vm_core/src/zr_vm_core/zrp_metadata.c`
  extends `ZrCore_ZrpMetadata_ValidateDefinitionTables()` with RID and range helpers.
- `tests/module/test_zrp_metadata_format.c`
  adds an in-memory payload that first validates, then mutates owner RIDs, child ranges, and constraint indexes to prove
  malformed rows are rejected.

## RED / GREEN

RED:

- The new cross-table range test failed at runtime because invalid owner RIDs, TypeDef method ranges, and generic-param
  constraint indexes still validated as true.

GREEN:

- The valid payload passes.
- A MethodDef owner TypeDef RID outside the TypeDef table is rejected.
- A TypeDef method range beyond the MethodDef table is rejected.
- A GenericParamConstraint row pointing past the GenericParam table is rejected.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_zrp_metadata_format_test -j2 && ./build-wsl-gcc/bin/zr_vm_zrp_metadata_format_test'`
  - 6 tests, 0 failures

## Acceptance Decision

Accepted as 11-S1E only. The zrp format now rejects the most direct malformed RID/range relationships, but full metadata
resolution and compiler-produced table materialization remain open.
