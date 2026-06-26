# 2026-06-24 AOT 08-S7A Full-AOT Generic CALL_TYPED

## Scope

08-S7A covers the first verifiable full-AOT switch for shared-reference generic `CALL_TYPED`:

- Default hybrid mode still emits the 08-S6A missing-instance deopt bridge.
- `requireFullAot = ZR_TRUE` suppresses that generated deopt branch for statically collected callsites.
- The full-AOT generated shared library still executes the collected static instance and matches the interpreter result.

This does not close full 08-S7. Compile-time diagnostics for uncollected generic instances still need the 12-S8
reachability/manifest/full-AOT closure graph.

## Implementation

- `zr_vm_parser/include/zr_vm_parser/writer.h`
  adds `SZrAotWriterOptions.requireFullAot`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot.c`
  exposes `backend_aot_option_require_full_aot()`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c` and
  `backend_aot_c_function_body.{h,c}` pass the option into function body generation.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir.{h,c}` and
  `backend_aot_c_value_semir_calls.{h,c}` pass and consume the full-AOT flag.
- In full-AOT mode, shared generic `CALL_TYPED` emits `zr_aot_generic_call_typed_full_aot_no_deopt`, retains
  `ZrAot_GenericSlot_Method()` plus `ZrLibrary_AotRuntime_CallInlineStruct()`, and uses `ZR_AOT_C_FAIL()` if the
  METHOD slot is null instead of calling `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge()`.
- `tests/parser/test_aot_c_generic_call_typed.c` adds a source-level full-AOT generated shared-library execution test.

## RED / GREEN

RED:

- After 08-S6A, every shared generic `CALL_TYPED` callsite generated a missing-instance deopt branch.
- There was no writer option that represented full-AOT mode for generic callsite generation.

GREEN:

- `zr_vm_aot_c_generic_call_typed_test` passes 6/0.
- The full-AOT generated C contains:
  - `zr_aot_generic_call_typed_shared_callsite`
  - `zr_aot_generic_call_typed_full_aot_no_deopt`
  - `if (zr_aot_generic_call_typed_method == ZR_NULL)`
- The same generated C does not contain:
  - `zr_aot_generic_call_typed_missing_instance_deopt`
  - `ZrLibrary_AotRuntime_CallInlineStructDynamicDeoptBridge(state,`
  - `"generic call typed missing AOT instance"`
- The generated shared library executes through AOT C and returns the same `42` as the interpreter.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_typed_test`
  - 6 tests, 0 failures

## Acceptance Decision

Accepted as 08-S7A only. The full-AOT writer switch now affects generated generic `CALL_TYPED` code and suppresses the
hybrid missing-instance deopt branch for statically collected callsites. Full 08-S7 still requires compile-time
diagnostics when the reachable generic instance set is incomplete.
