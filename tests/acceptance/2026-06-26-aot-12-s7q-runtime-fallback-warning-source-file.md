# AOT 12-S7Q Runtime Fallback Warning Source File

## Scope

- Slice: 12-S7Q, under 12-S7 trim warnings and diagnostics.
- Goal: runtime fallback trim warning markers report the source file identity in addition to source line and column spans.
- Output contract: each visible `trim_warning.runtimeFallback[...]` marker includes `sourceFile=<file>` before `sourceLine`.
- Non-goals: source path escaping, AST range IDs, full trim analyzer policy, attribute/annotation suppression, pre-trim generated-C attribution, metadata sweep diff, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test"`
- Result before implementation: build succeeded, then `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` ran 6 tests and failed 3 tests with `Expected Non-NULL`.
- Failure reason: visible runtime fallback warning markers still emitted line/column spans but no `sourceFile=dynamic_deopt_bridge.zr`.

## GREEN

- Production: `backend_aot_c_runtime_fallback.c` now reads `entry->function->sourceCodeList` and emits a stable source file token for visible runtime fallback warning markers.
- Production: missing or empty `sourceCodeList` falls back to `<unknown>`.
- Test: `test_aot_c_dynamic_deopt_bridge_smoke.c` now gives the synthetic fallback function a source file string and asserts dynamic-call and dynamic-value-access warning markers include it.

## Generated Evidence

- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_deopt_bridge.c` contains:
  - `trim_warnings.runtimeFallbackCount = 1`
  - `trim_warnings.runtimeFallbackSuppressedCount = 0`
  - `trim_warning.runtimeFallback[0] function=0 instruction=0 sourceFile=dynamic_deopt_bridge.zr sourceLine=41 sourceLineEnd=43 sourceColumn=7 sourceColumnEnd=19 reason=dynamic-call`
- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_member_deopt_bridge.c` contains:
  - `trim_warning.runtimeFallback[0] function=0 instruction=0 sourceFile=dynamic_deopt_bridge.zr sourceLine=41 sourceLineEnd=43 sourceColumn=7 sourceColumnEnd=19 reason=dynamic-value-access`
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
  Direct `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test.exe`: 0 failures, 6 ignored Unix shared-library tests.
  Same CTest filter: 3/3 passed.
  Direct `zr_vm_aot_c_source_contracts_test.exe`: 19/0 passed.

## Modularization Note

- Current physical line counts after this slice: `backend_aot_c_runtime_fallback.c` 496, `test_aot_c_dynamic_deopt_bridge_smoke.c` 602, and `backend_aot_exec_ir.c` 906.
- No new split was needed for this narrow attribution change.
- `test_aot_c_source_contracts.c` remains oversized from prior work; this slice did not add to it.

## Remaining

- 12-S7 remains open for full trim analyzer behavior, attribute/annotation suppression policy, pre-trim generated-C type/layout byte span attribution, metadata sweep diff, and release symbol stripping.
