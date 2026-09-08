---
related_code:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/type_layout_initialization.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_inline_array.c
  - zr_vm_core/src/zr_vm_core/module/module_internal.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h
  - zr_vm_core/src/zr_vm_core/metadata_runtime.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_value_slot_fast.h
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame_copy_fast.h
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/bound_expression.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_value_construct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c
  - zr_vm_parser/src/zr_vm_parser/bound_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_conversion.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_conversion.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_bitwise.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_bitwise.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bool_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/core/test_type_layout_metadata_contracts.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/test_inline_struct_array_layout.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/core/test_native_inline_span_dispatch.c
  - tests/core/test_aot_gc_root_frame.c
  - tests/gc/gc_tests.c
  - tests/module/test_metadata_runtime_type_layout.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_struct_value_init.c
  - tests/parser/test_compiler_integration_main.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/type_layout_initialization.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_inline_array.c
  - zr_vm_core/src/zr_vm_core/module/module_internal.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/include/zr_vm_core/metadata_runtime.h
  - zr_vm_core/src/zr_vm_core/metadata_runtime.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_layout_binding.c
  - zr_vm_core/src/zr_vm_core/metadata_runtime_binding_compatibility.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c
  - zr_vm_core/src/zr_vm_core/function_precall_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_inline_frame.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/bound_expression.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_value_construct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c
  - zr_vm_parser/src/zr_vm_parser/bound_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_call.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_semir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_conversion.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_conversion.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_bitwise.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_bitwise.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_bool_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_typed_direct_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layout_tokens.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_control.c
plan_sources:
  - user: 2026-05-16 struct inline stack storage and memcpy parameter passing
  - user: 2026-05-18 real GC/native entry wiring without claiming full ABI completion
  - user: 2026-06-04 align struct value execution with lua/hybridclr and lua/il2cpp architecture
  - user: 2026-08-04 retest and accept the strict Syntax cutover before commit
  - user: 2026-08-29 audit and optimize VM performance against Lua and C#
  - docs/plans/aot/03-instruction-set-refactor.md
  - docs/plans/aot/04-semir-and-c-backend.md
  - docs/plans/aot/06-implementation-blueprint.md
  - docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md
tests:
  - tests/core/test_type_layout_inline_copy.c
  - tests/core/test_type_layout_metadata_contracts.c
  - tests/core/test_frame_slot_layout_lookup.c
  - tests/core/test_inline_struct_array_layout.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/core/test_native_inline_span_dispatch.c
  - tests/container/test_pooling_closed_type_runtime.c
  - tests/gc/gc_tests.c
  - tests/module/test_metadata_runtime_binding_compatibility.c
  - tests/module/test_aot_runtime_typed_direct_call_compatibility.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/module/test_metadata_runtime_typespec_layout.c
  - tests/module/test_metadata_runtime_type_layout.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_struct_value_init.c
  - tests/parser/test_compiler_integration_main.c
  - tests/parser/test_value_type_runtime.c
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_aot_c_type_layout_contracts.c
  - tests/parser/test_aot_c_source_contracts.c
  - tests/parser/test_aot_c_call_contracts.c
  - tests/parser/test_aot_c_value_semir_contracts.c
  - tests/parser/test_aot_c_typed_direct_call_shared_library_smoke.c
  - tests/parser/test_aot_c_typed_direct_call_u64_shared_library_smoke.c
  - tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c
  - tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c
  - tests/parser/test_aot_c_code_stripping.c
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c
  - tests/parser/test_aot_c_generic_call_typed.c
  - tests/parser/test_semir_type_conflict_deopt.c
  - tests/parser/test_semir_dynamic_arithmetic_deopt.c
  - tests/parser/test_semir_dynamic_member_deopt.c
  - tests/parser/test_semir_dynamic_call_deopt.c
  - tests/parser/test_semir_dynamic_iter_deopt.c
  - tests/parser/test_semir_dynamic_index_deopt.c
  - tests/parser/test_aot_c_typed_scalar.c
  - tests/acceptance/2026-05-16-inline-struct-byte-stack.md
  - tests/acceptance/2026-08-29-frame-slot-layout-dense-lookup.md
  - tests/acceptance/2026-05-18-inline-frame-gc-native-entry.md
  - tests/acceptance/2026-06-20-aot-m1-semir-type-conflict-deopt.md
  - tests/acceptance/2026-06-20-aot-m1-semir-dynamic-index-deopt.md
  - tests/acceptance/2026-06-20-aot-m2-typed-scalar-i64.md
  - tests/acceptance/2026-06-25-aot-11-s4g-gc-inline-frame-runtime-layout-resolver.md
  - tests/acceptance/2026-06-25-aot-11-s4j-typespec-layout-binding-view.md
  - tests/acceptance/2026-06-25-aot-11-s4k-type-token-layout-cache.md
  - tests/acceptance/2026-06-25-aot-11-s4l-layout-id-token-reverse-cache.md
  - tests/acceptance/2026-06-25-aot-11-s4m-multi-entry-type-layout-cache.md
  - tests/acceptance/2026-06-25-aot-11-s4n-ctype-id-token-resolver.md
  - tests/acceptance/2026-06-25-aot-11-s4o-type-layout-token-carrier.md
  - tests/acceptance/2026-06-26-aot-11-s4p-generated-type-layout-token-population.md
  - tests/acceptance/2026-06-26-aot-11-s4q-generated-typespec-type-layout-token-population.md
  - tests/acceptance/2026-06-26-aot-11-s4r-generated-ownership-offset-table.md
  - tests/acceptance/2026-06-27-aot-11-s6a-runtime-binding-compatibility.md
  - tests/acceptance/2026-06-28-aot-11-s6b-function-binding-compatibility-scan.md
  - tests/acceptance/2026-06-28-aot-11-s6c-dynamic-loader-binding-reject.md
  - tests/acceptance/2026-06-28-aot-11-s6d-i64-typed-direct-call-deopt.md
  - tests/acceptance/2026-06-28-aot-11-s6e-u64-typed-direct-call-deopt.md
  - tests/acceptance/2026-06-28-aot-11-s6f-f64-typed-direct-call-deopt.md
  - tests/acceptance/2026-06-28-aot-11-s6g-bool-typed-direct-call-deopt.md
  - tests/acceptance/2026-06-28-aot-11-s6h-inline-struct-typed-call-deopt.md
  - tests/acceptance/2026-07-06-aot-09-s2-local-address-root-runtime-support.md
  - tests/acceptance/2026-06-26-aot-12-s7l-type-layout-payload-byte-trim-delta.md
  - tests/acceptance/2026-08-03-syntax-09-m3-canonical-pool-layout.md
doc_type: module-detail
last_verified: 2026-08-30
---

# Inline Type Layout And Byte Stack Copy

## Frame-Slot Lookup Metadata Contract

`SZrFunction.frameSlotLayouts` must contain at most one entry for each
`stackSlot`. Compiler-built functions create the unique canonical entry at
`frameSlotLayouts[stackSlot]`, and IO validation rejects duplicate slots.
Hand-built `SZrFunction` values must satisfy the same uniqueness contract.

This module is the first runtime layer for moving `struct` values toward inline, byte-sized stack storage. It does not replace the interpreter's existing fixed-slot frame ABI yet. It provides the typed layout and byte-offset stack primitives that later call-frame migration can use instead of directly assuming `functionBase + slot`.

## Type Layout

`SZrTypeLayout` describes the byte shape of a value stored inline:

- `byteSize` and `byteAlign` describe storage requirements.
- `copyKind` selects POD raw copy versus field-aware copy.
- `dropKind` selects no-op drop versus field-aware drop.
- `gcScanKind` distinguishes pointer-free, mapped, and barriered layouts.
- `fields` describes managed subfields such as embedded `SZrTypeValue` slots.
- `blittable` records the computed raw-copy eligibility used by `ZrCore_TypeLayout_CanRawCopy`.
- `cTypeId` is the stable generated-C type identifier reserved for AOT layout emission.
- `layoutVersion` and `layoutHash` freeze the schema-v1 structural identity consumed by VM, metadata runtime, AOT, artifact roundtrip, and reflection.
- `gcFieldOffsets`, `ownershipFieldOffsets`, and `refFieldOffsets` carry precomputed managed subfield locations for
  generated C, GC scanning, and ownership/drop lowering.

POD layouts use `memmove` through `ZrCore_TypeLayout_CopyInline`, so overlapping source and destination spans are safe. Field-copy layouts copy unmanaged byte ranges directly and route `SZrTypeValue` fields through the existing value/ownership copy path. Field-drop layouts release only embedded value slots marked with `ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE` through `ZrCore_Ownership_ReleaseValue`; GC-only value fields stay available for mark/rewrite visitors without being treated as owned storage.

`ZrCore_TypeLayout_InitStructWithMetadata` is the AOT-facing initializer for these generated-C metadata fields. The existing `ZrCore_TypeLayout_InitStruct` entry remains the compatibility path and initializes neutral metadata (`cTypeId == 0`, no offset tables). Metadata field counts are still derived from the layout field flags, so the offset tables must describe the same field model as the canonical `SZrTypeLayout`.

Generated AOT C descriptors now preserve that contract for owner-field struct layouts: when a struct layout has ownership fields whose offsets are present or can be derived from `ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT | ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE` fields, the emitter writes a static `ZrOwnershipOffsets_<typeLayoutId>[]` table and points `.ownershipFieldOffsets` at it. Zero-count, union, and unsafe offset cases keep `ZR_NULL` so consumers do not infer ownership locations that the generated metadata cannot prove.

`ZR_TYPE_LAYOUT_KIND_VALUE` is a special layout for a standalone `SZrTypeValue`. It always copies through `ZrCore_Value_Copy`, so boxed struct objects still clone instead of being raw-copied by pointer. Dropping a value layout releases the value slot through the ownership runtime.

`ZR_TYPE_LAYOUT_KIND_UNION` is the matching inline layout kind for tagged union values. The layout records the tag byte offset and size, and managed fields carry an `activeTag`. Copy, drop, and GC visitors only process fields whose `activeTag` matches the currently stored union tag. Union copy first drops the destination's old active value payload, then copies non-active byte ranges and active `SZrTypeValue` payload fields through the normal value copy path.

