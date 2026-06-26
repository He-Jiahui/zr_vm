# AOT 12-S7P Runtime Fallback Warning Column Span

## Scope

- Slice: 12-S7P, under 12-S7 trim warnings and diagnostics.
- Goal: runtime fallback trim warning markers report source columns in addition to source lines.
- Output contract: each visible `trim_warning.runtimeFallback[...]` marker includes `sourceColumn=<start>` and `sourceColumnEnd=<end>` after `sourceLine/sourceLineEnd`.
- Non-goals: source file attribution, AST range IDs, full trim analyzer policy, attribute/annotation suppression, pre-trim generated-C attribution, metadata sweep diff, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test"`
- Result before implementation: build succeeded, then `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` ran 6 tests and failed 3 tests with `Expected Non-NULL`.
- Failure reason: visible runtime fallback warning markers still emitted `sourceLine=41 sourceLineEnd=43 reason=...` with no `sourceColumn=7 sourceColumnEnd=19`.

## GREEN

- Production: `SZrAotExecIrInstruction` now carries `debugColumn` and `debugColumnEnd` beside `debugLine/debugLineEnd`.
- Production: source-location derivation moved from the oversized `backend_aot_exec_ir.c` into `backend_aot_exec_ir_source_location.{h,c}`.
- Production: the source-location helper derives columns from `SZrFunctionExecutionLocationInfo.columnInSourceStart/End`, with end falling back to start when absent.
- Production: `backend_aot_c_runtime_fallback.c` emits `sourceColumn` and `sourceColumnEnd` in each visible runtime fallback trim warning marker.
- Test: `test_aot_c_dynamic_deopt_bridge_smoke.c` now populates execution-location column span `7..19` and asserts dynamic-call and dynamic-value-access warning markers include it.
- Test: `test_aot_c_source_contracts.c` locks the source-location module split and column derivation helper calls.

## Generated Evidence

- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_deopt_bridge.c` contains:
  - `trim_warnings.runtimeFallbackCount = 1`
  - `trim_warnings.runtimeFallbackSuppressedCount = 0`
  - `trim_warning.runtimeFallback[0] function=0 instruction=0 sourceLine=41 sourceLineEnd=43 sourceColumn=7 sourceColumnEnd=19 reason=dynamic-call`
- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_member_deopt_bridge.c` contains:
  - `trim_warning.runtimeFallback[0] function=0 instruction=0 sourceLine=41 sourceLineEnd=43 sourceColumn=7 sourceColumnEnd=19 reason=dynamic-value-access`
- The reason-mask suppressed fixture still reports `runtimeFallbackCount = 0` and `runtimeFallbackSuppressedCount = 1`, with no visible warning entry.

## Validation

- WSL gcc:
  Direct `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test`: 6/0 passed.
  CTest filter `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- WSL clang:
  Direct `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test`: 6/0 passed.
  Same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- Windows MSVC Debug:
  Reconfigured after CMake glob mismatch and compiled `backend_aot_exec_ir_source_location.c`.
  Direct `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test.exe`: 0 failures, 6 ignored Unix shared-library tests.
  Same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test.exe`: 19/0 passed.

## Modularization Note

- The initial column implementation pushed `backend_aot_exec_ir.c` to 1023 lines, crossing the project threshold.
- The source-location/span derivation was extracted into `backend_aot_exec_ir_source_location.{h,c}` before acceptance.
- Current physical line counts after extraction: `backend_aot_exec_ir.c` 906, `backend_aot_exec_ir_source_location.c` 114, `backend_aot_c_runtime_fallback.c` 482, `test_aot_c_dynamic_deopt_bridge_smoke.c` 598, and `test_aot_c_source_contracts.c` 2212.
- `test_aot_c_source_contracts.c` remains oversized; this slice only adds module-boundary needles. Future cleanup should split AOT writer/backend source-contract groups by backend subsystem.

## Remaining

- 12-S7 remains open for full trim analyzer behavior, source file attribution, attribute/annotation suppression policy, pre-trim generated-C type/layout byte span attribution, metadata sweep diff, and release symbol stripping.
