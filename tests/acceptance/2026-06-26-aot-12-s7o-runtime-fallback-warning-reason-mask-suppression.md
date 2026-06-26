# AOT 12-S7O Runtime Fallback Warning Reason-Mask Suppression

## Scope

- Slice: 12-S7O, under 12-S7 trim warnings and diagnostics.
- Goal: allow writer callers to suppress selected runtime fallback warning reasons without hiding other reasons.
- Output contract: `suppressRuntimeFallbackWarningReasonMask` hides only matching `trim_warning.runtimeFallback[...]` entries, keeps compact visible indexes, and still reports `trim_warnings.runtimeFallbackSuppressedCount`.
- Non-goals: attribute-driven suppression, source file/column spans, full trim analyzer policy, pre-trim generated-C attribution, metadata sweep diff, and release symbol stripping.

## RED

- Command:
  `wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2"`
- Result before implementation: build failed because `SZrAotWriterOptions` had no `suppressRuntimeFallbackWarningReasonMask` member and `ZR_AOT_RUNTIME_FALLBACK_WARNING_DYNAMIC_CALL` was undeclared.

## GREEN

- Production: `writer.h` now exposes `EZrAotRuntimeFallbackWarningFlag` with dynamic-call, dynamic-value-access, dynamic-iterator, reflection, and all masks.
- Production: `SZrAotWriterOptions` now carries `suppressRuntimeFallbackWarningReasonMask`.
- Production: `backend_aot_option_runtime_fallback_warning_suppression_mask()` keeps global `suppressRuntimeFallbackWarnings` as the all-reasons shortcut and otherwise masks caller-provided reason flags to known bits.
- Production: runtime fallback warning counting and marker emission now filter by reason mask, count visible warnings separately from suppressed warnings, and keep visible marker indexes compact.
- Test: `test_aot_c_dynamic_deopt_bridge_smoke.c` now covers a dynamic-call warning suppressed by reason mask and a dynamic-value-access warning that remains visible under the same mask.
- Test: `test_aot_c_source_contracts.c` locks the public writer option and suppression-mask helper contract.

## Generated Evidence

- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_call_reason_suppressed.c` contains:
  - `trim_warnings.runtimeFallbackCount = 0`
  - `trim_warnings.runtimeFallbackSuppressedCount = 1`
  - no visible `trim_warning.runtimeFallback[0]` entry
- `build-wsl-gcc/tests_generated/aot_c_dynamic_deopt_bridge/runtime_project/bin/aot_c/src/dynamic_member_reason_visible.c` contains:
  - `trim_warnings.runtimeFallbackCount = 1`
  - `trim_warnings.runtimeFallbackSuppressedCount = 0`
  - `trim_warning.runtimeFallback[0] function=0 instruction=0 sourceLine=41 sourceLineEnd=43 reason=dynamic-value-access`

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

- Current physical line counts after this slice: `test_aot_c_dynamic_deopt_bridge_smoke.c` 584, `backend_aot_c_runtime_fallback.c` 446, `backend_aot_c_emitter.c` 882, and `test_aot_c_source_contracts.c` 2192.
- `test_aot_c_source_contracts.c` is already oversized, but this slice only adds public writer-option and internal-helper needles. Future cleanup should extract writer/public-ABI option contracts into a focused source-contract test.

## Remaining

- 12-S7 remains open for full trim analyzer behavior, file/column source spans, attribute/annotation suppression policy, pre-trim generated-C type/layout byte span attribution, metadata sweep diff, and release symbol stripping.