Schema-v1 layout initialization computes the hash from the size, alignment, kind, copy/drop/scan classifications, field spans, nested layout indices, active tags, and all three maps. `ZrCore_TypeLayout_Validate` rejects invalid alignment, out-of-range or overlapping field spans, inconsistent map counts, and a hash that does not match the structural payload. AOT C descriptors emit the same version, hash, copy/drop/scan kinds, and map offsets; metadata runtime and reflection consume the attached registry instead of deriving a second layout.

`ZrCore_TypeLayout_InitializeStorageWithRegistry` recursively initializes nested fields and embedded value slots before a destination becomes live. Registry-aware copy rejects move-only nested layouts. Full drop walks fields in reverse declaration order and can run custom drop before field teardown. `ZrCore_TypeLayout_DropPartialInlineWithRegistry` consumes a constructor initialization bitmap and drops only completed fields, also in reverse declaration order.

## Destination-First Struct Initialization

Source `init TypeRef(arguments)` has a distinct `ZR_AST_STRUCT_INIT_EXPRESSION`; it is not the existing object/new/ownership construct node. The parser enters the TypeRef grammar immediately after the contextual `init`, preserving qualified and constructed generic targets plus named argument syntax. `SZrBoundValueConstruct` resolves one canonical struct constructor, maps named arguments to parameters, synthesizes defaults, and uses a dedicated synthesized-default constructor identity only when the type declares no explicit constructor. Ordinary calls never retry this binder, and value construction never falls back to `@call` or class allocation.

The compiler lowers the bound form to `ZR_SEMANTIC_IR_VALUE_CONSTRUCT` with a destination `PlaceId`, `TypeId`, and constructor `SymbolId`. Local declarations, inline fields, fixed-array elements, and return storage provide their final destination before argument or constructor lowering, so normal struct construction does not allocate an object wrapper or perform prototype dispatch. Explicit constructor receivers are indirect aliases of that final inline storage. Field stores also emit `FIELD_INITIALIZE`, allowing semantic flow and the runtime constructor bitmap to describe partial initialization without reconstructing it from ExecBC.

Constructor frame metadata reserves a bitmap tail under `ZR_FUNCTION_FRAME_SLOT_FLAG_CONSTRUCTOR_INITIALIZATION_BITMAP`. Successful top-level field stores set the corresponding bit. VM exception unwinding resolves the receiver layout and invokes partial reverse drop for those bits; generated AOT C emits the same cleanup contract. Resuming a caller-side catch after a generated direct AOT callee throws still depends on the broader AOT cross-function exception protocol and remains a Syntax 04 promotion gate, not an M1 claim.

## Byte Stack Primitives

`ZrCore_Stack_SaveByteAddressAsOffset` and `ZrCore_Stack_LoadByteOffsetToAddress` convert between raw stack byte addresses and offsets from `state->stackBase`. `ZrCore_Stack_CopyInline` copies a layout-sized byte span between two stack byte offsets after checking that both ranges fit inside the current stack allocation.

`SZrStackFramePlace` is the typed place bridge between function frame metadata and raw stack bytes. `ZrCore_Stack_MakeFramePlace` resolves a frame base plus a frame-relative byte offset into a checked stack address, absolute stack byte offset, byte size, and relative alignment. `ZrCore_Stack_CopyInlinePlace` then copies between two checked places through `SZrTypeLayout`, rejecting too-small places before reaching the raw copy path.

`ZrCore_Function_MakeFrameSlotPlace` is the function-metadata layer over those raw stack places. It looks up a logical stack slot in `SZrFunctionFrameSlotLayout`, then resolves the stored byte offset and byte span against a caller-provided frame value base. `ZrCore_Function_CopyFrameSlotInline` copies between source and destination frame slots only when the runtime layout kind matches the slot kind, so struct layouts cannot accidentally raw-copy into legacy `SZrTypeValue` slots.

These APIs intentionally coexist with the old slot APIs. Existing instructions, closure captures, and return movement still use `TZrStackValuePointer` slot arithmetic. New code that needs inline struct payloads should go through byte offsets and `SZrTypeLayout` instead of adding more raw slot assumptions.

## Prototype Layout Metadata

Compiled prototype metadata now carries `layoutByteSize` and `layoutByteAlign` in `SZrCompiledPrototypeInfo`. `compiler_struct.c` computes these values from the same field offset pass that assigns `fieldOffset` and `fieldSize`: each non-static struct field is aligned, the running size advances by the field byte size, the maximum field alignment is recorded, and the final struct size is rounded up to that maximum alignment.

The fields are serialized into `function->prototypeData`, imported back into parser type prototypes, copied into runtime `SZrObjectPrototype`, and surfaced through debug/intermediate prototype printing and `%type` reflection layout objects. Runtime reflection now prefers the stored whole-struct layout and only falls back to deriving layout from fields when a prototype was produced before these fields existed or comes from a source that has not supplied them yet.

## Runtime Prototype Type Layout Resolver

`ZrCore_Function_ResolvePrototypeFrameTypeLayout` is the current runtime bridge from `SZrFunctionFrameSlotLayout.typeLayoutId` to `SZrTypeLayout`. In this increment the id is still a checked prototype index, not a standalone serialized type-layout table id. The resolver reads the owning entry function's `prototypeData`, validates the encoded prototype count and byte bounds, and builds a per-function cache of layouts.

For AOT-loaded functions that have an attached code registration, GC inline-frame mark/rewrite now resolves the same `typeLayoutId` through `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout`. That path reads the code-registration layout registry attached to the function or its prototype-context entry function, so AOT GC consumers use the same metadata runtime layout table as generic dictionary and GC descriptor lookup. When an AOT registry is present but a registry layout is missing, GC does not fall back to the prototype layout cache. When no AOT registry is attached, ordinary VM/interpreter inline-frame GC keeps using `ZrCore_Function_ResolvePrototypeFrameTypeLayout`.

`ZrCore_Function_GetPrototypeFrameTypeLayoutRegistry` exposes the ordinary
interpreter side of that contract as a stable borrowed registry. It resolves the
required prototype layout first, including recursively referenced local layouts,
then publishes one pointer table owned by the entry function. Repeated calls for
the same function return the same registry identity; invalid or unresolved ids
fail with a cleared view. The table and every cached layout remain valid until
the entry function releases its prototype-layout cache, so consumers must not
retain the view beyond that function lifetime.

Native inline argument binding uses this source registry only when no artifact
code registration is attached. Once a function or its prototype context carries
an artifact registration, the metadata-runtime registry is authoritative:
missing, corrupt, or mismatched artifact entries fail closed and never fall back
to the source prototype cache. Source `Pool<T>.deliver` can therefore use the
same canonical non-boxing inline view without weakening artifact validation.

`metadata_runtime_layout_binding.c` keeps the row-to-layout binding views separate from the main metadata runtime cache code. TypeDef and FieldDef binding views resolve their rows through the attached zrp metadata and the code-registration layout registry. TypeSpec binding now follows the same rule: a `TYPE_SPEC` token must match its zrp TypeSpec row and paired signature record, then the row's `typeLayoutId` resolves through `ZrCore_MetadataRuntime_ResolveTypeLayout`. `ZrCore_MetadataRuntime_ResolveTypeTokenLayout` wraps the TypeDef and TypeSpec binding views with a public token-level resolver. `ZrCore_MetadataRuntime_ResolveTypeLayoutToken` first checks bounded cache entries and, when present, `codeRegistration->typeLayoutTokens[typeLayoutId]`; accepted table entries must be TypeDef or TypeSpec tokens whose registry-backed layout resolves. If the table has no usable entry, it scans TypeDef/TypeSpec rows to reverse a registry-backed layout id back to its metadata token. `ZrCore_MetadataRuntime_ResolveCTypeIdToken` exposes the same reverse path under the current `cTypeId == typeLayoutId` registry invariant. Generated C now emits the token-table carrier as `zr_aot_type_layout_tokens[]`; entries for uniquely matched local TypeDef-backed named struct/union layouts carry real `TYPE_DEF` tokens, and current-function generated generic layouts whose type name structurally matches a unique `TYPE_SPEC` canonical signature carry real `TYPE_SPEC` tokens. Missing metadata, ambiguous matches, cross-module records, and unsupported signature shapes stay `0u`. Both directions share a bounded 8-entry cache on `SZrMetadataRuntime`, so TypeDef and TypeSpec token/layout hits can coexist instead of replacing only the latest hit. Missing registry layout data does not fall back to prototype layout cache. This is still a read-only binding/cache/carrier path; runtime construction of generic layouts and ownership-offset tables remains later work.

`metadata_runtime_binding_compatibility.c` keeps ABI drift checks out of the layout binding/cache module. `ZrCore_MetadataRuntime_CheckTokenBindingCompatibility` compares a recorded module version range from `SZrMetadataTokenRecord` with expected/resolved identity from `SZrMetadataTokenBinding`: module signature hash, metadata token, signature token/hash, and layout version/hash. `ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility` scans a function's attached module metadata bindings, resolves the ref record from local metadata first and module metadata second, then returns the first incompatible binding with the same status/report payload. The root AOT runtime loader now consumes that function scan after loading embedded/zro metadata and attaching the module metadata runtime: incompatible bindings reject dynamic module load before reflection/prototype materialization or runtime-record storage, and the error records the status plus token/hash/layout details. The current i64/u64/f64/bool scalar typed direct-call paths also ask the AOT runtime to scan both caller and callee bindings before invoking a direct thunk; on mismatch the generated guard deopts through the stack-call path and syncs the signed/unsigned/float/bool scalar result back into the generated local. Value SemIR inline-struct `CALL_TYPED` now uses the same caller/callee guard before `CallInlineStruct()` and falls back through `CallInlineStructDynamicDeoptBridge()` so interpreter execution can copy inline return bytes back into the generated destination slot. Cross-module token resolve integration and broader ABI-drift injection remain later AOT metadata work.

