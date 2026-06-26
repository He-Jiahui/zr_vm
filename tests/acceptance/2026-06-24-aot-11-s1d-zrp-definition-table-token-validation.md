# 2026-06-24 AOT 11-S1D ZRP Definition Table Token Validation

## Scope

11-S1D adds row-level token/table-tag validation for the zrp data-metadata definition tables:

- TypeDef rows require TYPE_DEF tokens.
- MethodDef and FieldDef rows require MEMBER_DEF tokens and TYPE_DEF owners.
- GenericParam rows require TYPE_DEF or MEMBER_DEF owners.
- GenericParamConstraint rows require type-reference tokens.
- TypeSpec rows require TYPE_SPEC tokens.
- MethodSpec rows use SIGNATURE tokens and point at member def/ref tokens.
- ModuleRef rows require ASSEMBLY_REF tokens.

This is format validation only. It does not validate cross-table RID ranges, build runtime caches, or export real
compiler-produced table rows.

## Implementation

- `zr_vm_core/include/zr_vm_core/zrp_metadata.h`
  declares `ZrCore_ZrpMetadata_ValidateDefinitionTables()`.
- `zr_vm_core/src/zr_vm_core/zrp_metadata.c`
  walks the version-2 section views and checks each definition row's token table tag and basic owner/reference tag.
- `tests/module/test_zrp_metadata_format.c`
  creates a small in-memory zrp metadata payload with all definition-table sections, then verifies both valid and invalid
  token-tag cases.

## RED / GREEN

RED:

- `zr_vm_zrp_metadata_format_test` linked against a missing `ZrCore_ZrpMetadata_ValidateDefinitionTables()` symbol.

GREEN:

- A valid definition-table payload passes validation.
- A TypeDef row with a MEMBER_REF token is rejected.
- A MethodDef row with a TYPE_REF owner token is rejected.
- A MethodSpec row whose `methodToken` points at a TypeDef token is rejected.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_zrp_metadata_format_test -j2 && ./build-wsl-gcc/bin/zr_vm_zrp_metadata_format_test'`
  - 5 tests, 0 failures

## Acceptance Decision

Accepted as 11-S1D only. Definition-table rows now have basic token/table-tag guardrails, but cross-row resolution and
runtime lazy metadata lookup remain future 11 work.
