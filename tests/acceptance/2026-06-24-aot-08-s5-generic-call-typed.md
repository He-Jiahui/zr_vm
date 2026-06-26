# 2026-06-24 AOT 08-S5 Generic CALL_TYPED

## Scope

08-S5 closes the planned generic `CALL_TYPED` dual shape:

- Value/monomorphized generic calls keep the direct concrete `zr_aot_fn_*` path.
- Shared reference generic calls resolve a `ZR_AOT_GENERIC_SLOT_METHOD` entry through a generic dictionary.
- Source-level generic calls now exercise the generated METHOD-slot callsite, not only the carrier shape.
- AOT execution of the generated shared library matches the interpreter result.

08-S6 dynamic-instance deopt and 08-S7 full-AOT missing-instance diagnostics remain open.

## Implementation

- `zr_vm_library/include/zr_vm_library/aot_runtime.h` and
  `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c`
  provide `ZrLibrary_AotRuntime_GenericSlot_Method()` for lazy METHOD-slot resolution and cache.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c`
  detects reference-generic `CALL_TYPED` sites from callee parameter metadata, emits a callsite-local
  `ZR_AOT_GENERIC_SLOT_METHOD` dictionary, resolves the method with `ZrAot_GenericSlot_Method()`, and passes the thunk
  to `ZrLibrary_AotRuntime_CallInlineStruct()`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c` and
  `backend_aot_c_value_semir.{h,c}` thread callee metadata into the value SemIR call writer.
- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c`
  recognizes current function/method generic parameters and builds script export metadata under the exported function
  declaration so `T` survives into typed parameter metadata.
- `tests/parser/test_aot_c_generic_call_typed.c`
  validates runtime METHOD-slot caching, generated monomorphized/shared call shapes, generated shared-library compile,
  and AOT/interpreter result equality for `func stamp<T>(value: T): Stamp where T: class`.

## RED / GREEN

RED:

- 08-S5A left only the carrier in place: generated METHOD slots existed, but source-level generic `CALL_TYPED` did not
  choose a shared METHOD-slot callsite and had no AOT/interpreter equality assertion.

GREEN:

- `zr_vm_aot_c_generic_call_typed_test` now passes 3/0.
- The reference generic call generated C contains:
  - `zr_aot_generic_call_typed_shared_callsite`
  - `ZrAot_GenericSlot_Method(&zr_aot_generic_call_typed_`
  - `ZR_AOT_GENERIC_SLOT_METHOD`
  - `.staticMethod = zr_aot_fn_`
  - `ZrLibrary_AotRuntime_CallInlineStruct(state,`
- The generated C compiles as a shared library and returns the same `42` as the interpreter.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test -j2`
  - target built successfully
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_typed_test`
  - 6 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_generic_reference_sharing_test`
  - 2 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_generic_monomorphization_test`
  - 1 test, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test`
  - 19 tests, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test`
  - 1 test, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test`
  - 1 test, 0 failures
- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_type_inference_test`
  - full suite passed
- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc "ctest --test-dir build-wsl-gcc -R 'aot_c_generic_(call_typed|reference_sharing|monomorphization)' --output-on-failure"`
  - 3/3 tests passed

## Acceptance Decision

Accepted as complete for 08-S5. Generic `CALL_TYPED` now has both planned shapes under test: direct monomorphized calls
and shared reference calls through dictionary METHOD slots, with generated shared-library execution matching the
interpreter. Dynamic reflection / uncollected generic instance deopt remains 08-S6, and full-AOT enforcement remains
08-S7.