The resolver succeeds only when it can prove the inline representation is safe:

- POD structs become `ZR_TYPE_LAYOUT_COPY_KIND_POD` and `ZR_TYPE_LAYOUT_DROP_KIND_NONE` only when every non-lifecycle field is either a known primitive scalar with the exact expected byte size and in-bounds offset, a local nested struct whose layout resolves successfully, or absent from the field list.
- Struct/class fields marked as managed, owned, close-capable, or destructor-capable must fit a whole embedded `SZrTypeValue` inside the declared struct layout; those fields become field-aware GC/ownership entries.
- Union payload fields whose variant metadata records a value-sized storage slot become active-tag-managed `SZrTypeValue` fields. If the payload field has a non-zero ownership qualifier, the field is also marked for ownership release during drop.
- Builtin reference fields such as `string` and `object` are accepted only when the recorded field storage is exactly a whole embedded `SZrTypeValue`. They become GC value fields without an ownership metadata flag; pointer-sized reference fields fail resolution and preserve the boxed/old path.
- A field whose serialized type name resolves to another local struct prototype reuses that nested prototype layout. Managed nested `SZrTypeValue` fields are flattened into the parent layout with the parent field offset added, so GC/copy/drop visitors still see exact embedded value locations instead of scanning the whole nested byte span.
- Bad prototype ids, malformed prototype data, unsafe managed field sizes, recursive cache re-entry, unknown non-local field type names, pointer-sized reference fields, and imported layouts without explicit serialized type-layout metadata cache as failed and return `ZR_NULL`.

That failure mode is intentional. Callers must treat a `ZR_NULL` layout as "inline handling unavailable" and keep the boxed or older path rather than pretending the byte span has a proven lifecycle model.

## AOT C Layout Declarations

The AOT C backend now emits a declaration layer for proven inline struct layouts before function bodies are emitted. `backend_aot_c_type_layouts.c` scans the `SZrAotFunctionTable` for inline struct frame slots, resolves each unique `typeLayoutId` through `ZrCore_Function_ResolvePrototypeFrameTypeLayout`, and walks fields through `ZrCore_Function_VisitPrototypeFrameFieldLayouts`.

For each resolved struct layout the generated file contains a `ZrLayout_<typeLayoutId>` C type, explicit padding members, generated field members, and static assertions for `sizeof`, `_Alignof`, and every field `offsetof`. The generated `ZR_AOT_C_LAYOUT_STRUCT` macro carries metadata alignment into the C type so a layout whose runtime `byteAlign` is larger than the natural alignment of its current fields still fails or passes by the same metadata rule as the interpreter/runtime resolver.

TypeLayout schema v2 also emits the canonical cross-domain transfer kind, schema version/hash,
and provider token/hash into the generated runtime descriptor. The C backend does not infer these
values from a generated type name or field shape. A provider-backed layout containing GC,
ownership, or reference fields is rejected by the same runtime validation used by the VM; a
blittable GcFree layout may use the explicit or default `ValueCopy` contract.

This layer is a drift detector and type-shape anchor for later pure C lowering. It does not yet make struct operations themselves pure C: value SemIR field loads/stores and struct copy/call lowering still need their own slices before the full value-type shared-library smoke can execute without the existing `unsupported AOT value SemIR field` fallback.

## SemIR Static C Types

`SZrFunctionTypedTypeRef` now carries `staticCType` and `staticCTypeId` alongside the existing language type metadata. The compiler annotates SemIR type-table entries with an AOT-facing `EZrStaticCType` category for proven bool, integer, floating-point, GC reference, native pointer/data, and inline struct values. Inline struct entries use `staticCTypeId` to point at the same frame type-layout id that drives `ZrLayout_<typeLayoutId>` declaration emission.

The binary writer and runtime loader preserve these fields behind `ZR_IO_SOURCE_PATCH_HAS_SEMIR_STATIC_C_TYPES`, so generated C, parser tests, and loaded runtime functions observe the same static type table after `.zro` roundtrip. Older binaries load with dynamic/none annotations, and non-struct entries normalize the id to `ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE`.

This is only the type annotation layer for AOT C lowering. Conflict analysis, typed-block deopt insertion, and rejection of generic arithmetic opcodes in typed blocks remain separate instruction-set refactor slices.

## Typed Scalar SemIR

The SemIR layer now has typed scalar operation rows for arithmetic, comparisons, and the first bitwise/shift slice: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `EQ`, `NE`, `LT`, `LE`, `GT`, `GE`, `BIT_NOT`, `BIT_AND`, `BIT_OR`, `BIT_XOR`, `SHL`, and `SHR`. The compiler maps already-specialized numeric bytecode such as signed, unsigned, and floating-point arithmetic/comparison instructions into these SemIR opcodes. Typed bitwise and shift bytecode uses typed local static C type hints to keep signed and unsigned integer lowering explicit. Generic dynamic arithmetic bytecode remains outside this typed scalar path.

Scalar SemIR rows carry an explicit result `EZrStaticCType` when the destination is a temporary slot rather than a declared typed local. During SemIR table construction, that explicit static C type is resolved back to the function's `semIrTypeTable`, so a later C backend can choose the C operator and C storage type without re-reading VM value tags.

The first AOT C lowering slices now consume these rows for signed `i64` binary arithmetic and comparisons, unsigned `u64` binary arithmetic and comparisons, `f64` binary arithmetic, focused numeric conversions, and signed `i64` bitwise/shift expressions. `backend_aot_c_scalar_semir.c` matches the ExecIR instruction to its SemIR row, validates the frame-slot bounds and signed-int, unsigned-int, or float tags, emits divide/modulo zero checks for arithmetic, and writes integer, unsigned integer, bool, or double destinations through direct frame-slot value fields instead of `ZrCore_Stack_GetValue` plus `ZR_VALUE_FAST_SET`. `backend_aot_c_scalar_conversion.c` handles the first numeric conversion set for `TO_INT`, `TO_UINT`, and `TO_FLOAT` specialized variants by writing `nativeInt64`, `nativeUInt64`, or `nativeDouble` directly. `backend_aot_c_scalar_bitwise.c` handles focused `~`, `&`, `|`, `^`, `<<`, and `>>` emission with direct `nativeInt64` / `nativeUInt64` reads and writes plus shift-count bounds checks. `backend_aot_c_scalar_stack_copy.c` handles the first typed scalar local-copy slice for bool, signed `i64`, unsigned `u64`, and `f64` `GET_STACK` / `SET_STACK` instructions by dispatching direct frame-slot scalar copies before the older generic stack-copy fallback. `backend_aot_c_lowering_control.c` now handles the first typed branch slice for bool false and fused signed `i64` comparisons by reading `&frame.slotBase[slot].value` directly and emitting C `goto` branches without branch-specific `ZrCore_Stack_GetValue` or typed-place fallback code; when both fused signed branch operands are proven `i64` scalar locals, it synchronizes them into `zr_aot_sN` and emits `if (zr_aot_sL op zr_aot_sR)`.

`backend_aot_c_scalar_locals.c` is the first 04-S3 declaration slice. After generated frame setup and before value SemIR / bytecode dispatch emission, it scans `typedLocalBindings` and SemIR destination static C types to emit a `zr_aot_scalar_locals_begin` / `zr_aot_scalar_locals_end` block of `TZrBool zr_aot_bN`, `TZrInt64 zr_aot_sN`, `TZrUInt64 zr_aot_uN`, and `TZrFloat64 zr_aot_fN` locals. Scalar local kind tracking is a per-slot bitmask, so source typed-local evidence and SemIR destination evidence merge instead of replacing each other; a reused slot can therefore declare multiple C mirrors such as `zr_aot_s16` and `zr_aot_f16` when different lifetimes prove different static C types.

Signed `i64` binary arithmetic, signed `i64` comparisons, unsigned `u64` binary arithmetic, `f64` binary arithmetic, signed `i64` binary bitwise operations, the first signed `i64` shift, bit-not, and branch operand paths, and the first conversion source paths now have local-expression slices on top of those declarations. When arithmetic or bitwise destination and operands are proven declared scalar locals, the scalar lowering modules synchronize the frame-slot inputs into `zr_aot_sN`, `zr_aot_uN`, or `zr_aot_fN` locals, emit expressions such as `zr_aot_s2 = zr_aot_s0 * zr_aot_s1;`, `zr_aot_u8 = zr_aot_u6 + zr_aot_u7;`, `zr_aot_f28 = zr_aot_f16 * zr_aot_f17;`, and `zr_aot_s13 = zr_aot_s9 & zr_aot_s0;`, then mirror that local result back to the existing frame slot ABI. When a signed compare destination is a proven bool local and both operands are proven `i64` locals, the same module emits a bool local expression such as `zr_aot_b24 = (TZrBool)(zr_aot_s2 > zr_aot_s4);` before mirroring the bool result back to the frame slot. Fused signed branches now also split branch operand eligibility: if both branch operands are proven `i64` locals, generated C validates the frame-slot values, synchronizes them into `zr_aot_sN`, and branches on a local expression such as `if (zr_aot_s2 <= zr_aot_s4) {`. Signed `i64` shifts and bit-not now use the bitmask declarations to write destination locals for reused temporary slots when SemIR destination metadata proves `i64`, for example `zr_aot_s16 = (TZrInt64)((TZrUInt64)zr_aot_s12 << zr_aot_s1);`, `zr_aot_s17 = zr_aot_s13 >> zr_aot_s1;`, and `zr_aot_s16 = ~zr_aot_s1;`. `TO_FLOAT` conversion can compute from `zr_aot_sSource` or `zr_aot_uSource`, and `TO_INT_FLOAT` can compute from `zr_aot_fSource`, when the source slot is a declared scalar local.

