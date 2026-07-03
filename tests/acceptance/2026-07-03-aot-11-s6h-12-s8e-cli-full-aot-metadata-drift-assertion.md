# AOT 11-S6H / 12-S8E CLI Full-AOT Metadata-Drift Assertion

Date: 2026-07-03 03:56:32 +08:00

Status: complete for this test-alignment sub-slice. The broader AOT 07-12 goal remains active.

## Scope

- Reconciled CLI project-level full-AOT generated-C assertions with the later 11-S6H inline-struct typed-call
  metadata-drift guard/deopt behavior.
- Preserved 12-S8E's guarantee that statically collected shared generic `CALL_TYPED` in full-AOT mode does not keep a
  missing METHOD-slot runtime branch or missing-instance deopt.
- Kept 11-S6H's guarantee that inline-struct typed calls still guard caller/callee metadata compatibility and deopt on
  metadata drift.

## RED

- `cli_project_incremental` failed with `Expected NULL` because the older CLI project test forbade every
  `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,` occurrence.
- The generated C already had `zr_aot_generic_call_typed_full_aot_no_deopt` and no missing-instance marker, but it also
  correctly contained the 11-S6H metadata-drift fallback.

## GREEN

- The CLI project test now forbids `zr_aot_generic_call_typed_missing_instance_deopt` and
  `"generic call typed missing AOT instance"`.
- The same test now requires `zr_aot_value_exec_call_typed_metadata_guard`,
  `ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, ...)`, and
  `"typed inline struct direct call metadata drift"`.
- WSL GCC focused CTest:
  `cli_project_incremental|cli_aot_compacted_metadata_sidecar|aot_runtime_typed_direct_call_compatibility|aot_c_generic_call_typed`
  passed 4/4.
- WSL clang focused CTest: same set passed 4/4.
- Windows MSVC Debug focused CTest: same set passed 4/4.

## Notes

- No production AOT generator code changed in this sub-slice.
- The test now documents the intended intersection of 12-S8E and 11-S6H: full-AOT closes missing generic-instance
  fallback, while metadata-drift fallback remains a typed-boundary ABI safety path.
