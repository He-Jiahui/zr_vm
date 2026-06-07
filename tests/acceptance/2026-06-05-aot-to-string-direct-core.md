# AOT TO_STRING Direct Core Conversion

Date: 2026-06-05

Scope: generated AOT C `TO_STRING` lowering.

## Contract

- Generated AOT C lowers `TO_STRING` to a direct core semantic conversion block, not an AOT runtime instruction helper.
- The emitted block resolves source and destination values from `frame.slotBase`, calls `ZrCore_Value_ConvertToString(state, source)`, refreshes `frame.callInfo`, `frame.slotBase`, and `state->stackTop` from the active call info after possible meta conversion work, then writes either a string object or null directly.
- String results are written with `ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(resultString))` and `destination->type = ZR_VALUE_TYPE_STRING`.
- Null results are written with `ZrCore_Value_ResetAsNull(destination)`.
- This is a direct core semantic boundary, not primitive-only formatting. `TO_STRING` can invoke `ZR_META_TO_STRING`, so generated C deliberately keeps the conversion inside `ZrCore_Value_ConvertToString`.
- The C backend no longer emits `ZrLibrary_AotRuntime_ToString(state, &frame, ...)` from the value/function-body lowering path. LLVM still keeps its existing helper route until LLVM parity is handled.

## RED

The focused source contract was added before production code and failed as expected:

```text
Missing source contract text: zr_aot_value_exec_to_string
5 Tests 1 Failures 0 Ignored
```

## Test Inventory

- `test_aot_c_source_lowers_to_string_to_direct_core_conversion` in `tests/parser/test_aot_c_global_contracts.c`.
- `test_aot_c_generated_shared_library_compiles_to_string_direct_core_conversion` in `tests/parser/test_aot_c_global_shared_library_smoke.c`.
- The generated-C smoke hand-builds `GET_GLOBAL` -> `TO_STRING` -> `FUNCTION_RETURN`, verifies the direct core conversion block, rejects the old `ZrLibrary_AotRuntime_ToString(state, &frame` helper route, and compiles the generated C into a Unix shared library.

## Validation

- GCC focused/global set in `build-wsl-gcc`: source contracts 17 tests, constant contracts 4 tests, global contracts 5 tests, global shared-library smoke 6 tests, shared-library smoke 6 tests, all 0 failures.
- Clang focused/global set in `build-wsl-clang`: source contracts 17 tests, constant contracts 4 tests, global contracts 5 tests, global shared-library smoke 6 tests, shared-library smoke 6 tests, all 0 failures.
- MSVC focused set in `build/codex-msvc-aot-constant-debug`: source contracts 17 tests, constant contracts 4 tests, global contracts 5 tests, global shared-library smoke 6 tests, all 0 failures. The 6 generated shared-library smoke cases are ignored because that compile path is Unix-only.
- Source scan found `zr_aot_value_exec_to_string` and `ZrCore_Value_ConvertToString` in `backend_aot_c_lowering_values.c`, with no `ZrLibrary_AotRuntime_ToString` emission in the checked C backend value/function-body lowering sources.

## Files

- `tests/parser/test_aot_c_global_contracts.c`
- `tests/parser/test_aot_c_global_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
