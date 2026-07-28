---
related_code:
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/include/zr_vm_core/task_frame_runtime.h
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/include/zr_vm_core/property_reference.h
  - zr_vm_core/src/zr_vm_core/property_reference.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/ownership_transfer.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_telemetry.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_lifecycle.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
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
  - zr_vm_core/include/zr_vm_core/task_frame_runtime.h
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_core/include/zr_vm_core/constant_reference.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/property_reference.c
  - zr_vm_core/src/zr_vm_core/function_type_layout.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/object/object_index_contract_direct_binding.c
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/ownership_transfer.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_lifecycle.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
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
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/core/test_type_layout_inline_copy.c
  - tests/task/test_task_frame_runtime.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_precall_frame_slot_reset.c
  - tests/core/test_object_call_known_native_fast_path.c
  - tests/parser/test_property_access_lowering.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_resource_same_domain_handoff.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/core/test_resource_cross_domain_transfer_races.c
  - tests/core/test_native_inline_span_dispatch.c
  - tests/gc/gc_tests.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_compiler_integration_main.c
  - tests/parser/test_buffer_pool_ffi.c
  - tests/acceptance/2026-05-16-inline-struct-byte-stack.md
  - tests/acceptance/2026-05-18-inline-frame-gc-native-entry.md
doc_type: category-index
---

# Core Runtime

Core runtime documents cover VM stack storage, call-frame data movement, ownership-aware inline values, and low-level execution helpers.

- `task-frame-runtime.md`: structured Task/frame state, synchronous no-allocation completion,
  suspension-only promotion, layout-declared GC/drop maps, result roots, non-Copy transfer,
  and typed frame pooling without a dynamic-object coroutine fallback.
- `inline-type-layout-and-byte-stack.md`: type layout descriptors, POD inline copy, field-aware copy/drop, byte-offset stack copy primitives, struct prototype `layoutByteSize/layoutByteAlign`, function frame byte-layout sidecar metadata, runtime prototype layout resolution, VM pre-call and single-result post-call copy for already-inline payloads, conservative tail-reuse fallback for inline parameters, GC/drop traversal, and real native inline-span dispatch context with stack-relocation refresh and span-only inline parameter access for the inline stack migration.
- `property-accessor-dispatch.md`: descriptor-backed getter/setter dispatch, receiver-source frame/slot
  provenance for inline structs, cache and exception boundaries, static/virtual/interface behavior,
  source/artifact parity for typed compound property access, and managed class/inline/index/static
  reference Places for `ref` and `ref readonly` getters; reflection joins visible properties and
  accessors by exact property identity/role and keeps legacy-looking ordinary methods separate.
- `exception-scope-resource-cleanup.md`: exception-handler checkpoints for `%using`
  registrations, LIFO close before catch/finally, scratch-safe error arguments, and
  stack-relocation rules for resource cleanup.
- `iterator-frame-runtime.md`: synchronous caller-owned iterator frame state,
  GC-rooted current values across compaction, exactly-once terminal cleanup,
  recursive-entry rejection, and opt-in typed free-list frame reuse.
- `gc-domain-single-mutator-bridge.md`: single-mutator `GcDomain` identity, generation-checked
  root handles, explicit ownership roots, cross-domain write rejection, permanent-parent major
  scanning, and the `Unique<Resource>.intoGc()` / GcBox runtime bridge.
- `gc-domain-multimutator-and-owner-handoff.md`: domain-local STW epoch/handshake, registered
  VM/AOT roots, native safepoint modes, interpreter poll/reload, and the same-domain
  `Unique<Resource>` TransferEnvelope state machine.
- `gc-domain-concurrent-major.md`: incremental/concurrent major snapshot-mark-remark lifecycle,
  concurrent write-barrier closure, budgeted compaction, full-collection cancellation and
  per-domain GC/transport telemetry.
- `cross-domain-transfer-contracts.md`: artifact-reproducible cross-domain transfer kinds,
  layout/provider identity, ValueCopy and StructuredClone payloads, ResourceMove
  DropOnFailure, quotas, stale generation, shutdown, and release/acquire race contracts.

Global teardown preserves code lifetime across GC destruction: project-owned AOT functions
are unpinned before collector teardown, while their dynamic-library handles are closed only
by the single-owner post-GC cleanup registered on `SZrGlobalState`. A conflicting cleanup
owner is rejected so an unsafe early unload cannot be introduced by state aliasing.

Functions with a typed frame layout treat every logical stack slot as GC-visible frame state.
Their VM pre-call reset therefore clears the full logical `stackSize`, including temporary
slots above the parameter prefix, before execution starts. Untyped legacy frames keep the
smaller parameter-prefix reset so their established transient-slot reuse remains unchanged.
This distinction prevents stale bytes in a typed temporary slot from being interpreted as a
managed reference when the collector walks `frameSlotLayouts`.
