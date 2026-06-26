# AOT 12-S7N Runtime Fallback Source Line Span

## Scope

- Slice: 12-S7N, under 12-S7 trim warnings and diagnostics.
- Goal: runtime fallback trim warning markers report a source line span, not only a start line.
- Output contract: each visible `trim_warning.runtimeFallback[...]` marker includes `sourceLine=<start>` and `sourceLineEnd=<end>`.
- Non-goals: source file name, columns, AST range IDs, full trim analyzer, per-warning suppression policy, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test"`
- Result before implementation: `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` ran 5 tests and failed 2 tests with `Expected Non-NULL`.
- Failure reason: visible runtime fallback warning markers still emitted `sourceLine=41 reason=...` with no `sourceLineEnd=43`.

## GREEN

- Production: `SZrAotExecIrInstruction` now carries `debugLineEnd` beside `debugLine`.
- Production: `backend_aot_exec_ir.c` computes end lines from execution-location metadata when available, falls back to the per-instruction line list, then to function end/start lines, and clamps end line to be no earlier than start line.
- Production: `backend_aot_c_runtime_fallback.c` emits `sourceLineEnd` in each visible runtime fallback trim warning marker.
- Test: `test_aot_c_dynamic_deopt_bridge_smoke.c` now uses a synthetic function span `41..43` and asserts both dynamic-call and dynamic-value-access warning markers include `sourceLineEnd=43`.

## Generated Evidence

- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_deopt_bridge.c` contains:
  - `trim_warnings.runtimeFallbackCount = 1`
  - `trim_warnings.runtimeFallbackSuppressedCount = 0`
  - `trim_warning.runtimeFallback[0] function=0 instruction=0 sourceLine=41 sourceLineEnd=43 reason=dynamic-call`
- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_member_deopt_bridge.c` contains:
  - `trim_warning.runtimeFallback[0] function=0 instruction=0 sourceLine=41 sourceLineEnd=43 reason=dynamic-value-access`
- The suppressed fixture still reports `runtimeFallbackCount = 0` and `runtimeFallbackSuppressedCount = 1`, with no visible warning entry.

## Validation

- WSL gcc:
  Direct `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test`: 5/0 passed.
  CTest filter `aot_c_code_stripping|aot_c_generic_call_typed|zrp_metadata_format`: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- WSL clang:
  Direct `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test`: 5/0 passed.
  Same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test`: 19/0 passed.
- Windows MSVC Debug:
  Direct `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test.exe`: 0 failures, 5 ignored Unix shared-library tests.
  Same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test.exe`: 19/0 passed.
- Modularization note:
  `backend_aot_exec_ir.c` is 972 lines after this change. The slice extends the file's existing source-location extraction responsibility and does not add a new subsystem. If it grows again, extract instruction source-location/span derivation into a focused helper module.

## Remaining

- 12-S7 remains open for full trim analyzer behavior, file/column source spans, per-warning suppression policy, pre-trim generated-C byte span attribution, metadata sweep diff, and release symbol stripping.
