---
related_code:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - user: 2026-05-16 struct inline stack storage and memcpy parameter passing
  - user: 2026-05-18 real GC/native entry wiring without claiming full ABI completion
tests:
  - tests/core/test_type_layout_inline_copy.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/core/test_native_inline_span_dispatch.c
  - tests/gc/gc_tests.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_compiler_integration_main.c
  - tests/acceptance/2026-05-16-inline-struct-byte-stack.md
  - tests/acceptance/2026-05-18-inline-frame-gc-native-entry.md
doc_type: category-index
---

# Core Runtime

Core runtime documents cover VM stack storage, call-frame data movement, ownership-aware inline values, and low-level execution helpers.

- `inline-type-layout-and-byte-stack.md`: type layout descriptors, POD inline copy, field-aware copy/drop, byte-offset stack copy primitives, struct prototype `layoutByteSize/layoutByteAlign`, function frame byte-layout sidecar metadata, runtime prototype layout resolution, VM pre-call and single-result post-call copy for already-inline payloads, conservative tail-reuse fallback for inline parameters, GC/drop traversal, and real native inline-span dispatch context with stack-relocation refresh and span-only inline parameter access for the inline stack migration.
