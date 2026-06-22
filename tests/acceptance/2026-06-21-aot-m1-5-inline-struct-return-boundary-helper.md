# AOT M1.5 Inline Struct Return Boundary Helper

## Scope
- Continued AOT 07-S5 boundary marshaling by moving value SemIR typed inline-struct returns through a runtime helper.
- Covered the adjacent typed-call argument-source materialization needed before prepared VM calls.
- Fixed two regressions exposed by the executable shared-library smoke: stale dense argument values on static/direct calls, and over-eager scalar stack-copy lowering in value-frame code.

## Baseline
- Typed inline-struct return lowering still wrote return layout checks, `state->stackTop.valuePointer`, and `zr_aot_skip_drop_slot` directly in generated C.
- Typed-call argument staging could pass stale value-frame slots when scalar locals had the current value but the dense/source slot had not been materialized.
- Scalar stack-copy lowering inferred scalar type from whole-function local declarations, so a value-frame temporary struct/object slot could be read as i64 before the scalar value was actually written.

## Test Inventory
- `tests/parser/test_aot_c_return_contracts.c`
  - Requires public/source coverage for `ZrLibrary_AotRuntime_ReturnInlineStruct()`.
  - Requires the helper to resolve the prototype frame type layout and publish `outSkipDropSlot`.
- `tests/parser/test_aot_c_value_semir_contracts.c`
  - Requires value SemIR typed returns to call `ZrLibrary_AotRuntime_ReturnInlineStruct(state, ...)`.
  - Forbids the old generated-C inline return `SZrCallInfo`, `state->stackTop.valuePointer = zr_aot_return_source + 1`, and direct skip-drop assignment shapes.
- `tests/parser/test_aot_c_call_shared_library_smoke.c`
  - Executes the dynamic/static int call fixture.
  - Executes the POD struct typed-call return fixture through the generated shared library.
  - Checks typed-call argument materialization, helper-based inline struct return, scalar stack-copy force-write behavior, and absence of old helper/direct inline return paths.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_call_contracts.c`
  - Lock static direct-call argument materialization, inline-frame stack-copy fallback, and the scalar stack-copy eligibility guard.

## Tooling Evidence
- Focused RED:
  - The value typed-call smoke failed at `ZrLibrary_AotRuntime_ExecuteEntry(...)` after generated function `zr_aot_fn_1` lowered `STACK_COPY dstSlot=3 srcSlot=2` as `zr_aot_scalar_stack_copy_i64` before slot 2 had a scalar value.
  - GDB stopped at `ZrCore_Debug_RunError`; after the scalar guard change the next failure moved to `ZrCore_Function_CopyObjectValueToFrameSlotInline`, showing the fallback stack copy still needed inline-struct-to-inline-struct copying instead of object conversion.
- Focused GREEN:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
  - Result: return contracts 1/0; value SemIR contracts 4/0; source contracts 19/0; call contracts 4/0; call shared-library smoke 3/0.
- WSL GCC broader focused suite:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
  - Result: source contracts 19/0; return contracts 1/0; frame setup contracts 1/0; typed scalar 1/0; value SemIR contracts 4/0; call contracts 4/0; call shared-library smoke 3/0.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_typed_scalar|aot_c_call_shared_library' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. The call shared-library smoke binary is not registered as a CTest in this build and was executed directly above.
- Generated C inspection:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'zr_aot_value_exec_return_typed|ZrLibrary_AotRuntime_ReturnInlineStruct|state->stackTop.valuePointer = zr_aot_return_source|zr_aot_skip_drop_slot = 4|zr_aot_materialize_argument_source_slot|zr_aot_scalar_stack_copy_i64 dstSlot=3 srcSlot=2|ZrCore_Function_CopyFrameSlotInline' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_call_shared_library/runtime_value_type_project/bin/aot_c/src/main.c || true"`
  - Result: generated C shows `ZrLibrary_AotRuntime_ReturnInlineStruct`, `zr_aot_materialize_argument_source_slot`, and `ZrCore_Function_CopyFrameSlotInline`; the old inline return stackTop write, direct skip-drop assignment, and mistaken `dstSlot=3 srcSlot=2` scalar i64 copy are absent.

## Results
- Added `ZrLibrary_AotRuntime_ReturnInlineStruct()` to the AOT runtime return boundary module.
- Value SemIR typed returns now call the helper and leave stackTop/skip-drop validation to the runtime boundary.
- Typed-call and static direct-call paths materialize VALUE argument source slots before `PreCallPreparedResolvedVmFunctionWithArgumentSource`.
- Scalar stack-copy lowering now refuses a copy when the source slot is not already proven written as the scalar kind, not an entry parameter, and lacks an explicit static scalar source binding.
- Direct stack-copy fallback now copies inline-struct source slots to inline-struct destination slots with `ZrCore_Function_CopyFrameSlotInline()` before falling back to object-value conversion.

## Acceptance Decision
- Accepted as a 07-S5 sub-slice.
- 07-S5 remains partial. This slice covers typed inline-struct return boundary helperization, typed-call argument materialization, and the required stack-copy fallback corrections; broader typed-to-typed signature routing, in/out writeback, deopt/dynamic bridge templates, and remaining value-frame fallback boundaries remain future 07 work.
