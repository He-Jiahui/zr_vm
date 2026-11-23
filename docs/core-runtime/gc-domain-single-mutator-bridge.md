---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_sweep.c
  - zr_vm_core/src/zr_vm_core/gc/gc_roots.c
  - zr_vm_core/src/zr_vm_core/gc/gc_temporary_roots.h
  - zr_vm_core/src/zr_vm_core/exception/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/object/object.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_sweep.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_resource.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/object/object.c
plan_sources:
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
tests:
  - tests/core/test_gc_domain_bridge.c
  - tests/gc/gc_tests.c
  - tests/parser/test_resource_unique_drop.c
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_pre_semantic_ir.c
doc_type: module-detail
---

# GC Domain Single-Mutator Bridge

## Domain identity

Syntax 04 M4 gives every runtime state a live `GcDomain` owned by its global state. Domain
creation, state attachment, detachment, and destruction remain internal runtime operations in
this milestone. Each domain has a stable id plus a generation, and each managed object records
that exact pair when allocated. Public queries compare both fields; an id match without the
current generation is not sufficient.

The M4 topology is deliberately single mutator. A state is attached to one domain, and the
domain's collector and precise roots are operated by that state. Public mutator registration,
domain-local stop-the-world handshakes, native safepoint modes, and same-domain handoff belong to
M5 and are not implied by this implementation.

## Precise root slots

`SZrGcRootHandle` identifies a domain root slot by domain identity, slot index, and slot
generation. The public C API supports create, clone, update, resolve, and release:

- clone retains the same slot;
- updating a shared handle performs copy-on-write so the clone keeps its original target;
- release invalidates the caller's handle and increments slot generation before reuse;
- resolve rejects a stale domain generation or stale slot generation;
- minor, major, and compact rewrites update the slot target before mutator code can resolve it.

Direct resource owners use a second structured root kind in the same table. `Unique`, `Shared`,
and their stable control lifetime keep a live resource object in an ownership root slot. Final
release removes that slot. This replaces the former GC ignore-registry bridge: collector reachability
now comes from explicit domain roots, and no hidden ignored-object set participates in correctness.

Major collection also scans permanent parents. Permanent objects remain permanent, but their
managed children are marked for the current major generation. A permanent module or prototype
therefore cannot retain a stale pointer to a collected child.

## Cross-domain write gate

Ordinary managed edges are domain-local. Value barriers and object member/index storage validate
the owner first and then every GC-managed target, including a dynamic managed key. The store is
rejected before copy or mutation when the owner is not in the current domain or when a target has
a different domain identity. Null and non-GC scalar values remain valid.

This gate does not transport an object and does not make cross-domain pointers safe. Structured
clone, immutable handles, resource move envelopes, and domain shutdown races are M6 work.

## Owner-to-GC bridge

`intoGc(owner)` is an explicit consuming bridge for `Unique<Resource>`. The compiler publishes
`ZR_SEMANTIC_OWNERSHIP_INTO_GC_BOX` with the source Place, rejects Shared and active-borrow input,
and lowers the operation through the canonical `OWN_INTO_GC_BOX` execution slot. VM execution
calls `ZrCore_Ownership_IntoGcBoxValue`; AOT C and LLVM use the matching
`ZrLibrary_AotRuntime_OwnIntoGcBox` boundary. `OWN_DETACH` remains only as an old-artifact reader
identity and is not emitted for current source.

The resulting GC-managed box owns the resource payload. Reaching the box delays Drop; collecting
the box executes Drop exactly once, including when Drop allocates and reaches a safepoint. The
source owner is moved and cannot be used again.

`Gc<T>` and `GcBox<T>` have distinct canonical bridge kinds rather than ownership qualifiers.
`Gc<T>` accepts an ordinary GC class target, while `GcBox<T>` accepts a resource target. M4
publishes and tests the source type contract and the public C `SZrGcRootHandle` carrier. It does
not yet claim a general source-language constructor that turns arbitrary `T` into `Gc<T>`.

