# AOT M1 / 02-S2 Type Layout Declarations

## Scope

- Slice: `02-S2` from `docs/plans/aot/02-typed-value-and-layout.md`.
- Milestone: M1 type determinism foundation, partial milestone completion only.
- Goal: emit generated C layout declarations from canonical runtime type metadata and fail generated C compilation if metadata size, alignment, or field offsets drift.

## Completed Items

- Added `backend_aot_c_type_layouts.h` and `backend_aot_c_type_layouts.c`.
- Integrated AOT C layout declaration emission into `backend_aot_c_emitter.c`.
- Emitted one deduplicated `ZrLayout_<typeLayoutId>` declaration per inline struct frame layout discovered from `SZrAotFunctionTable`.
- Reused `ZrCore_Function_ResolvePrototypeFrameTypeLayout` and `ZrCore_Function_VisitPrototypeFrameFieldLayouts` so generated declarations are driven by the canonical runtime metadata.
- Emitted padding members, generated field members, and `sizeof`, `_Alignof`, and `offsetof` static assertions.
- Added `ZR_AOT_C_LAYOUT_STRUCT` to carry metadata alignment into generated C structs on MSVC, GCC, and Clang.
- Added focused source contracts in `tests/parser/test_aot_c_type_layout_contracts.c`.
- Registered the focused target and CTest entry `aot_c_type_layout_contracts`.

## RED / GREEN Evidence

- RED: `zr_vm_aot_c_type_layout_contracts_test` failed before implementation because the new backend layout helper module and emitter integration did not exist.
- RED: compiling an actual generated value-type AOT C file failed on `ZrLayout_0 align drift`, proving that field declarations alone were not enough to satisfy metadata `byteAlign`.
- GREEN: the generated layout alignment macro makes the same generated value-type C file compile successfully with its `sizeof`, `_Alignof`, and `offsetof` static assertions enabled.

## Tests

Focused validation:

```text
zr_vm_type_layout_metadata_contracts_test
zr_vm_aot_c_type_layout_contracts_test
zr_vm_aot_c_guardrail_contracts_test
zr_vm_aot_c_golden_scalar_smoke_test
ctest -R "type_layout_metadata_contracts|aot_c_type_layout_contracts|aot_c_guardrail_contracts|aot_c_golden_scalar_smoke"
manual gcc compile of generated aot_c_value_type_shared_library main.c
```

Observed results:

- Type layout metadata contracts: 4 tests / 0 failures.
- AOT C type layout contracts: 1 test / 0 failures.
- AOT C guardrail contracts: 4 tests / 0 failures.
- AOT C scalar golden smoke: 1 test / 0 failures.
- Focused CTest filter: 4 tests / 0 failures.
- Generated value-type C compile: success after alignment fix.

## Status

- Status: 02-S2 complete.
- M1 remains partially complete. The following M1 items are still open:
  - `semIrTypeTable` static C type annotations;
  - typed-block validation that rejects generic arithmetic opcodes.
- The existing `zr_vm_aot_c_value_type_shared_library_smoke_test` still fails before compiling its generated C because the generated body contains `unsupported AOT value SemIR field`. That is a later value SemIR / struct lowering gap, not part of this 02-S2 layout declaration acceptance.
