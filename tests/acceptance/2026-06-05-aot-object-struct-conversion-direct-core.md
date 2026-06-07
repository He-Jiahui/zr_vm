# AOT Object/Struct Conversion Direct Core Boundary

Date: 2026-06-05

Scope: generated AOT C `TO_OBJECT` / `TO_STRUCT` lowering.

## Contract

- Generated AOT C lowers `TO_OBJECT` and `TO_STRUCT` to explicit core semantic calls, not AOT runtime instruction helpers.
- Generated modules include `zr_vm_core/execution.h`.
- The emitted blocks resolve destination and source values with `ZrCore_Stack_GetValue(frame.slotBase + slot)`, load the type-name operand from `frame.function->constantValueList[index]`, emit `zr_aot_value_exec_to_object` / `zr_aot_value_exec_to_struct`, and fail through `ZR_AOT_C_FAIL()` on invalid operands or failed conversion.
- The generated C calls `ZrCore_Execution_ToObject(state, frame.callInfo, ...)` and `ZrCore_Execution_ToStruct(state, frame.callInfo, ...)`.
- The C backend no longer emits `ZrLibrary_AotRuntime_ToObject` or `ZrLibrary_AotRuntime_ToStruct` from the value/function-body lowering path. LLVM still keeps its existing helper route until LLVM parity is handled.

## RED

The focused source contract was added before production code and failed as expected:

```text
Missing source contract text: #include \"zr_vm_core/execution.h\"\n
3 Tests 1 Failures 0 Ignored
```

## Validation

- GCC focused/global set in `build-wsl-gcc`: source contracts 17 tests, constant contracts 3 tests, global contracts 3 tests, global shared-library smoke 3 tests, shared-library smoke 6 tests, all 0 failures.
- Clang focused/global set in `build-wsl-clang`: source contracts 17 tests, constant contracts 3 tests, global contracts 3 tests, global shared-library smoke 3 tests, shared-library smoke 6 tests, all 0 failures.
- MSVC focused set in `build/codex-msvc-aot-constant-debug`: source contracts 17 tests, constant contracts 3 tests, global contracts 3 tests, all 0 failures.
- MSVC global shared-library smoke in `build/codex-msvc-aot-global-debug`: 3 tests, 0 failures, 3 ignored because generated shared-library compilation is Unix-only.
- `git diff --check` reported only existing LF-to-CRLF warnings for touched backend files and no whitespace errors.
- A C-backend source scan found no `ZrLibrary_AotRuntime_ToObject` or `ZrLibrary_AotRuntime_ToStruct` emission in the AOT C value/function-body lowering sources.

## Files

- `tests/parser/test_aot_c_global_contracts.c`
- `tests/parser/test_aot_c_global_shared_library_smoke.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_values.c`
