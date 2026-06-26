# AOT 08-S4 Generic Reference Sharing

Time: 2026-06-24 12:14:13 +08:00

Status: accepted for 08-S4. 08-S5 generic `CALL_TYPED`, 08-S6 dynamic-instance deopt, and 08-S7 full-AOT missing-instance diagnostics remain open.

## Scope

- Add a public AOT generic dictionary ABI matching the 08 plan: slot layout, mutable lazy cache, dictionary instance, and MethodInfo attachment.
- Add runtime lazy resolution for TYPE_LAYOUT and SIZEOF dictionary slots.
- Emit shared-reference generic C shape: multiple reference-type closed instances get per-instance dictionaries while sharing one `zr_fn_<base>__shared` function.
- Keep value-type generic monomorphization dictionary-free.

## Implementation

- `zr_vm_common/include/zr_vm_common/zr_aot_abi.h`
  - Bumped `ZR_VM_AOT_ABI_VERSION` to 4.
  - Added `EZrAotGenericSlotKind`, `SZrAotGenericSlot`, `SZrAotGenericResolvedSlot`, and `SZrAotGenericDictionary`.
  - Added `genericDictionary` to `SZrAotMethodInfo`.
- `zr_vm_library/include/zr_vm_library/aot_runtime.h`
  - Published `ZrLibrary_AotRuntime_GenericSlot_TypeLayout()` and `ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf()`.
- `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c`
  - Resolves TYPE_LAYOUT from a static slot layout or `ZrCore_Function_ResolvePrototypeFrameTypeLayout()`.
  - Resolves SIZEOF from the same `SZrTypeLayout` source and stores the result in the dictionary cache.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.{h,c}`
  - Scans closed reference-like generic typed locals.
  - Emits `zr_aot_generic_dictionary_table`, per-instance dictionaries, and one shared target per generic base.
  - Provides MethodInfo dictionary id lookup for the emitter.
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  - Emits `ZrAot_GenericSlot_*` access macros.
  - Emits generic sharing tables before MethodInfo.
  - Writes `.genericDictionary = &zr_aot_generic_dict_N` for functions with shared reference generic instances.
- `tests/parser/test_aot_c_generic_reference_sharing.c`
  - Verifies lazy slot cache behavior directly.
  - Verifies generated C for `Box<RefA>` and `Box<RefB>` has two dictionaries, one `zr_fn_box__shared`, MethodInfo dictionary attachment, and no monomorphized marker.
  - Compiles the generated C as a shared library.

## RED/GREEN

- RED: new `zr_vm_aot_c_generic_reference_sharing_test` failed to compile because `SZrAotGenericSlot`, `SZrAotGenericResolvedSlot`, `SZrAotGenericDictionary`, `ZR_AOT_GENERIC_SLOT_*`, and the runtime lazy APIs did not exist.
- GREEN: after ABI/runtime/emitter implementation, the focused test passed 2/0 and the generated C compiled to a shared library.

## Validation

- `zr_vm_aot_c_generic_reference_sharing_test`: 2/0.
- CTest `aot_c_generic_reference_sharing`: 1/1.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
- `zr_vm_aot_c_source_contracts_test`: 19/0.
- `zr_vm_aot_c_method_info_signature_test`: 1/0.
- `zr_vm_aot_c_generic_monomorphization_test`: 1/0.

Additional probe: `zr_vm_aot_c_shared_library_smoke_test` still has one pre-existing/generated-product text assertion that does not find `zr_aot_arith_exec` in its numeric arithmetic case. That failure is not on the 08-S4 dictionary path and was not used as the 08-S4 acceptance gate.

## Acceptance Decision

Accepted. 08-S4 closes the generic dictionary ABI, lazy slot cache, MethodInfo attachment, and shared-reference codegen shape. It intentionally does not implement dictionary METHOD-slot generic `CALL_TYPED`; that is the next planned slice, 08-S5.
