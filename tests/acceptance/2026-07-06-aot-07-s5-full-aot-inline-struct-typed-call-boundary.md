# AOT 07-S5 Full-AOT Inline Struct Typed-Call Boundaries

Timestamp: 2026-07-06 02:45:48 +08:00

## Scope

- Removed the typed direct-call metadata guard from full-AOT inline-struct `CALL_TYPED` routes.
- The full-AOT shared generic and ordinary static callsites now emit
  `/* zr_aot_value_exec_call_typed_inline_struct_full_aot_direct */` and calls
  `ZrLibrary_AotRuntime_CallInlineStruct(state, ...)` with the fixed AOT thunk.
- Non-full-AOT callsites still keep `CanUseTypedDirectCall` and
  `CallInlineStructDynamicDeoptBridge` for metadata drift and missing-instance safety.

## Assertions

- Present in the targeted generated C:
  `/* zr_aot_value_exec_call_typed_inline_struct_full_aot_direct */`.
- Present in the targeted generated C:
  `ZrLibrary_AotRuntime_CallInlineStruct(state,`.
- Absent from the targeted full-AOT generated C:
  `/* zr_aot_value_exec_call_typed_metadata_guard */`,
  `ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame,`,
  `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,`,
  `typed inline struct direct call metadata drift`, and
  `generic call typed missing AOT instance`.

## RED/GREEN

- RED: WSL GCC `zr_vm_aot_c_generic_call_typed_test` failed 7/1 because the full-AOT inline-struct route did not
  emit `/* zr_aot_value_exec_call_typed_inline_struct_full_aot_direct */`.
- RED: WSL GCC `zr_vm_aot_c_call_shared_library_smoke_test` then failed 5/1 after the ordinary static inline-struct
  call smoke was switched to full AOT and required the same direct marker.
- GREEN: `backend_aot_try_write_c_value_semir_call_typed_exec()` now bypasses the metadata guard/deopt writer in both
  `requireFullAot` inline-struct branches and emits the fixed inline-struct helper call directly.

## Validation

- WSL GCC generic call typed:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_typed_test`
  - Result: 7 tests, 0 failures, 0 ignored.
- WSL Clang generic call typed:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_generic_call_typed_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_generic_call_typed_test`
  - Result: 7 tests, 0 failures, 0 ignored.
- WSL GCC call shared-library smoke:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_call_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test`
  - Result: 5 tests, 0 failures, 0 ignored.
- WSL Clang call shared-library smoke:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_call_shared_library_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test`
  - Result: 5 tests, 0 failures, 0 ignored.
- WSL GCC typed-call contracts:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- WSL Clang typed-call contracts:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- WSL GCC value SemIR contracts:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_value_semir_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_value_semir_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- WSL GCC source contracts:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test`
  - Result: 24 tests, 0 failures, 0 ignored.
- MSVC Debug generic call typed:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --target zr_vm_aot_c_generic_call_typed_test --config Debug -- /m:1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_call_typed_test.exe`
  - Result: 7 tests, 0 failures, 3 expected Unix-only ignores.
- MSVC Debug call shared-library smoke:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --target zr_vm_aot_c_call_shared_library_smoke_test --config Debug -- /m:1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe`
  - Result: 5 tests, 0 failures, 5 expected Unix-only ignores.
- MSVC Debug value SemIR and source contracts:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --target zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_source_contracts_test --config Debug -- /m:1`
  - Result: value SemIR contracts 4/0; source contracts 24/0.
