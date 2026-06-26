# AOT 12-S7M Runtime Fallback Warning Suppression

## Scope

- Slice: 12-S7M, under 12-S7 trim warnings and size statistics.
- Goal: generated AOT C can suppress hybrid runtime fallback trim warning marker emission when the caller explicitly requests it.
- Public option: `SZrAotWriterOptions.suppressRuntimeFallbackWarnings`.
- Output contract: suppressed warnings move from `trim_warnings.runtimeFallbackCount` to `trim_warnings.runtimeFallbackSuppressedCount`; individual `trim_warning.runtimeFallback[...]` entries are not emitted.
- Non-goals: full trim analyzer, per-warning/attribute-based suppression, complete source spans, metadata sweep diff, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2"`
- Result before implementation: build failed in `test_aot_c_suppresses_runtime_fallback_trim_warnings_when_requested`.
- Failure reason: `SZrAotWriterOptions` had no `suppressRuntimeFallbackWarnings` member.

## GREEN

- Production: `writer.h` exposes `suppressRuntimeFallbackWarnings` on `SZrAotWriterOptions`.
- Production: `backend_aot.c` and `backend_aot_internal.h` expose `backend_aot_option_suppress_runtime_fallback_warnings()`.
- Production: `backend_aot_c_emitter.c` still counts runtime fallback diagnostics, then emits either visible count or suppressed count. Suppression only skips generated warning marker output.
- Test: `test_aot_c_dynamic_deopt_bridge_smoke.c` now writes a dynamic-call deopt fixture with suppression enabled and asserts:
  - `trim_warnings.runtimeFallbackCount = 0`
  - `trim_warnings.runtimeFallbackSuppressedCount = 1`
  - no `trim_warning.runtimeFallback[0]`
  - the dynamic deopt bridge call remains present
- Source contract: `test_aot_c_source_contracts.c` locks the public option and internal helper surface.

## Generated Evidence

- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_deopt_bridge_trim_suppressed.c` contains:
  - `trim_warnings.runtimeFallbackCount = 0`
  - `trim_warnings.runtimeFallbackSuppressedCount = 1`
  - `ZrLibrary_AotRuntime_CallDynamicDeoptBridge(state,`
- The same generated file does not contain `trim_warning.runtimeFallback[0]`.

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
  `test_aot_c_source_contracts.c` is already above the normal split threshold, but this slice only adds public writer-option contract needles and no new fixture/helper responsibility. A future cleanup should extract writer/public-ABI option contracts into a focused source-contract test file.

## Remaining

- 12-S7 remains open for full trim analyzer behavior, complete source spans, per-warning suppression policy, pre-trim generated-C byte span attribution, metadata sweep diff, and release symbol stripping.
