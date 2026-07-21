---
related_code:
  - zr_vm_lib_container/include/zr_vm_lib_container/module.h
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.h
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/runtime.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
plan_sources:
  - user: 2026-07-19 按 docs/plans/syntax 严格执行并逐里程碑提交
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_span_core.c
  - tests/parser/test_span_semantic_ir_cases.c
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

## Exception Cleanup

`%using (lease)` registers the lease in the VM to-be-closed chain. Every exception
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
rounds, `%using` cleanup through throw/catch, pinned byte mutation after owner
close and full compact GC, idempotent unpin, and live native-view unpin rejection.
The same target also validates the cross-module ref-like artifact ABI consumed by
VM and AOT projections.

## Follow-up Boundary

This milestone provides exact-length reusable arrays and pinned byte-buffer views.
Size-class pooling, concurrent pools, arbitrary typed native slices, custom
marshallers, and generational slab handles remain separate designs. They must
extend the structured protocol, TypeLayout, and artifact contracts rather than
adding provider-name recognition.
