# 2026-06-24 AOT 08-S5A Generic CALL_TYPED METHOD Slot

## Scope

08-S5A covers the carrier and generated-C contract for dictionary-backed generic method calls:

- Runtime lazy resolution and caching for `ZR_AOT_GENERIC_SLOT_METHOD`.
- Generated `ZrAot_GenericSlot_Method(dict, slot)` access macro.
- Shared-reference generic dictionary METHOD slots carrying an `FZrAotEntryThunk`.
- Shared generic generated code that fetches and invokes a METHOD slot.
- Monomorphized value-generic wrapper marker for direct concrete specialization calls.

This does not close full 08-S5. Source-level generic `CALL_TYPED` route selection and AOT/interpreter result equality remain 08-S5B.

## Implementation

- `zr_vm_library/include/zr_vm_library/aot_runtime.h`
  exposes `ZrLibrary_AotRuntime_GenericSlot_Method()`.
- `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c`
  resolves `ZR_AOT_GENERIC_SLOT_METHOD` from `SZrAotGenericSlot.staticMethod` and caches it in
  `SZrAotGenericResolvedSlot.value.method`.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c`
  emits `ZrAot_GenericSlot_Method`, a METHOD slot for each shared reference generic dictionary, and a shared-body
  METHOD-slot lookup/call.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_monomorphization.c`
  marks monomorphized wrappers with `zr_aot_generic_call_typed_monomorphized_direct`.
- `tests/parser/test_aot_c_generic_call_typed.c`
  validates runtime METHOD-slot caching plus generated monomorphized/shared call shapes and compiles the generated C
  shared library.

## RED / GREEN

RED:

- Adding `zr_vm_aot_c_generic_call_typed_test` first failed to link because
  `ZrLibrary_AotRuntime_GenericSlot_Method` was not declared or implemented.

GREEN:

- `zr_vm_aot_c_generic_call_typed_test` passed 2/0 after adding the helper and codegen.
- The generated C contains:
  - `zr_aot_generic_call_typed_monomorphized_direct`
  - `ZrAot_GenericSlot_Method(dict, 1u)`
  - `ZR_AOT_GENERIC_SLOT_METHOD`
  - `.staticMethod = zr_aot_generic_dict_1_method_0`
  - `zr_aot_generic_method_1(state)`
- The generated C compiles as a shared library under the WSL GCC toolchain.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_typed_test`
  - 2 tests, 0 failures
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
- `wsl.exe --cd /mnt/e/Git/zr_vm sh -lc 'ctest --test-dir build-wsl-gcc -R "aot_c_generic_(call_typed|reference_sharing|monomorphization)" --output-on-failure'`
  - 3/3 tests passed

## Acceptance Decision

Accepted as 08-S5A only. The METHOD-slot carrier and generated shared/monomorphized call shapes are in place and
regression-covered. Full 08-S5 still requires source-level generic `CALL_TYPED` route selection with AOT/interpreter
result equality.