## Single-owner fast path

The interpreter r2 work (R2-M2, after M4) added a fast path for a domain that is used by one
native thread. This section only records how the code at tip behaves. It does not change domain
identity, root handles, ownership roots, or the cross-domain write gate.

Owner-thread mutex elision:

- `ZrCore_GcDomain_CoordinationInit` (`gc/gc_domain_mutator.c`) records the constructing thread as
  the domain's `elisionOwner` and sets `elisionMode` to enabled. The modes are 1 enabled,
  2 draining, and 0 native mutexes (`gc/gc_domain_internal.h`).
- While the mode is enabled, `ZrCore_GcDomain_Lock` and `ZrCore_GcDomain_MutationLock` on the owner
  thread do not take the native mutex. They only increment the atomic `elidedCoordinationDepth` or
  `elidedMutationDepth` counter, and the matching unlock decrements it.
- Draining starts when a lock call comes from a thread other than the owner, or when
  `ZrCore_GcDomain_RegisterMutator` registers a second mutator. A foreign root API has no
  registered mutator, but it still goes through these locks, so it also starts draining. The
  mode switches from enabled to draining. A non-owner lock caller then yields until both elided
  depths are zero, and the mode becomes native mutexes. Owner scopes that are already nested may
  still finish during the drain. No code path switches a domain back to enabled, so the change is
  permanent.
- `ZrCore_GcDomain_TryExclusiveMutation` gives `ZrCore_GarbageCollector_Barrier` (`gc/gc_mark.c`)
  an elided exclusive mutation scope. The barrier returns early inside that scope only when there
  is no ignored-root work, no old-or-pinned to young edge to remember, and no marking or sweeping
  work for the edge. Otherwise it does the full barrier work: inside the exclusive scope when it
  got one, or under `ZrCore_GcDomain_SharedMutationBegin` when it did not.

Per-state LIFO temporary roots:

- `SZrState.temporaryRoots`, `temporaryRootLength`, and `temporaryRootCapacity`
  (`zr_vm_core/include/zr_vm_core/state.h`) form a LIFO root stack for each state.
- `ZrCore_Gc_TemporaryRootPush` and `ZrCore_Gc_TemporaryRootsRestore(state, depth)`
  (`gc/gc_temporary_roots.h`, `gc/gc_roots.c`) run under the domain mutation lock. On the owner
  thread that lock is elided.
- A push grows the stack with the raw allocator, which does not collect. If the growth or the
  reference count fails, the push returns false and leaves the existing entries unchanged.
- Each entry increments the object's `temporaryRootRefCount`.
  `garbage_collector_object_supports_evacuation` (`gc/gc_cycle.c`) refuses to move an object
  while that count is non-zero, so native pointers to it stay valid.
- Collection marks the entries in `garbage_collector_mark_aot_root_frames` (`gc/gc_mark.c`) and
  rewrites them in `garbage_collector_rewrite_aot_root_frames` (`gc/gc_cycle.c`).
- `ZrCore_Exception_TryRun` (`exception/exception_try_run.c`) saves the depth on entry and
  restores it when it unwinds.
- Current users include object member/index writes (`object/object_write.c`) and short-string
  interning (`string/string.c`). Generic `ZrCore_Gc_RootLeaseAcquire` leases still exist for other
  callers.

The branch-authored measurements for this change are in
`docs/benchmarks/interpreter-review-20261018-r2-results.md` ("R2-M2"). They have not been
re-measured at tip.

## Verification boundary

The focused domain suite covers identity, cross-domain writes, handle clone/update/release,
stale generations, explicit ownership roots, minor/major/compact rewrites, and permanent-parent
child scanning. Resource tests cover deferred and exactly-once GcBox Drop, Drop-time allocation,
canonical bridge types, source `intoGc(owner)`, active-loan rejection, VM execution, and AOT helper
order.

M4 does not provide multi-mutator collection, domain-local STW, transfer envelopes, cross-domain
transport, or concurrent major marking. Those remain the M5-M7 promotion gates.