This is still an incremental M2 scalar backend step. Signed `i64` binary arithmetic, signed `i64` comparisons, unsigned `u64` binary arithmetic, `f64` binary arithmetic, and signed `i64` binary bitwise currently use declared `sN` / `bN` / `uN` / `fN` locals for primary expressions; signed `i64` shifts and bit-not can now also write reused temporary `sN` destinations when SemIR provides `i64` destination evidence. Fused signed branches and the first numeric conversion source paths use declared `sN/uN/fN` operands when available. The generated code still mirrors through existing frame slot storage. Focused typed scalar local copies and focused typed branch helpers no longer use the old stack/value fallback paths, and scalar C local declarations now exist, but conversion destination-local coverage, broader branch variants, broader C-local mirroring, GC root registration, non-numeric/generic conversions, and typed/dynamic bridge/deopt execution remain separate slices.

Scalar stack-copy elision is additionally constrained by downstream use. A
destination may remain local-only only when a later proven scalar consumer can
read the matching local kind; otherwise the generated C materializes the frame
slot so a generic value consumer cannot observe stale or null storage. The
value-kind proof is overwrite-aware within a block: once a slot is reset or
written with a different scalar kind, it cannot fall back to a historical
block-entry kind. This is required for reused temporary slots, including a u64
call result that occupies a slot previously known as bool.

Ownership operations and call results are hard provenance barriers before a
stack-copy specialization is selected. If either most recently wrote the source
slot, the C backend emits the ordinary ownership-preserving runtime stack copy;
it does not read or synchronize a historical scalar local for that slot.

## Dynamic Arithmetic Deopt Boundary

