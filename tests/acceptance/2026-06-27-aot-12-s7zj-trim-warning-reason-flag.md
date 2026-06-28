# AOT 12-S7ZJ Trim Warning Reason Flag

## Scope
- Added a machine-readable reason flag to emitted runtime-fallback trim warning comments.
- Affected layers: AOT C runtime fallback warning writer, generated warning smoke tests, AOT plan/status docs, and module documentation.

## Baseline
- Before this slice, `trim_warning.runtimeFallback[...]` comments exposed source location and textual `reason=...`, but did not include the corresponding `ZR_AOT_RUNTIME_FALLBACK_WARNING_*` suppression-mask bit.
- RED evidence: WSL gcc `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` failed 3/6 once existing warning expectations required `reasonFlag=... reason=...`.

## Test Inventory
- Generated-product smoke: `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`
- Source contracts: `tests/parser/test_aot_c_source_contracts.c`
- Regression integration: `tests/parser/test_aot_c_code_stripping.c`

## Tooling Evidence
- WSL gcc:
  - dynamic deopt bridge smoke 6/0, source contracts 21/0, code stripping 5/0
  - CTest `aot_c_code_stripping` 1/1
- WSL clang:
  - dynamic deopt bridge smoke 6/0, source contracts 21/0, code stripping 5/0
- Windows MSVC Debug:
  - dynamic deopt bridge smoke 0 failures/6 ignored; source contracts 21/0; code stripping 5/0
  - CTest `aot_c_code_stripping` 1/1

## Results
- `backend_aot_c_runtime_fallback.c` now writes `reasonFlag=<mask>` before the textual reason in each unsuppressed runtime-fallback warning.
- The flag value is derived from the existing `backend_aot_c_runtime_fallback_warning_flag_for_reason()` mapping, keeping warning output aligned with reason-mask suppression.
- Dynamic call warnings now include `reasonFlag=1`; dynamic value-access warnings include `reasonFlag=2`.

## Acceptance Decision
- Accepted for runtime-fallback trim warning reason consumption.
- Remaining risks: this does not implement `@requires_unreferenced_code`, reflection data-flow annotations, annotation-based suppression/promotion, or a complete trim analyzer.
