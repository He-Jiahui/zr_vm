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
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool_internal.h
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool_type_layout.c
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/reflection.c
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
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_stable_slot_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_stable_slot_contract.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.h
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling_generational_runtime.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling_generational_runtime.h
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool.c
  - zr_vm_lib_container/src/zr_vm_lib_container/generational_pool_type_layout.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_argument_view.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/artifact_projection.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ref_struct_rules.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_stable_slot_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_metadata_provider.c
plan_sources:
  - user: 2026-07-19 按 docs/plans/syntax 严格执行并逐里程碑提交
  - user: 2026-08-03 严格按设计完成一次性破坏性切换并重新验收
  - user: 2026-08-04 确认 55 份状态、重新测试验收 review 后详细提交
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
  - docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md
tests:
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_span_core.c
  - tests/parser/test_span_semantic_ir_cases.c
  - tests/container/test_generational_pool.c
  - tests/container/test_generational_pool_gc_stress.c
  - tests/container/test_generational_pool_performance_matrix.c
  - tests/container/test_generational_pool_type_layout.c
  - tests/container/test_generational_pool_artifact.c
  - tests/container/test_pooling_closed_type_runtime.c
  - tests/language_server/test_lsp_stable_slot_contract_cases.h
  - tests/language_server/test_lsp_project_features.c
  - tests/core/test_inline_struct_array_layout.c
  - tests/library/test_official_provider_convergence.c
  - tests/acceptance/2026-08-05-syntax-10c-official-provider-convergence.md
  - tests/acceptance/2026-08-03-syntax-09-m3-canonical-pool-layout.md
  - tests/acceptance/2026-08-04-syntax-09-m2-guarded-direct-ref.md
  - tests/acceptance/2026-08-04-syntax-09-m5-performance-promotion.md
doc_type: module-detail
last_verified: 2026-08-04
---

# Pooling And Pinned FFI Views

## Purpose

Syntax03 M5 turns the owner/native source kinds introduced by Span core into real
library providers. `zr.pooling` owns reusable GC-tracked arrays through
`BufferPool` and affine `PoolLease<T>` values. `zr.ffi` exposes native bytes only
after `BufferHandle.pin()` creates an owner-rooted pointer handle. Both providers
consume the same structured contiguous-view contracts and compiler loan facts.

FFI handle objects retain their native cleanup payload in
`SZrRawObject::finalizerData`. The hidden object field remains the language and
native-call lookup surface, but finalization must not recreate its string key or
depend on the string table: GC shutdown may already be releasing peer string
objects. The finalizer clears the context before freeing the payload so a
repeated finalizer callback is a no-op rather than a read through freed memory.

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
was introduced in native plugin ABI v4. The current provider/type-role schema is
native plugin ABI v6, and the external trace field moves the native runtime ABI
to v4. Providers built against earlier descriptor/runtime layouts are rejected
during registration and must be rebuilt.

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

The native pool also accepts canonical `SZrTypeLayout` through
`ZrPool_CreateFromTypeLayout`. Element size, alignment, GC scan class, copy,
Drop, and scan traversal are derived from that one layout instead of a second
handwritten `SZrPoolTypeLayout`. Admission validates the complete copy path,
rejects move-only or dangling nested layouts, rejects root scan/Drop facts that
would hide a stronger nested lifecycle, requires a GC visitor for managed
layouts, and requires a live VM state for `SZrTypeValue` or value-slot storage.
A VM copy error rolls back canonical storage and never publishes a handle.
The VM state, nested layouts, registry pointer arrays, every field/offset table,
and callback user data referenced by their layouts are borrowed and must remain
valid until the pool is destroyed. The root layout value itself is copied.
Stateful canonical layouts are currently thread-local; concurrent admission is
rejected until operations can receive an isolation-domain-safe state per call.

The production provider consumes `ZrLibInlineArgumentView` from either an
artifact registry or the stable prototype-layout registry owned by an ordinary
source entry function. The first successful `deliver(T)` fixes the pool's
registry identity, layout id, layout/hash/size/alignment, and metadata function.
Later deliveries must match that same registry entry; a structurally identical
layout from another registry is rejected. The slab copies the inline bytes
directly through the canonical layout. The former permanent `__zr_pool_values`
mirror has been removed.

Pool slabs are native stable storage, so the pool owner exposes their embedded
GC values through `SZrRawObject.traceGcFunction`. Classic mark, remembered-set
minor checks, and forwarding rewrite invoke the same visitor contract. The
metadata function is traced and write-barriered when it is first bound, and
every successful element publication or writable-guard copyback barriers the
embedded managed values. Live and retired initialized slots are traced; free or
uninitialized slots are not. Object finalization remains a separate callback
and destroys the pool exactly once.

`PoolRef<T>`/`PoolReadRef<T>` materialize an object projection only for the
guard's lifetime. Read guards never write it back. Explicit writable close
copies the projection back into canonical inline storage and propagates a copy
failure while still releasing the guard. This temporary projection is not a
second persistent pool owner. Writable `ref T` member chains load the projection,
apply nested inline-struct updates, and store the completed value back through
the property reference before close. Ordinary variable/assignment/return value
contexts consume the temporary reference shell, while explicit `ref`/`out`
calls preserve its Place identity.

