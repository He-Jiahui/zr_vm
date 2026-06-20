# AOT M1 / 02-S1 Type Layout Metadata

## Scope

- Slice: `02-S1` from `docs/plans/aot/02-typed-value-and-layout.md`.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: extend `SZrTypeLayout` with AOT-facing metadata without changing generated AOT behavior yet.

## Completed Items

- Added `SZrTypeLayoutMetadata` as the initializer input for stable generated C metadata.
- Extended `SZrTypeLayout` with:
  - `blittable`
  - `cTypeId`
  - `gcFieldOffsets`
  - `ownershipFieldOffsets`
- Added `ZrCore_TypeLayout_InitStructWithMetadata`.
- Kept `ZrCore_TypeLayout_InitStruct` as the neutral-metadata compatibility entry.
- Changed `ZrCore_TypeLayout_CanRawCopy` to use the computed `blittable` bit.
- Added focused contracts in `tests/core/test_type_layout_metadata_contracts.c`.
- Registered the focused target and CTest entry `type_layout_metadata_contracts`.

## RED / GREEN Evidence

- RED: building `zr_vm_type_layout_metadata_contracts_test` failed before implementation because
  `SZrTypeLayoutMetadata`, `ZrCore_TypeLayout_InitStructWithMetadata`, and the new
  `SZrTypeLayout` metadata fields did not exist.
- GREEN: the focused target builds and its direct binary run passes after implementation.

## Tests

Focused validation target:

```text
zr_vm_type_layout_metadata_contracts_test
```

Covered contracts:

- POD struct metadata records `blittable` and `cTypeId`.
- Managed field metadata records GC and ownership offset tables.
- Default struct init keeps neutral AOT metadata.
- A null field table does not get scanned while counting metadata fields.

## Status

- Status: 02-S1 complete.
- M1 remains partially complete. The following M1 items are still open:
  - generated `struct ZrLayout_*` declarations with `offsetof` / `sizeof` static assertions;
  - `semIrTypeTable` static C type annotations;
  - typed-block validation that rejects generic arithmetic opcodes.

