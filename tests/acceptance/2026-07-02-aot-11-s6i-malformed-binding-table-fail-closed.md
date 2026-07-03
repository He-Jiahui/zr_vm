# AOT 11-S6I Malformed Binding Table Fail-Closed

Timestamp: 2026-07-02 08:28:32 +08:00

## Scope

This slice tightens the 11-S6 no-crash ABI drift guard for malformed function metadata binding tables.

If a generated or imported function advertises a nonzero `moduleMetadataBindingLength` but leaves
`moduleMetadataBindings` null, the metadata binding compatibility check now rejects the function as an invalid
argument. Typed direct-call eligibility consumes that failure and deopts instead of treating the malformed caller as
compatible.

This does not claim full 11-S6 closure. Cross-module token resolve integration and broader end-to-end ABI drift
injection remain open.

## Baseline / RED

- Added `test_function_binding_compatibility_rejects_nonzero_count_with_null_binding_table`, requiring
  `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility()` to return
  `ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT` for nonzero length plus null binding table.
- Added `test_typed_direct_call_guard_deopts_on_malformed_caller_binding_table`, requiring
  `ZrLibrary_AotRuntime_CanUseTypedDirectCall()` to reject a malformed caller binding table.
- Initial RED on WSL GCC failed metadata runtime binding compatibility with `16 Tests 1 Failure`; the old scan broke out
  of the loop on a null binding table and returned compatible.

## Implementation

- `zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c` now validates the binding table shape before the
  scan loop.
- A function with nonzero `moduleMetadataBindingLength` and null `moduleMetadataBindings` now fills the optional report
  with `ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT` and returns that status.
- The previous loop-time null-table break was removed so malformed tables cannot silently pass as compatible.
- The existing typed direct-call guard already treats non-compatible binding status as ineligible, so no additional AOT
  runtime production change was required.

## Test Inventory

- `tests/module/test_metadata_runtime_binding_compatibility.c`
- `tests/module/test_aot_runtime_typed_direct_call_compatibility.c`
- `zr_vm_metadata_runtime_binding_compatibility_test`
- `zr_vm_aot_runtime_typed_direct_call_compatibility_test`

## Tooling Evidence

- WSL GCC: metadata runtime binding compatibility passed `16 Tests 0 Failures`; typed direct-call compatibility passed
  `4 Tests 0 Failures`.
- WSL clang: metadata runtime binding compatibility passed `16 Tests 0 Failures`; typed direct-call compatibility passed
  `4 Tests 0 Failures`.
- Windows MSVC Debug: metadata runtime binding compatibility passed `16 Tests 0 Failures`; typed direct-call
  compatibility passed `4 Tests 0 Failures`.

## Acceptance Decision

Accepted for 11-S6 support: malformed function binding tables now fail closed at the metadata runtime boundary, and the
typed direct-call runtime guard deopts instead of taking a direct call when the caller metadata binding table is
malformed.
