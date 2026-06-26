# 2026-06-24 AOT 08-S6A Generic CALL_TYPED Missing-Instance Deopt

## Scope

08-S6A covers the first executable missing-instance fallback for shared-reference generic `CALL_TYPED`:

- If a `ZR_AOT_GENERIC_SLOT_METHOD` resolves to an AOT thunk, generated C keeps the 08-S5 fast path.
- If the METHOD slot has no AOT thunk, generated C deopts the inline-struct typed call to the interpreter.
- The fallback preserves the inline struct return destination and matches interpreter results.

This does not close full 08-S6. Reflective `MakeGenericType` / runtime dynamic generic instance construction remains open,
as does 08-S7 full-AOT missing-instance diagnostics.

## Implementation

- `zr_vm_library/include/zr_vm_library/aot_runtime.h`
  exposes `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge()`.
- `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c`
  validates the deopt id, prepares a VM call window, copies value arguments out of the AOT frame, invokes the callable
  through the interpreter with an inline return destination, and restores the AOT frame.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c`
  emits `zr_aot_generic_call_typed_missing_instance_deopt` and calls the new bridge when
  `ZrAot_GenericSlot_Method()` returns `ZR_NULL`.
- `tests/parser/test_aot_c_generic_call_typed.c`
  verifies missing METHOD slots are not cached as resolved, checks the generated fallback shape, and executes a generated
  shared library whose METHOD slot is rewritten to `ZR_NULL`.

## RED / GREEN

RED:

- After 08-S5, a missing generic METHOD slot still failed with `ZR_AOT_C_FAIL()` and had no executable fallback path.

GREEN:

- `zr_vm_aot_c_generic_call_typed_test` passes 5/0.
- The generated C contains:
  - `zr_aot_generic_call_typed_missing_instance_deopt`
  - `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,`
  - `"generic call typed missing AOT instance"`
- The missing-instance test rewrites `.staticMethod = zr_aot_fn_*` to `.staticMethod = ZR_NULL`, rebuilds the generated
  shared library, executes through AOT, and receives the same `42` as the interpreter.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_typed_test`
  - 6 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test zr_vm_aot_c_generic_call_typed_test -j2`
  - targets built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test`
  - 2 tests, 0 failures

## Acceptance Decision

Accepted as 08-S6A only. The METHOD-slot missing AOT entry can now fall back to the interpreter for inline-struct typed
generic calls. Full 08-S6 still needs runtime dynamic generic instance construction, and 08-S7 still needs the full-AOT
missing-instance diagnostic mode.