Compiler scope cleanup marks guard locals for close and emits cleanup on normal
block exit, return, throw, break, and continue. Reusing an `out` variable closes
its previous active guard before writing the replacement. Runtime `CLOSE_SCOPE`
saves the next bytecode PC before invoking native close metadata and restores
the normal native-call frame/exception state afterward, so nested close cannot
re-enter an instruction that still refers to the now-closed projection.

An ordinary source child function does not need `metadataCodeRegistration` to
produce `ZrLibInlineArgumentView`. The core lazily resolves the required
prototype layout and publishes one entry-function-owned registry whose backing
outlives every borrowed call view and pool binding. Nested ready layouts share
that same table, and repeated calls preserve registry identity. If any artifact
registration is attached, however, its metadata-runtime registry is the only
authority: invalid or missing artifact entries fail closed instead of falling
back to source prototype metadata.

The native module publishes that hash as a descriptor constant. The generic
artifact projection maps it to the StableSlotSource layout capability by
protocol id, not provider name. Source import, native metadata, binary artifact,
canonical consumer and reflection therefore observe one hash; zero, dangling or
unknown layout capabilities fail closed.

Native reflection classifies a struct as ref-like from its registered
`REF_LIKE` protocol, even when there is no compiled source prototype row. Fields
marked `runtimeOnly` are omitted. A method carrying the
`POOL_REF_PROJECTION` role is not exposed under its hidden provider spelling;
reflection instead publishes one visible getter-only property with the exact
readonly/writable reference access. The linked getter is descriptive metadata,
not a path that creates or retains a guard. Ref-like pool views remain
non-constructible through reflection.

The language server classifies stable-slot types from `protocolMask`, member
`contractRole`, and structured reference access. Hover therefore labels the
handle as weak identity, the provider as a stable slot source with its actual
acquire member names, and the guard view as a scoped readonly/writable ref.
Property hover identifies the getter-only projection and its active-guard
lifetime. Completion follows the real API owner: acquisition methods appear on
the stable-slot source, while a handle exposes only its identity surface. No
consumer compares `Pool`, `PoolHandle`, `PoolRef`, or their member spellings.

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
`zr_vm_generational_pool_performance_matrix_test` records successful slab
allocation events separately from current slab/slot state. Its deterministic
matrix compares 65,536 per-item class-storage allocations with 256 slabs per
pool, separates GcFree zero-scan work from GcMapped payload scan work, records
the larger per-item object traversal, and reports thread-local versus concurrent
ticks for one million validations. Allocation/slot/byte/validation counts are
asserted; clock ticks are observational because scheduler and toolchain variance
must not make the regression test flaky. The target passes 2/2 under GCC 11.4,
Clang 14, and MSVC 19.44.
`zr_vm_generational_pool_artifact_test` executes the source constant and proves
native/binary/reflection hash parity plus corrupt-layout rejection.
`zr_vm_generational_pool_type_layout_test` covers canonical GcFree and GcMapped
admission, visitor routing, exactly-once deferred Drop, managed-state and visitor
requirements, VM copy-error rollback, stateful-concurrency rejection, successful
nested managed scan/Drop, missing copy paths, raw-copy bypass, and nested
scan/Drop downgrade rejection.
`zr_vm_pooling_closed_type_runtime_test` covers mirror-free inline publication,
strict registry identity, stable source-registry ownership, invalid-id clearing,
artifact-registry fail-closed behavior, source-level writable struct member
chains, ordinary property-reference value loads, explicit close/readback, and
writable guard object-to-inline copyback without a fabricated code registration.
`zr_vm_property_ref_return_test` and `zr_vm_property_access_lowering_test` cover
ref/out identity, nested writeback, binary roundtrip, and C/LLVM AOT emission.
`zr_vm_inline_struct_array_layout_test` additionally proves that an
external trace keeps and rewrites managed children across full compaction and a
barriered minor collection, while finalization remains exactly once.
`zr_vm_language_server_lsp_project_features_test` proves name-independent
stable-slot classification, source acquisition completion, weak-handle/source/
guard hover, and getter-only active-guard property hover. The same suite keeps
descriptor-plugin semantic tokens green with canonical `fn` declarations after
the one-time syntax cutover.

## Follow-up Boundary

This milestone provides exact-length reusable arrays and pinned byte-buffer views.
Size-class pooling, arbitrary typed native slices, and custom marshallers remain
separate work. A movable managed-slab implementation or
stateful concurrent admission may be added later, but the current design-valid
stable native slab and fail-closed concurrency boundary do not prevent Gate 09
promotion. Future variants must extend the structured protocol, TypeLayout, and
artifact contracts rather than adding provider-name recognition.

## Official Provider Identity

`zr.pooling` is an N2 Runtime provider with public contract hash
`zr.pooling:v1:stable-slot-generational-pool`. Syntax 10C obtains the real
descriptor through `ZrVmLibContainer_GetPoolingModuleDescriptor`, validates it
against the frozen official inventory, and confirms `Pool`/`PoolRef` ownership
without manufacturing a placeholder provider.
