# AOT 07-S5 Dynamic Value-Access Compact Deopt Bridge

Timestamp: 2026-07-06 03:11:05 +08:00

## Scope

- Compact dynamic member/index value-access deopt bridges from two generated guard statements into one guarded
  expression.
- Preserve the visible SemIR deopt validation through `ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(...)`.
- Keep the existing member/index runtime helpers and unsupported dynamic value-access fallback behavior.

## Assertions

- Dynamic member and index generated C contain `zr_aot_value_dynamic_deopt_bridge_compact`.
- The generated guard shape is:
  `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(...) && ZrLibrary_AotRuntime_GetMember/GetByIndex(...));`.
- Source contracts lock the compact bridge marker and the member/index helper calls without a second nested
  `ZR_AOT_C_GUARD(...)`.

## RED/GREEN

- RED: WSL GCC `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` failed 7/1 after the smoke started requiring
  `zr_aot_value_dynamic_deopt_bridge_compact`.
- GREEN: `backend_aot_write_c_dynamic_value_access_deopt_bridge()` now opens the compact guard, and each direct
  dynamic member/index writer closes it with its runtime helper call.

## Validation

- WSL GCC dynamic deopt bridge smoke:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test`
  - Result: 7 tests, 0 failures, 0 ignored.
- WSL GCC global contracts:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_global_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test`
  - Result: 9 tests, 0 failures, 0 ignored.
- WSL Clang dynamic deopt bridge smoke:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test`
  - Result: 7 tests, 0 failures, 0 ignored.
- WSL Clang global contracts:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_global_contracts_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_global_contracts_test`
  - Result: 9 tests, 0 failures, 0 ignored.
- MSVC Debug dynamic deopt bridge smoke:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test --config Debug -- /m:1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_dynamic_deopt_bridge_smoke_test.exe`
  - Result: 7 tests, 0 failures, 7 expected Unix-only ignores.
- MSVC Debug global contracts:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --target zr_vm_aot_c_global_contracts_test --config Debug -- /m:1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_global_contracts_test.exe`
  - Result: 9 tests, 0 failures, 0 ignored.
- Generated C inspection:
  `dynamic_member_deopt_bridge.c` and `dynamic_index_deopt_bridge.c` contain the compact marker and one
  `ValidateDynamicDeoptBridge(...) && GetMember/GetByIndex(...)` guard.
