---
related_code:
  - zr_vm_lib_container/include/zr_vm_lib_container/module.h
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.h
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling_generational_runtime.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling_generational_runtime.h
  - zr_vm_lib_container/include/zr_vm_lib_container/generational_pool.h
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/include/zr_vm_parser/artifact_projection.h
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ref_struct_rules.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.h
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling_generational_runtime.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling_generational_runtime.h
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ref_struct_rules.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
plan_sources:
  - user: 2026-07-19 按 docs/plans/syntax 严格执行并逐里程碑提交
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
  - docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md
tests:
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_span_core.c
  - tests/parser/test_span_semantic_ir_cases.c
  - tests/container/test_generational_pool.c
  - tests/container/test_generational_pool_gc_stress.c
  - tests/container/test_generational_pool_artifact.c
doc_type: module-detail
---

# Pooling And Pinned FFI Views

## Purpose

Syntax03 M5 turns the owner/native source kinds introduced by Span core into real
library providers. `zr.pooling` owns reusable GC-tracked arrays through
`BufferPool` and affine `PoolLease<T>` values. `zr.ffi` exposes native bytes only
after `BufferHandle.pin()` creates an owner-rooted pointer handle. Both providers
consume the same structured contiguous-view contracts and compiler loan facts.

## Pool Lease Model

`BufferPool.rent<T>(length)` chooses an exact-length backing array from its
available list or creates a new one. The backing array is held by a temporary GC
root before it is removed from the pool or populated, so an allocation-triggered
collection cannot reclaim the array in the ownership handoff. A returned lease
clears every element before the backing is made available again.

Each lease records its pool owner, backing array, active length, monotonic
generation, and returned bit. `close()` is single-return and idempotent: the first
successful close clears and returns the backing, drops the lease's backing
reference, sets length to zero, and increments `returnCount`; later closes do
nothing. `reuseCount` increments only when rent actually consumes a previously
returned array. A returned lease cannot index or create another view.

The type descriptor publishes `CONTIGUOUS_SOURCE_OWNER`, the exact index-length
field role, the view-create method role, index meta methods, and close meta. The
compiler therefore derives the mutable source loan from protocol/role and the
resolved receiver Place. A later Span use keeps the loan live and rejects lease
close/reuse; when the view reaches non-lexical last use, close becomes legal.

## Generational Stable-Slot Pool

`zr.pooling.Pool<T>` publishes the provider-neutral `STABLE_SLOT_SOURCE`
protocol. Its ordinary `PoolHandle<T>` contains only the pool identity, slot
index, and generation, so a handle remains a weak, storable identity and never
acts as a direct reference. A successful `deliver` publishes the handle only
after element initialization completes. `recycle` immediately retires the
identity, rejects later acquisition, and delays physical drop and slot reuse
until every active guard has closed.

`tryRead(handle, view: out PoolReadRef<T>): bool` and
`tryBorrow(handle, view: out PoolRef<T>): bool` are the canonical acquisition
contracts. The native descriptor carries the parameter passing mode as
structured metadata; the native metadata projection and parser import path
preserve it as `EZrParameterPassingMode`, rather than encoding `out` in a type
name. Runtime-only backing fields, readonly field facts, and ref-property
access contracts are also structured descriptor fields. This descriptor schema
is native plugin ABI v4. Providers built against ABI v3 or earlier are rejected
during registration and must be rebuilt; the runtime ABI remains v3.

Imported `PoolRef<T>` and `PoolReadRef<T>` are ref-like because the compiler
queries their canonical `REF_LIKE` capability. The storage and escape passes do
not compare either source spelling. The same rule rejects imported views in
class/global/array/unconstrained-generic/native-ABI storage, array literals,
closure captures, and values live across `await` or `yield`.

Read guards may coexist, while a write guard excludes every other guard. A
retired entity remains readable through guards acquired before retirement; the
last guard release performs the pending drop and permits reuse with a new
generation. The runtime keeps fixed-capacity slabs, validates full
pool/slot/generation identity, permanently exhausts a slot before generation
wrap, and scans only initialized live or retired elements according to the
closed element layout's `GcFree`, `GcMapped`, or `GcBarriered` classification.
`GcBarriered` slabs maintain a dirty bit per initialized slot. Initial
publication and write-guard acquisition mark the slot; scanning clears a card
only when no writer is active, so a direct pointer held by a writer cannot lose
its remembered-set obligation. `GcMapped` scans every initialized live/retired
slot and `GcFree` reports zero scan bytes.