Generic dynamic arithmetic bytecode now has an explicit SemIR boundary instead of disappearing from the typed model. The compiler maps generic `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `LOGICAL_EQUAL`, and `LOGICAL_NOT_EQUAL` to `DYN_ARITHMETIC` rows marked as `DYNAMIC_RUNTIME` effects. Each row records the original destination and operand slots and receives a `deoptId` entry that points back to the exec instruction index.

This keeps invariant A visible in metadata: operations that are not proven to have a single static C type stay dynamic and are represented as deopt-capable runtime points. It is not the full conflict analysis yet. Broader type-flow conflicts and mixed typed/dynamic block splitting remain part of the remaining instruction-set refactor work.

## Static Type Conflict Deopt Boundary

Typed scalar SemIR now has a conservative conflict guard before emitting pure typed arithmetic/comparison rows. If an instruction's destination or operand slot has multiple typed-local bindings whose annotated static C type differs, the mapper emits `DYN_ARITHMETIC` instead of the typed scalar opcode. The row is marked `DYNAMIC_RUNTIME`, receives a deopt entry, and preserves the original destination and operands.

This is the first concrete 03-S2 conflict trigger. It prevents known contradictory slot metadata from entering the typed path, but it is still narrower than full type-flow analysis: def/use joins, block splitting, and deopt execution remain later work.

## Dynamic Member Deopt Boundary

Generic member-access bytecode now has explicit SemIR runtime boundaries. The compiler maps generic `GET_MEMBER` and `SET_MEMBER` to `META_GET` and `META_SET` rows marked as `DYNAMIC_RUNTIME` effects. Each row receives a `deoptId` entry and preserves the destination/value, receiver, and member-entry operands from the exec instruction.

This separates dynamic object/member dispatch from typed inline struct access. Proven inline struct field operations continue to use typed value-place SemIR such as `FIELD_ADDR`, `LOAD_VALUE`, and `STORE_VALUE`, while generic member dispatch stays visible as a deopt-capable runtime point until later lowering can bridge or reject it explicitly.

## Dynamic Call Deopt Boundary

Generic call bytecode that is not proven to be a typed value call now remains visible in SemIR. After the value-type `CALL_TYPED` lowering pass declines a generic `FUNCTION_CALL`, the fallback mapper records it as `DYN_CALL`; `FUNCTION_TAIL_CALL` records as `DYN_TAIL_CALL`. Both rows are `DYNAMIC_RUNTIME` effects, receive deopt entries, and preserve the result slot, callee slot, and argument count.

This keeps call lowering in the same two-path shape used elsewhere in the AOT plan: typed calls use `CALL_TYPED`, while unproven dynamic calls stay explicit runtime/deopt boundaries. Direct C call ABI lowering and typed/dynamic bridge execution remain later work.

Call-site quickening must preserve the same slot-shape contract. A `DYN_CALL` whose result slot is lower than its callee slot stays on the generic instruction instead of being rewritten to cached or no-argument superinstructions, because those fast paths assume the return write cannot clobber the staged callable/receiver window.

The VM no-argument dynamic superinstruction uses the same frame-layout-aware
pre-call helper as ordinary dynamic calls. It allocates the physical call window
after the complete byte-frame storage and stages the logical callable there;
using `BASE(functionSlot) + 1` would overlap layout-backed payload bytes even when
the logical slot numbers appear disjoint. The pool/FFI full-GC stress exercises
this path through a native zero-argument probe while a Span value remains live.

## Dynamic Iterator Deopt Boundary

Generic iterator bytecode now has explicit SemIR runtime boundaries when it is not lowered to a typed loop. The fallback mapper records `ITER_INIT` as `DYN_ITER_INIT` and `ITER_MOVE_NEXT` as `DYN_ITER_MOVE_NEXT`. Both rows are `DYNAMIC_RUNTIME` effects, receive deopt entries, and preserve the result plus iterator/source operands.

This records the current dynamic iterator contract without pretending it is pure C. Typed iterator lowering to indexed `for` loops, array-specific fast paths, and branch-shaped iterator control flow remain later decomposition work.

## Dynamic Index Deopt Boundary

Generic index bytecode now has explicit SemIR runtime boundaries when it is not lowered to typed array element access. The fallback mapper records `GET_BY_INDEX` as `DYN_INDEX_GET` and `SET_BY_INDEX` as `DYN_INDEX_SET`. Both rows are `DYNAMIC_RUNTIME` effects, receive deopt entries, and preserve the destination/value slot, receiver slot, and index slot from the exec instruction.

This keeps array and object indexing in the same two-path model as member access and calls: proven typed array work must later lower into explicit bounds checks plus address/value operations, while unproven indexing remains a dynamic runtime/deopt boundary. The current slice records the boundary only; pure C array element lowering and bounds-check SemIR are still later work.

## Runtime Shutdown And GC Root Marking

`ZrCore_GlobalState_Free` releases the garbage collector before releasing the string table. Shutdown GC can still need to mark string-table major roots, so freeing the string table first leaves shutdown collection with dangling root metadata. The string table is therefore kept alive until after `ZrCore_GarbageCollector_Free` returns.

`ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_RELEASED` is a lifecycle state,
not proof that the object's allocation has already been freed. It records that
the finalizer path has completed and prevents the object from being remarked.
When sweep or shutdown later unlinks that object, it must still release region
accounting, ownership/registry state, type-specific metadata, and the raw object
allocation. The free path skips only a repeated `scanMarkGcFunction` callback
for an already-released object. The released embedded-child regression and the
full 67-case GC suite pass under Clang ASan with leak detection enabled.

AOT root frames support two root-location encodings. `ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET` keeps the existing
`SZrTypeValue` stack-slot path and validates the computed address against the VM stack bounds before mark/rewrite.
`ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS` treats `frameBase + frameByteOffset` as a registered `SZrRawObject **`
local-address root slot, marks the pointed raw object during root marking, and rewrites the slot through forwarding
addresses during minor GC. This completes the runtime side of the optional local-address root ABI; generated-C
root-map emission for that location kind remains a later AOT backend slice.

In Debug builds, the short-string major-root traversal uses the string hash-set capacity as the cycle guard instead of the current element count. The linked short-string root list can contain entries whose traversal bound is not safely represented by `elementCount` during shutdown and full-collection edge cases; capacity remains the conservative bounded walk limit for detecting accidental cycles without tripping on valid retained roots.

## Function Frame Layout Metadata

`SZrFunction` now carries a sidecar byte-frame description in addition to the existing fixed-slot execution ABI:

- `frameByteSize` and `frameByteAlign` describe the contiguous byte region needed for the function frame.
- `frameSlotLayoutLength` matches the current `stackSize` for compiled functions.
- `SZrFunctionFrameSlotLayout` maps each logical stack slot to `stackSlot`, `byteOffset`, `byteSize`, `byteAlign`, `slotKind`, `isParameter`, and `typeLayoutId`.

These fields are kept as an append-only sidecar at the end of `SZrFunction`. That preserves the offsets of existing runtime fields such as constant pools, call-site caches, and child-function graphs while the VM still has native fixtures and copied function graphs that observe the public function ABI.

The layout builder lives in `compiler_typed_metadata.c`. It walks stack slots in order, aligns a byte cursor for each slot, and emits deterministic byte offsets. Ordinary slots keep `sizeof(SZrTypeValue)` storage. Typed slots whose local binding resolves to a local struct prototype use that prototype's `layoutByteSize` and `layoutByteAlign`, are marked `ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT`, and carry the prototype index as the first layout id used by this migration.

Frame byte offsets intentionally start after the legacy fixed-slot mirror, `stackSize * sizeof(SZrTypeValueOnStack)`, so byte-backed inline storage does not alias the dense logical slots that older instructions still read. Compiler frame-layout tests assert this base offset after binary roundtrip. The typed metadata classifier also treats conditional branch operands as relative jump metadata, not value slots, unless an actual extra operand names a slot. That keeps inline receiver temporaries typed when branches and inline member reads share a lowering window.

Function declarations, lambdas, class members, meta functions, tests, and final assembled entry functions all call the builder after `stackSize` and typed local metadata are available. `ZrCore_Function_FindFrameSlotLayout` is the public lookup helper for callers that need a typed place without scanning the array themselves.

Compiled frame layouts are dense and emitted in logical stack-slot order. The lookup helper first probes `frameSlotLayouts[stackSlot]` and accepts that O(1) path only when the entry's recorded `stackSlot` matches the request. Hand-built, sparse, reordered, and out-of-range layouts retain the original linear scan. This removes repeated metadata walks from interpreter operand access without changing the public function layout, `.zro` serialization, stack relocation, or inline-storage address validation.

Validated complete VALUE slots now carry the append-only
`ZR_FUNCTION_FRAME_SLOT_FLAG_DIRECT_VALUE` bit in `reserved0`. Before function
metadata is published, the compiler and binary loader finalize dense layouts
and mark non-alias slots whose byte range, size, and alignment are valid for a
direct address calculation. The derived bit is masked out of `.zro` output and
recomputed after loader validation, so serialized input cannot assert trust.
`execution_inline_frame_get_value_slot` probes the canonical indexed layout
before invoking the generic lookup, then passes that record to an inline
fail-closed helper which adds the current frame base to the cached byte offset
without re-running stack bounds checks. A miss alone enters
`ZrCore_Function_FindFrameSlotLayout` and the checked place path.
Alias, inline-struct, malformed, externally constructed, and otherwise
unvalidated layouts continue through `ZrCore_Function_MakeFrameSlotPlace`.
The helper never caches a raw address, so a stack relocation only requires the
caller to provide its refreshed frame base. Runtime frame initialization only
reads the finalized metadata, avoiding a shared-function mutation between
mutators. Append-only profile helpers `frame_value_slot_direct` and
`frame_value_slot_checked` expose the boundary; the hot getter reads the active
profile runtime from the current state instead of performing a TLS lookup per
operand. The focused frame-slot test covers the direct offset contract,
relocation, finalization, unsafe-layout rejection, initialization, stable helper
names, and direct/checked counts.

The interpreter dispatch loop has a narrower direct-slot helper for repeated
operands. It receives the loop-cached profile runtime and helper-recording flag,
checks the same canonical index, recorded slot number, and `DIRECT_VALUE` bit,
then computes the address from the current frame base. Any missing or malformed
metadata calls the public getter, preserving the complete checked fallback.
This boundary avoids repeated `state->global->profileRuntime` discovery without
duplicating the generic layout/place logic or storing relocation-sensitive raw
addresses.

Cached member get/set/initialize entry points use the same finalized bit as a
negative inline-storage proof. When the receiver slot is a canonical direct
VALUE, they return before resolving a member name or probing frame-place
metadata because only inline struct/union slots can satisfy that path. Missing,
untrusted, aliased, sparse, reordered, or inline layouts keep the original
lookup and checked fallback.

The same proof now covers non-dispatch frame consumers. The public
`ZrCore_Function_MakeFrameSlotPlace` constructs a bounded place directly for a
canonical `DIRECT_VALUE` record before using generic stack place resolution.
Frame initialization, value-overlap checks, reverse frame-pointer mapping, and
inline-frame drop similarly use the direct record when possible. Reverse mapping
first converts a canonical logical stack pointer to its slot index in O(1), then
falls back to the complete layout walk for inline, alias, sparse, reordered, or
interior byte pointers. None of these paths stores a raw address across stack
relocation.

Each VM precall also records its allocated frame-storage slot count on the
resulting `SZrCallInfo` as a three-byte `frameStorageSlotCountPlusOne` value in
the legacy padding after `hasReturnDestination`. This preserves the existing
structure size and all following field offsets on supported targets. The
plus-one encoding reserves zero for native, legacy, externally initialized, or
over-capacity call infos, which continue to resolve the metadata function and
calculate the storage boundary.
`ZrCore_Function_GetCallInfoFrameStorageTop` can therefore recover the common VM
boundary without rescanning generated instruction temporaries. The cached value
belongs to the call instance rather than shared `SZrFunction` metadata, so later
metadata changes and concurrent callers cannot make the allocation boundary
stale. Tail-call frame reuse updates the same field after calculating the new
callee's storage count, preventing the reused call info from retaining the prior
callee's boundary.

The retained ext4 GCC 11.4 Release Callgrind run records `722,029,136 Ir`, down
`12.04%` from the pre-change `820,818,823 Ir`. `ZrCore_Stack_MakeFramePlace`
accounts for `14,352 Ir` (`0.00199%` exclusive) and the generic layout lookup for
`5,424 Ir` (`0.00075%`), so the deterministic `<5%` frame-place gate is met.
The paired process wall-time run is deliberately not accepted: all C, ZR interp,
and ZR binary samples remained unstable (`15.27%` to `22.48%` CV), leaving the
numeric `+10%` wall-time gate open.

On `mixed_service_loop`, the direct frame consumers and per-call storage
boundary first reduce the retained GCC 11.4 Release Callgrind total from
`868,860,510 Ir` to `409,692,473 Ir`. The later VALUE-parameter summary slice
uses an exact paired baseline in which both binaries include the member-cache
null guard: `409,431,558 Ir` before and `396,430,578 Ir` after (`-3.18%`). The
next exact pair adds the direct VALUE-only frame-drop summary and records
`396,142,221 Ir` before and `378,649,763 Ir` after (`-4.42%`). Reusing that
strict summary in frame initialization then records `378,637,009 Ir` before
and `365,295,917 Ir` after (`-3.52%`). Later dispatch-getter, copy-probe,
generated-slot-count, frame-drop preflight/no-owner, in-place direct VALUE
  parameter-copy, dispatch copy-probe bypass, and packed direct-frame boundary
  slices plus packed owner batching and prepared-precall fusion retain
  `236,125,782 Ir`. The current cumulative reduction from the original baseline
  is `72.824%`, at
the unchanged checksum `408940136`.
The deterministic instruction-count result is accepted; the wall-time gate
remains open until the calibrated harness produces an eligible paired series.

On `object_field_hot`, the dispatch helper first reduces the same-build scale-1
Callgrind total from `205,647,828 Ir` to `159,970,049 Ir`; the direct VALUE
member-probe guard then reduces it to `126,716,379 Ir` (`-38.38%` cumulative).
Checksum `623146080` and direct/checked counts `1,686,066/1` are unchanged. The
paired ZR interpreter timing remains ineligible because its final CV is
`16.52%`; this deterministic result does not substitute for the plan's stable
wall-time gate.

The `.zro` binary function format now writes the frame byte header and slot layout array immediately after `stackSize`. `ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT` gates reading so older binaries still load with empty frame layout metadata. Runtime IO copies the metadata into loaded `SZrFunction` instances, including child functions, so tooling and the next VM-stack migration layer can inspect the same byte-frame shape after a binary roundtrip.

`ZrCore_Function_GetFrameStorageSlotCount` computes the number of legacy `SZrTypeValueOnStack` allocation units needed to hold the byte frame. It returns the greater of the old logical `stackSize` and the rounded-up `frameByteSize`. VM precall now uses that storage count for `functionTop` and stack growth, while still using `stackSize` for logical slot semantics. Padding storage slots beyond `stackSize` are reset to null during call setup so existing stack scanning and frame teardown do not observe stale `SZrTypeValue` contents.

VM resolved/prepared pre-call now takes the first real payload-movement step for inline parameters. When a callee has inline struct parameter layouts, exact-args fast-path probes fall back to the generic pre-call path so the layout hook can run. After the callee frame storage is initialized, the runtime derives the caller argument start from the previous VM call-info frame base and calls `ZrCore_Function_CopyInlineFrameParameters` with the prototype resolver. This copies caller inline frame payload bytes into the callee inline parameter span only when both caller and callee layout metadata can prove the operation. Missing caller metadata, missing frame layout, or resolver failure leaves the existing boxed call behavior in place.

Byte-backed VALUE parameters use the same frame-layout boundary. `ZrCore_Function_CopyValueFrameParametersFromFrame` resolves the source and destination VALUE slots through `SZrFunctionFrameSlotLayout`; if the source byte-backed slot is still null but the dense caller slot is materialized, the dense slot is used as the source. After copying into the callee byte span, the helper also mirrors the value into the callee dense slot. Generated AOT C still has mixed consumers during the byte-frame migration: field stores can read byte-backed VALUE storage, while typed arithmetic and branch lowering can read dense slots. Keeping both views synchronized at the call boundary prevents a staged direct call from entering a callee with only one half of the VALUE parameter initialized.

Tail-call frame reuse also keeps byte-backed VALUE parameters synchronized. When a VM frame is reused for a callee without inline-struct parameter layouts, the runtime copies VALUE parameters from the caller's logical frame value slots into the reused byte-backed parameter spans and mirrors them to dense slots before reinitializing frame storage. Destination VALUE parameter slots release existing owned values before being overwritten, so owner payloads left by a previous frame do not leak or double-release.

Finalized functions now also carry an immutable VALUE-parameter layout summary.
`directValueParameterCountPlusOne` records the number of canonical direct VALUE
parameters while reserving zero for checked fallback, and
`directValueParameterScanLength` stops the copy walk after the last parameter
layout instead of scanning trailing locals. The summary is produced only after
every frame layout is canonical and every parameter has the validated
`DIRECT_VALUE` bit. It is cleared before every finalization attempt, initialized
and reset with the function lifecycle, and never serialized. The loader rejects
an input `DIRECT_VALUE` flag, copies validated layout fields, and then rebuilds
both the bit and summary locally.

`ZrCore_Function_CopyValueFrameParameters` and its frame-source variant use the
summary only while its bounds and last direct parameter still validate. A stale
or hand-built summary therefore falls back to the original full scan and checked
place construction. Zero arguments return before any layout visit. Direct
destinations and sources still release overwritten ownership where required,
copy the byte-backed value, and synchronize the dense mirror. Append-only helper
counts distinguish direct, checked, empty, and layout-visit paths. The final
scale-1 `mixed_service_loop` profile records `61,449` direct copies, `61,449`
layout visits, two empty copies, and zero checked copies. In the exact paired
Callgrind result, `ZrCore_Function_CopyValueFrameParameters` falls from
`20,470,486` to `12,636,314` exclusive Ir (`-38.27%`) and from `34,929,366` to
`21,688,474` inclusive Ir (`-37.91%`). Scale-1 `numeric_loops` changes by
`-0.012%` and `object_field_hot` by `+0.005%`, both within the representative-set
`1%` regression gate.

The ordinary call-window path now specializes this same strict summary. It
preflights the complete frame span once and then uses the finalized parameter
offsets directly. When `argumentBase` is the callee frame base, the source is
already the dense destination: the runtime copies it into the byte-backed
mirror and leaves the unchanged dense source in place. Separate argument
windows still copy both mirrors. `ZrCore_Value_Copy` remains responsible for
normalizing stale control pointers and releasing overwritten owners; the
frame-source and checked paths are unchanged. This changes the exact
mixed-service pair from `296,648,172` to `282,552,302 Ir` (`-4.752%`) and the
parameter-copy call edge from `21,688,474` to `7,372,972 Ir` inclusive. Numeric
and object representatives change by only `+0.0012%` and `+0.0001%`.

Finalized functions also carry a strict direct VALUE-only frame-drop summary.
`directValueFrameSlotCountPlusOne` reserves zero for the original checked path
and is published only when every frame layout is its canonical direct VALUE
entry and `DIRECT_VALUE` is its only derived flag. Like the parameter summary,
it is cleared before finalization, initialized and tombstoned with the function,
never serialized, and rebuilt locally after loader validation. Mixed inline,
alias, sparse, reordered, malformed, extra-flag, unfinalized, and hand-built
layouts therefore retain registry lookup, complete inline-layout preflight, and
checked VALUE place construction.

The direct drop loop does not weaken address or lifecycle checks. It still uses
the bounded direct-place helper for every slot, releases byte-backed and dense
VALUE owners independently, and preserves the overlap guard. Strict publication
also excludes constructor bitmap and inline receiver flags, so ordinary exit
and unwind can share the loop while all inline lifecycle cases remain checked.
The final scale-1 profile records `20,485` direct drops and one checked drop.
`function_drop_inline_frame_values` falls from `37,282,153` to `19,658,273`
exclusive Ir (`-47.27%`) and from `49,861,149` to `32,114,373` inclusive Ir
(`-35.59%`). Exact-pair representatives change by `-0.021%` for
`numeric_loops` and `-0.0009%` for `object_field_hot`.

Frame initialization reuses the same strict summary; it does not publish a
second trust bit. Proven direct-only frames walk their canonical layouts once,
track parameter order linearly, preserve the first `preservedArgumentCount`
parameters, and initialize every remaining slot through the bounded direct
VALUE helper. Frames without the summary keep the original alias checks,
parameter-index lookup, generic place construction, and inline-storage path.
Append-only helper counts expose `frame_value_initialization_direct` and
`frame_value_initialization_checked`; the final scale-1 profile records
`20,486` direct initializations and one checked initialization.
`ZrCore_Function_InitializeFrameLayoutStorage` falls from `21,691,743` to
`8,061,782` exclusive Ir (`-62.83%`) and from `21,694,073` to `8,350,930`
inclusive Ir (`-61.51%`). Exact-pair representatives improve by `-0.055%` for
`numeric_loops` and `-0.005%` for `object_field_hot`.

The interpreter dispatch loop also keeps the proven direct VALUE probe inside
its hot translation unit. `FRAME_VALUE_SLOT` first calls the small inline probe
with the loop-cached profile runtime and current frame base. A canonical indexed
layout returns `frameBase + byteOffset`; every guard miss returns null and then
calls the unchanged generic getter, which records the checked helper and retains
layout lookup, place construction, bounds, and inline-layout behavior. The probe
never caches a derived address, so stack relocation continues to supply the new
frame base on the next operand access.

The outer wrapper deliberately uses ordinary `inline`, not forced inline. GCC
may outline cold/fallback call sites while keeping the dominant direct probes in
`ZrCore_Execute`. In the retained GCC 11.4 Release artifact the direct/checked
profile remains `1,890,775 / 30,725`; the wrapper itself accounts for only
`430,710 Ir` on 20,510 outlined calls. Exact paired Callgrind results improve
`mixed_service_loop` by `4.42%`, `numeric_loops` by `17.02%`, and
`object_field_hot` by `12.21%`, with unchanged checksums. The tradeoff is a
`90,112` byte (`3.51%`) increase in `libzr_vm_core.so` versus the preceding
frame-initialization binary; this size and the measured large-dispatch compile
cost are part of the acceptance record rather than hidden by the runtime gain.

Direct VALUE-to-VALUE stack copies also skip the speculative inline-copy probe.
After the null and return-slot guards, the probe now returns false immediately
when both layouts carry the canonical `DIRECT_VALUE` proof. Such a pair cannot
be an inline payload or alias, so the caller performs its existing ordinary
`SZrTypeValue` copy without two layout lookups and two generic frame getters.
If either side lacks the proof, the complete inline/union/constructor-carrier
path remains unchanged. The small predicate lives in
`execution_inline_frame_copy_fast.h`, separate from the dispatch helper, so an
inline-frame-only edit does not invalidate the large dispatch translation unit.
The exact mixed-service pair is `349,179,948 -> 325,175,994 Ir` (`-6.87%`),
with checksum `408940136`; direct/checked getter counts become
`1,582,901 / 30,725`. Numeric changes by `+0.037%` and object by `-0.002%`, both
inside the `1%` representative regression gate. The core shared library and
dispatch object sizes are unchanged from the accepted dispatch-getter binary.

Dispatch now bypasses that remaining helper call entirely when the immutable
strict direct VALUE-only frame summary proves both bounded slots belong to the
canonical direct frame. If the summary is absent, the wrapper applies the same
per-slot direct predicate before deciding whether to call the helper. The
helper retains its own predicate and every inline struct, union,
constructor-carrier, unfinalized, malformed, and out-of-range fallback. The
appended `frame_value_copy_probe` profile helper records real fallback entries;
the production callback is compile-time-known while the focused test injects a
stub to verify the boundary. The first per-slot-only candidate was rejected at
`-2.961%`. The strict-summary result is `282,552,302 -> 273,765,184 Ir`
(`-3.110%`) for `mixed_service_loop`, with numeric/object changes of
`-0.0060%/-0.0084%`; the final mixed profile contains no copy-probe helper
entry. Full evidence is recorded in
`tests/acceptance/2026-08-31-dispatch-direct-value-copy-probe-bypass.md`.

The strict frame summary now proves a stronger packed shape before it is
published. The layout table must contain exactly one canonical direct VALUE
entry per logical stack slot, byte mirrors must begin immediately after the
dense frame and advance at `SZrTypeValueOnStack` stride, every mirror must be
exactly `sizeof(SZrTypeValue)`, and `isParameter` must describe the exact
parameter prefix. Canonical direct slots in non-packed frames keep their
per-slot `DIRECT_VALUE` proof, but the frame summary remains zero and they use
the existing checked/per-layout paths.

That proof removes several repeated layout operations without weakening frame
storage or GC boundaries. Precall can prove that a packed frame has no inline
parameters without scanning its layout table. Parameter copy derives the byte
mirror base once and copies the parameter prefix, frame initialization starts
after the preserved parameter prefix, and direct drop walks byte and dense
mirrors by index. All three preserve the existing span preflight, value-copy,
ownership release, dense synchronization, and checked fallback semantics.
Postcall inline return, receiver-copyback, and constructor-copyback probes also
decline immediately for a strict direct-only callee; caller inline object
destinations still use their separate fallback.

The prepared-precall fast guard remains unchanged. The target frame has byte
storage beyond its logical stack and requires GC-safe clearing of the complete
logical frame, so it must not use the existing fast path that requires
`frameStorageSlotCount == stackSize` and exact argument-only entry clearing.
The precall scan alone improved only `0.656%`, and packed loops before the
return guards improved only `2.839%`; neither was independently accepted. The
combined exact pair is `273,765,184 -> 255,021,394 Ir` (`-6.846667%`) at the
same checksum. Numeric improves `0.012560%` and object regresses `0.010445%`,
both inside the `1%` representative gate. Full evidence is recorded in
`tests/acceptance/2026-08-31-packed-direct-value-frame-summary.md`.

Packed direct teardown also avoids treating an internal dense-mirror read as a
profiled logical stack access. The loop uses `ZrCore_Stack_GetValueNoProfile`
and tests four slots at a time by combining the ownership kinds from four byte
mirrors and four dense mirrors. An all-`NONE` group cannot release anything and
is skipped with one branch. If any lane may own storage, each pair still runs
the original releasable-ownership checks and releases both mirrors; the tail
uses the same pair helper. The packed summary proves the two fixed-stride
regions are disjoint, while every frame without that proof retains the old
checked overlap and layout behavior.

The no-profile access alone improved the target by `2.517827%` and was not
independently accepted. With four-slot owner batching, the exact pair is
`255,021,394 -> 245,339,382 Ir` (`-3.796549%`) at unchanged checksum.
`function_drop_inline_frame_values` falls from `8,578,193` to `3,324,901 Ir`
exclusive and `__tls_get_addr` from `5,985,228` to `2,175,828 Ir`. Numeric
regresses `0.020328%` and object improves `0.015146%`, both inside the `1%`
representative gate. Full evidence is recorded in
`tests/acceptance/2026-08-31-packed-direct-value-drop-owner-batching.md`.

Prepared VM calls now use a narrower steady-state path for that same strict
packed frame. It requires inactive debug hooks, exact arguments, an exact call
window, sufficient existing stack capacity, a bounded entry-clear range, byte
storage beyond the logical stack, and an already reusable call-info. Any miss
returns to the original exact-args probe and generic precall, so stack growth,
first-call allocation, debug, mixed layouts, and inline/alias frames retain
their complete behavior.

The specialized path still clears the logical entry frame and every padding
storage slot. Because the packed summary proves that byte mirrors occupy that
entire fixed-stride padding region, this clear also initializes all byte VALUE
slots and their stack metadata; a second layout initialization walk would only
repeat the same reset. Exact parameters are copied from their unchanged dense
sources to the byte mirrors with `ZrCore_Value_Copy`, preserving ownership and
the existing direct/layout profile counts. The generic-copy plus layout-init
candidate improved only `1.611%`, and padding/init fusion reached only `2.609%`.
The retained exact-copy/private-helper result is
`245,339,382 -> 236,125,782 Ir` (`-3.755451%`), with numeric/object changes of
`-0.003641%/+0.013457%`. Full evidence is recorded in
`tests/acceptance/2026-08-31-packed-direct-prepared-precall-fusion.md`.

Quickened signed scalar handlers now reuse that strict packed proof at a
narrower boundary. Dispatch derives the fixed-stride byte-mirror base once per
resolved frame and uses it for signed arithmetic, comparison, conversion,
fused load, branch operands, and complete-value destinations. The cached base
is valid only while the call-info function base still matches the identity
used to resolve `currentFunction`; a call switch or relocation clears it until
the outer loop resolves the frame again. Missing proof, an out-of-range slot,
or a cleared cache returns to the existing direct/checked getter. Values remain
full `SZrTypeValue` objects, so this is an address-side precursor rather than
the final unboxed typed-lane representation.

Rechecking the summary per access regressed numeric by `7.076%` and was
rejected. The once-per-frame cache changes `numeric_loops` from `111,746,343`
to `94,984,122 Ir` (`-15.000241%`); mixed changes `+0.437322%` and object
improves `6.308004%`, both inside the representative gate. The helper profile
remains direct/checked `2,502,333 / 1`. Full evidence is recorded in
`tests/acceptance/2026-09-01-packed-signed-scalar-frame-base.md`.

Finalized functions also publish the generated frame-slot count as immutable
derived metadata. `generatedFrameSlotCountPlusOne` reserves zero for the
original instruction-stream scan, so unfinalized and hand-built functions keep
dynamic behavior. Finalization clears the field, scans the current stream, and
publishes `count + 1`; the loader does this only after copying instructions, and
quickening repeats it after all rewrites. The field is initialized and
tombstoned with the function, never serialized, and appended at the public
structure tail. In the exact scale-1 `mixed_service_loop` pair, removing the
per-precall scan changes `325,175,994 -> 314,490,481 Ir` (`-3.286%`) with the
same checksum. Numeric and object representatives improve by `0.023%` and
`0.012%`. The once-per-function scan costs `17,347 Ir`, while the hot public
getter falls from `10,624,761` to `163,904 Ir`.

Single-result VM post-call now has the matching limited return hook for payloads that are already inline on both sides. `ZrCore_Function_TryCopyInlineFrameReturnValue` derives the callee result slot and caller return destination slot from their call-info frame bases, resolves both prototype layouts, checks that the two layouts are byte/lifecycle compatible, then copies the inline span before the callee frame is dropped. If either side is missing frame metadata, the destination is not a caller inline slot, or the two layouts cannot both be resolved, the helper declines and the old `SZrTypeValue` return path remains in force.

Tail-call frame reuse stays deliberately conservative for inline parameters. Until the runtime has a real in-place move operation for overlapping inline payloads with ownership fields, `ZrCore_Function_TryReuseTailVmCall` refuses callees with inline struct parameter layouts. The interpreter then falls back to the non-reuse call path, where the already-inline pre-call copy hook can run without treating raw struct bytes as ordinary stack slots.

## Value-Type Execution Shape

The 2026-06-04 value-type runtime slice follows the same architectural split as the HybridCLR/IL2CPP implementations under `lua/`: inline frame bytes are the canonical value storage, and object/boxed values are materialization bridges. A source `$Struct(...)` constructor can still seed an object-shaped receiver for existing member/constructor dispatch, but the compiler records a separate inline result slot for struct values. Constructor receiver copyback then copies the callee `this` inline payload back into the caller inline result span before the callee frame is dropped. The 2026-06-17 union slice reuses the same idea for typed union locals: a constructor carrier object can be materialized into the target inline frame span as `[tag][payload]` bytes when the destination slot has union inline layout metadata. Union local-to-local declaration initialization and simple assignment are now kept on source inline local slots by the compiler, so `SET_STACK` can copy the existing tag/payload bytes without routing through an object-shaped temporary; this path is scoped to union layouts and leaves the existing inline struct argument/copy lowering unchanged. Constructor assignment into an existing typed union local uses the same `SET_STACK` interception: the inline-frame copy hook checks both the physical source slot and the logical frame value slot for a constructor carrier, drops the destination's old active union payload through the resolved union layout, and then writes the new tag/payload bytes. When the active variant contains embedded value or owner payload fields, the union type layout uses the stored tag to copy/drop/visit only that variant's fields.

Typed union inline slots also participate in the interpreter member-read path for pattern matching. When `GET_MEMBER` targets an inline union slot, `execution_inline_frame_try_get_member_by_name_to_slot` recognizes `__zr_unionVariant` and `__zr_unionPayloadN` as pseudo-members. The tag bytes are read from the inline span and matched against serialized variant metadata to produce the variant name; payload reads use the active variant's payload field byte offset/size/align metadata to load POD fields into a normal result slot. The same pseudo-member path now supports targeted writes used by explicit owner `move` cleanup, and struct-field inline union values can copy owner payloads both from typed locals into fields and from fields back into typed locals. Broader expression/member mutation matrices remain staged, but the current copy/drop path is owner-aware for these typed-local and struct-field cases.

Inline frame initialization also handles managed fields. `ZrCore_Function_InitInlineStorage` zeroes the inline byte span, resolves the prototype layout, recursively initializes nested inline structs, and resets embedded `SZrTypeValue` field slots to null. That keeps string/object fields in value types visible to later GC visitors without treating uninitialized struct bytes as arbitrary stack values.

Return movement now distinguishes physical stack slots from logical frame value slots. `function_move_returns` and the single-result post-call fast path resolve the callee return source through the callee frame layout before doing the ordinary `SZrTypeValue` copy. Inline struct returns still go through `ZrCore_Function_TryCopyInlineFrameReturnValue`; scalar/object returns whose source lives in frame bytes now read from the logical `FRAME_VALUE_SLOT` rather than from a stale physical stack position.

Generic `ZrCore_Function_PostCall` keeps the historical stack-top contract: after return movement, `state->stackTop` follows the destination/result count, and inline frame payload cleanup must not raise it to the caller frame storage top. The hot single-result/frame-layout return helpers are the places that preserve the previous frame storage top when the interpreter is still executing inside a caller frame whose inline byte storage remains live.

Plain scalar result writes normalize value metadata only when the destination is a complete `SZrTypeValue` frame slot. Destinations that resolve to inline struct storage keep their layout-owned bytes as the canonical representation; call and index results targeting those slots materialize through the inline storage helpers and only mirror a safe value view when the runtime can prove the destination is not a partial inline payload.

Interpreter native and generic call paths stage call windows outside the caller frame storage when frame layout metadata is active. `execution_prepare_frame_layout_call_window` snapshots the logical callable and arguments, materializes inline struct arguments where needed, reserves scratch storage at or beyond `ZrCore_Function_GetCallInfoFrameStorageTop`, and copies the snapshot there. Known native calls, known native member calls, generic calls, meta calls, dynamic calls, and the `SUPER_DYN_TAIL_CALL_NO_ARGS` native fallback use staged windows so temporary frames do not overlap inline local payload bytes. Native and meta-call results are written back through the logical return destination, preserving the caller frame layout.

Frame-layout generic calls compute their effective return destination only after `execution_prepare_frame_layout_call_window` has copied or staged the call window and restored the possibly relocated caller frame base. The slow fetch path also refreshes the cached interpreter `base` after any debug hook or stack relocation, because traceback/debug hooks can reserve stack space and move the underlying stack allocation before control returns to dispatch.

Ownership and typed branch/equality opcodes are part of the same boundary. When frame layout metadata is active, ownership casts/releases, object conversion, typed equality, typed comparisons, and fused signed branch tests read the logical `FRAME_VALUE_SLOT` rather than assuming the physical `BASE(slot)` storage unit is the canonical value. Weak reference expiry additionally verifies that a candidate slot is still weak and still points to the expiring weak ref before clearing it, which prevents a release path from nulling unrelated frame-layout values that happen to share an old weak-reference side table entry.

## GC, Drop, And Native Entries

Inline arrays use the same registry contract instead of storing boxed struct elements. `ZrCore_Object_NewInlineArray` allocates an aligned object tail, records the element layout id/hash/size, and initializes every element through the registry. `CREATE_INLINE_ARRAY` creates that storage and `BIND_INLINE_ARRAY_ELEMENT_PLACE` resolves a checked element offset for destination-first construction. GC mark/rewrite and object teardown visit or drop each initialized element with the recorded layout, rejecting registry/hash drift rather than interpreting the tail as ordinary object values.

The frame byte layout is now used by real runtime entries when metadata is present and the resolver proves the inline layout.

GC stack scanning keeps the callable slot on the legacy path, then treats `callInfo->functionBase.valuePointer + 1` as the frame byte base. Ordinary stack slots that intersect an inline struct span are skipped by raw slot scanning. `ZrCore_Function_VisitInlineFrameGcValues` then visits only the embedded `SZrTypeValue` fields declared by the resolved layout. Minor collection rewrite uses the same layout visitor, so forwarded embedded values are rewritten in place instead of leaving stale object pointers inside raw struct bytes.

Frame teardown and tail-call reuse call `ZrCore_Function_DropInlineFrameValues` before old frame storage is overwritten or reused. For inline single-result returns, post-call captures the callee metadata first, copies the inline return payload to the caller destination, and then drops the captured callee frame layout so ordinary return movement cannot erase the metadata needed for cleanup. Tail reuse also checks `ZrCore_Function_FrameStackSlotIntersectsInlineStruct` while releasing old storage slots, so raw inline bytes are cleared as storage units rather than being interpreted as ordinary `SZrTypeValue` slots. The drop helper preflights all inline layouts before performing any field drop; if any layout cannot be resolved, it fails without partially dropping the frame.

For a finalized frame proven to contain only canonical direct VALUE layouts,
the immutable drop summary bypasses the irrelevant inline-layout preflight. All
other frames keep the failure-atomic behavior above. The direct path validates
the complete `frameByteSize` span against the current stack once before any
drop, after which finalized per-slot offsets can be addressed without repeating
the same bounds arithmetic. If both the byte-backed and dense mirrors carry
`NONE` ownership, the slot cannot release an owner and skips the release plus
overlap helpers. Every other ownership kind follows the original per-mirror
release path. On the exact scale-1 mixed-service pair, these two changes reduce
`314,490,481 -> 296,648,172 Ir` (`-5.673%`) with unchanged checksum; numeric and
object representatives change by only `+0.0043%` and `+0.0058%`.

Native dispatch now seeds `ZrLibCallContext` with the current VM frame's metadata function, callable base, local frame base, and inline argument start. `ZrLib_CallContext_InlineArgumentSpan` can be called from a real known-native callback and returns an address, byte size, alignment, and type id for inline struct arguments that already exist in the VM frame layout. Stack-root callbacks use the existing stack-layout anchor; stable-argument native lanes now also adopt a lightweight inline-frame anchor, so a callback that grows or relocates the stack before asking for the span receives the relocated frame payload address without converting stable argument copies back to old stack slots. The span API requires the resolved frame slot to be both inline and marked as a parameter, so inline locals cannot be exposed through an argument index. When an argument is an inline struct parameter, ordinary `ZrLib_CallContext_Argument` and the typed `Read*` helpers report it as unavailable instead of returning a stable `SZrTypeValue` copy, because the inline payload bytes are not a boxed value. Object known-native direct-binding fast paths also clear their local call-context copies before filling fields, so absent inline frame metadata remains explicit absence rather than uninitialized state. When the current call has no frame layout, the argument is not an inline parameter, or metadata resolution is unavailable, the span API reports unavailable and the existing boxed/native behavior remains unchanged.

`ZrLib_CallContext_InlineArgumentView` builds the provider-facing canonical view
on top of that span. It requires an attached metadata registry, resolves the
span's layout id through that registry, checks pointer identity plus layout
validation/size/alignment, and zeroes the output on every failure. Providers
borrow the span, layout, and registry only for the documented call/lifetime
contract; they do not reinterpret raw bytes from an unverified frame slot.

External native storage that embeds managed layout values uses
`SZrRawObject.traceGcFunction`, a visitor-based child-enumeration callback that
is separate from `scanMarkGcFunction` finalization. Classic mark, generational
live-young checks, and forwarding rewrite pass their own visitor through this
callback, so the same external slot can be marked or updated in place. Owners
with an external trace remain address-stable during old-object compaction;
their children may move and are rewritten. The direct full-compact free path
now runs object/array finalizers as well as native-data finalizers exactly once.

Native finalizers that own non-GC cleanup state store that context in
`SZrRawObject.finalizerData`. The context is independent of dynamic object
fields so shutdown finalization does not allocate an interned field-name string
or inspect a peer string object whose release order is not guaranteed. A
finalizer clears the context before releasing it, making repeated callbacks
idempotent without reading freed native storage.

## Imported Contiguous-View Layouts

Native `Span<T>` and `ReadOnlySpan<T>` remain owned by the provider module, but
their consumer bytecode still needs a resolvable inline frame layout. The
compiler therefore serializes a layout-only prototype record for imported
struct/union types that carry a mutable or readonly contiguous-view protocol.
`ZR_TYPE_MODIFIER_FLAG_IMPORTED_LAYOUT_ONLY` distinguishes this record from a
consumer declaration without changing the compiled prototype record size.

The runtime consumes the record for `SZrTypeLayout` resolution, skips prototype
creation/export, and binds the function's prototype instance to the already
loaded exact provider type or its open generic base. Import metadata inference
also ignores the record as a declaration. Types without a contiguous-view
protocol, including native scoped guards, are not admitted through this bridge.
This preserves provider pointer identity while allowing inline member-slot
instructions and AOT metadata to resolve the same layout.

## Current Boundary

The implemented boundary covers:

- POD inline copy with overlap-safe byte movement.
- Managed inline copy/drop for layouts containing embedded `SZrTypeValue` fields.
- Standalone value layout copy/drop that preserves existing `SZrTypeValue` ownership and struct clone semantics.
- Sequential frame layout calculation with per-slot alignment.
- Stack-base-relative byte offset load/save/copy.
- Typed stack frame places that resolve frame-relative byte offsets and copy layout-sized spans without exposing raw slot arithmetic to callers.
- Function-level frame slot places and layout-kind-checked frame slot inline copy.
- Constant-time frame-slot layout lookup for canonical dense compiled layouts, with the original linear fallback for sparse or reordered metadata.
- Struct prototype metadata round-tripping of whole-value `layoutByteSize` and `layoutByteAlign`.
- Function frame sidecar metadata for parameters, locals, generated stack slots, and `.zro` runtime loading.
- VM precall reserves enough legacy stack allocation units to cover `frameByteSize`, including prepared-call fallback and tail-call frame reuse.
- VM resolved/prepared pre-call copies already-inline caller frame payloads into callee inline parameter spans when both frame layouts and prototype layout resolution are available.
- VM resolved/prepared pre-call copies byte-backed VALUE parameters from original frame-layout source slots and mirrors them into the callee dense slot, with dense source fallback when the caller byte slot was not materialized.
- Tail-call VM frame reuse copies byte-backed VALUE parameters through logical frame slots, mirrors dense slots, and releases overwritten VALUE owners before reuse.
- VM single-result post-call copies already-inline callee return payloads into caller inline destinations before dropping the callee frame when both layouts resolve compatibly.
- Source-level struct constructors keep boxed/object materialization separate from the inline result span and copy the mutated constructor receiver back into inline frame storage.
- Ordinary scalar/object returns read callee logical frame value slots when the physical stack slot is no longer the canonical source.
- Known native, known native member, generic, meta, dynamic, and dynamic tail-call native fallback paths stage call windows outside caller frame storage so temporary frames cannot overwrite inline struct payload bytes.
- Ownership, object-conversion, typed equality/comparison, and fused branch opcodes read logical frame value slots under frame-layout metadata; weak expiry clears only matching weak slots.
- Tail-call frame reuse refuses inline-parameter callees until a layout-aware overlapping move exists, causing those calls to fall back to the non-reuse path instead of raw slot copying.
- Runtime prototype-index layout resolution for provable primitive POD fields, value-sized builtin reference fields, managed embedded `SZrTypeValue` fields, and local nested struct fields with managed embedded values, with checked failure for unsafe, pointer-sized reference, recursive, imported, or otherwise unknown metadata.
- GC mark and minor rewrite traversal for embedded inline-frame values through the resolved field layout, using metadata-runtime layout resolution for attached AOT code-registration functions and the prototype resolver for non-AOT VM frames.
- Generated AOT `SZrTypeLayout` descriptors for struct owner fields now include static ownership-offset arrays when the field offsets are provable; union and unsupported offset cases keep null ownership-offset pointers.
- Generated AOT code-stripping statistics report referenced inline type-layout payload bytes before and after reachability filtering via `code_stripping.typeLayoutPayloadBytesBefore/After/Removed`; the metric sums each distinct inline slot layout's `frameSlotLayout.byteSize` and is separate from emitted-C descriptor byte-span markers.
- Frame post-call and tail-call reuse drop wiring for owned embedded inline values.
- Immutable direct VALUE-only frame-drop summary with bounded per-slot address
  validation and checked fallback for every mixed or inline lifecycle layout.
- Native callback inline argument spans in the real dispatch context for already-inline VM frame payloads, including span refresh after native callback stack relocation in stack-root, stable fast/inline-pinned, and generic dispatcher lanes, with non-parameter inline slots rejected, ordinary boxed argument reads blocked for inline struct parameters, and missing/non-inline metadata preserving boxed argument reads.
- Canonical native inline argument views that fail closed on absent or drifting
  metadata registries, plus external-layout GC tracing across full compaction
  and barriered minor collection without conflating tracing with finalization.
- Runtime validation for local struct field mutation, frame-byte probes, by-value parameter mutation, by-value return mutation, large POD values, managed string fields, GC scanning of embedded value fields, constructor copyback, and nested struct field copy.
- AOT typed scalar generated C currently declares `sN/uN/fN/bN` locals for proven bool, signed `i64`, unsigned `u64`, and `f64` slots before dispatch; signed `i64` binary arithmetic, signed `i64` comparison, unsigned `u64` binary arithmetic, `f64` binary arithmetic, and signed `i64` binary bitwise can emit the first `sN = sN op sN` / `bN = (sN cmp sN)` / `uN = uN op uN` / `fN = fN op fN` expressions and mirror them back to frame slots. Signed `i64` shift, signed `i64` bit-not, fused signed branch comparisons, unsigned `u64` comparison, and the first numeric conversion source paths (`TO_UINT`, `TO_UINT_SIGNED`, `TO_UINT_FLOAT`, `TO_FLOAT`, `TO_INT_FLOAT`, and `TO_INT_UNSIGNED`) now reuse proven `sN/uN/fN` source locals when available, while the declarations remain a partial 04-S3 skeleton for other scalar operations.
- Typed union constructor materialization into inline frame bytes for POD and value-sized payload fields, using the same type-layout bridge as struct inline values.
- Typed union constructor assignment into existing inline locals, including replacement of an active owner payload before writing the new variant.
- Typed union inline pseudo-member reads for `__zr_unionVariant` and `__zr_unionPayloadN`, allowing `switch`/`using` pattern matching to read tag and POD payload bytes from inline local slots.
- Typed union inline pseudo-member writes for explicit `move` cleanup and struct-field inline union owner payload copy in both directions between typed locals and fields.
- Active-tag-aware inline union copy/drop/GC traversal for embedded value payload fields, including ownership release for payload fields with an ownership qualifier.

The remaining migration work is to move source-to-argument materialization and the full return/tail-call by-value payload model onto the byte-frame ABI beyond the already-inline pre-call copy, already-inline single-result post-call copy, and conservative tail-reuse fallback; broaden inline union member-level store/mutation matrices beyond typed-local copy, struct-field owner payload copy, and explicit move cleanup; extend union owner-payload stress coverage beyond the current multi-field and struct-field regressions if future layout limits appear; replace `typeLayoutId` as prototype index with an explicit module/function type-layout table if broader metadata needs it; implement platform ABI marshaling for struct declarations inside `native extern("library") { ... }`; expose a text `.zri` frame layout section if needed by tooling; and remove boxed-struct fallback paths only after escape, closure, native ABI, and reflection cases have equivalent inline handling.
