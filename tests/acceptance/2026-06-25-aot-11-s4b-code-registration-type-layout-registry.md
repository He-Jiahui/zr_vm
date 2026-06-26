# 2026-06-25 AOT 11-S4B Code Registration Type Layout Registry

## Scope

11-S4B adds the first generated-C code-registration type layout registry. ABI version 9 exposes
`typeLayouts/typeLayoutCount` on both `SZrAotCodeRegistration` and `ZrAotCompiledModule`.

## Completed

- Generated C emits `SZrTypeLayoutField`, `SZrTypeLayout`, and sparse `zr_aot_type_layouts[]` entries for reachable inline struct layouts.
- The registry is indexed by `typeLayoutId/cTypeId`, and the module descriptor points at the same registry as `codeRegistration`.
- Runtime descriptor validation rejects descriptor/codeRegistration type-layout registry mismatches and inconsistent null/count shapes.
- Value-type smoke coverage verifies the loaded registry is non-empty, `layout->cTypeId == descriptor->typeLayoutId`, and GC offsets match the descriptor.

## RED/GREEN

- RED: `zr_vm_aot_c_source_contracts_test` required `const struct SZrTypeLayout *const *typeLayouts;` before the ABI field existed.
- GREEN: no-layout generated modules expose null/0 layout registry; value-type generated modules expose the layout registry and align it with GC descriptors.

## Validation

- WSL gcc: metadata runtime query 19/0, frame setup 1/0, source contracts 19/0, shared-library smoke 8/0, value-type shared-library smoke 2/0, descriptor diagnostics 2/0.
- WSL clang: metadata runtime query 19/0, frame setup 1/0, source contracts 19/0, shared-library smoke 8/0, value-type shared-library smoke 2/0, descriptor diagnostics 2/0.
- Windows MSVC Debug: metadata runtime query 19/0, frame setup 1/0, source contracts 19/0, shared-library smoke 8/0 with 8 Unix-only branches ignored, value-type shared-library smoke 2/0 with 1 Unix-only branch ignored, descriptor diagnostics 2/0 with 2 Unix-only branches ignored.

## Remaining

This does not close full 11-S4. TypeSpec/generic layout materialization, ownership offset emission, runtime layout construction, reflection/generic/GC unified consumers, and the complete token/cTypeId/layout cache remain open.