Failed initialization never publishes a handle. A layout may provide
`abortInitialize` to release partially initialized fields before the slot is
zeroed and returned to the free list. Construction-failure, partial-cleanup,
drop, validation, barrier, dirty-slot, scan-pass, slot and byte counters remain
separate. The StableSlotSource contract hash is version 2 after adding this
rollback/barrier contract.

The native module publishes that hash as a descriptor constant. The generic
artifact projection maps it to the StableSlotSource layout capability by
protocol id, not provider name. Source import, native metadata, binary artifact,
canonical consumer and reflection therefore observe one hash; zero, dangling or
unknown layout capabilities fail closed.

## Exception Cleanup

`using (lease)` registers the lease in the VM to-be-closed chain. Every exception
handler captures the chain boundary present at entry. Exception unwind closes
only registrations above that boundary before entering a matching catch/finally
or popping the handler. The close call builds its error argument in reserved
scratch storage and reloads the resource after possible stack relocation, so it
cannot overwrite an adjacent local or callable slot.

This makes a thrown pool scope equivalent to an explicit successful return: the
next rent can reuse the same backing and counters still reflect one return.

## Explicit Native Pin Model

`BufferHandle.pin()` is the only path that turns its allocation into a
native-pinned contiguous source. It rejects an already closed owner, increments
the owner's pin count, and returns a pointer handle that retains the owner object,
the native address, and its byte length. `BufferHandle.close()` records close
immediately but defers freeing bytes until the last pin releases the owner.

`Ptr<u8>.span()` creates a `Span<u8>` whose source is the pointer handle. The
pointer descriptor publishes `CONTIGUOUS_SOURCE_NATIVE_PINNED`, length, view-create,
and byte-index roles. A full moving/compacting GC may run while the view is live:
the pointer's owner reference keeps the buffer allocation alive and native bytes
do not move. Pointer close is idempotent and releases the owner pin only once;
finalization does not release it again after explicit close.

The safe view surface is intentionally byte-oriented for the buffer pin path.
Arbitrary native pointers do not acquire a length or stable lifetime implicitly,
and a ZR ref-like value is not passed through a native ABI without an explicit
marshaller/lowering contract.

## Failure Boundaries

- Negative rent lengths and exhausted generations fail before allocation.
- Active owner views prevent lease close, return, or reuse.
- Active native views prevent pointer close/unpin.
- Closed leases reject indexing and view creation.
- Closed buffers reject new pins; zero-length pins remain valid empty views.
- Pointer byte indexing checks `0 <= index < length`; writes accept only `0..255`.
- Pool and pointer close operations are idempotent and cannot decrement ownership
  counters twice.

## Verification

`zr_vm_buffer_pool_ffi_test` covers descriptor contracts, exact-generation reuse,
double close, live-view close rejection, 32 rent/view/full-compact-GC/return
rounds, `using` cleanup through throw/catch, pinned byte mutation after owner
close and full compact GC, idempotent unpin, and live native-view unpin rejection.
The same target also validates the cross-module ref-like artifact ABI consumed by
VM and AOT projections.

`zr_vm_generational_pool_test` covers descriptor roles, canonical import,
ref-like storage/escape rejection, identity/ABA, read/write conflict, deferred
reclamation, generation exhaustion, alignment, cross-slab expansion, one
million handles and concurrent churn. `zr_vm_generational_pool_gc_stress_test`
separates partial-init rollback, GcFree/GcMapped/GcBarriered scan accounting,
card retention during a write guard, one million direct-field accesses without
another generation validation, and 100,000 reuse cycles.
`zr_vm_generational_pool_artifact_test` executes the source constant and proves
native/binary/reflection hash parity plus corrupt-layout rejection.

## Follow-up Boundary

This milestone provides exact-length reusable arrays and pinned byte-buffer views.
Size-class pooling, arbitrary typed native slices, custom marshallers, managed
moving-slab compaction, and language-level early-exit cleanup for native pool
guards remain separate work. They must
extend the structured protocol, TypeLayout, and artifact contracts rather than
adding provider-name recognition.
