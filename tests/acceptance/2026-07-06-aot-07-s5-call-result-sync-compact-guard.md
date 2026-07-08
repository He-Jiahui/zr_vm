# AOT 07-S5 call-result runtime helper sync compact guard

## Summary

Completed the focused 07-S5 slice that compacts call-result runtime helper sync for `CallStackValue`,
`CallStaticDirect`, and `CallDynamicDeoptBridge`. When a call result must refresh a typed scalar local, generated C now
uses `zr_aot_call_result_sync_compact` and emits one `ZR_AOT_C_GUARD(Call* && Sync*Local)` expression. Call boundaries
without typed destination sync keep the old single-helper guard and do not emit the compact marker.

## RED

- WSL GCC `zr_vm_aot_c_call_contracts_test`: 8 tests / 3 failures after quickened dynamic, generic function, and static
  direct call contracts required `zr_aot_call_result_sync_compact` and forbade standalone `Sync*Local` guards.
- An intermediate executable smoke assertion on a dynamic callable result exposed an expected int / actual object
  mismatch; the generated-C smoke was moved to a static typed `maybeAdd` result path.

## GREEN

- WSL GCC and Clang: call contracts 8/0.
- WSL GCC and Clang: source contracts 24/0.
- WSL GCC and Clang: call shared-library smoke 5/0.
- WSL GCC and Clang: dynamic deopt bridge smoke 7/0.
- MSVC Debug: call contracts 8/0 and source contracts 24/0.
- MSVC Debug: call shared-library smoke 5 expected Unix-only ignores / 0 failures.
- MSVC Debug: dynamic deopt bridge smoke 7 expected Unix-only ignores / 0 failures.

## Generated C Evidence

- Static direct call generated C contains `zr_aot_call_result_sync_compact` and one
  `CallStaticDirect(...) && SyncSignedIntLocal(...)` guard.
- Dynamic deopt bridge generated C contains `zr_aot_call_result_sync_compact` and one
  `CallDynamicDeoptBridge(...) && SyncSignedIntLocal(...)` guard.
- Generic `CallStackValue` paths without typed destinations do not emit the compact marker.

## Changed Files

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_boundaries.c`
- `tests/parser/test_aot_c_call_contracts.c`
- `tests/parser/test_aot_c_call_shared_library_smoke.c`
- `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`

## Open Scope

This does not complete 07-S5. In/out writeback, GC roots/exports/frame cleanup, performance counters, and complete
07-S5 acceptance remain open.
