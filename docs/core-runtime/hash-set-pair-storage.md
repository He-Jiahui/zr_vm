---
related_code:
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/hash_set.c
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/hash_set.c
plan_sources:
  - user: 2026-09-05 repair the pooled HashSet removal prerequisite exposed by compiler integration
  - docs/plans/ssa/05-data-layout/03-maps-strings.md
tests:
  - tests/core/test_hash_set_dense_paths.c
  - tests/parser/test_compiler_regressions.c
  - tests/acceptance/2026-09-05-hash-set-pooled-removal.md
doc_type: module-detail
---

# Hash Set Pair Storage

## Purpose

`SZrHashSet` supports individually allocated key/value pairs and pairs carved
from `SZrHashPairPoolBlock` allocations. Native container arrays use pooled
pairs for dense append paths, so both allocation forms can occur in one set.
Removal and teardown must distinguish the two forms.

## Allocation Ownership

`ZrCore_HashSet_Add` allocates an independent pair. Pool reservation allocates
a block containing its header and pair array; `TakeReservedPair` returns an
interior pointer into that array. Only the block base is a valid allocator
deallocation target for pooled storage.

The private `zr_hash_pair_pool_contains` helper classifies a pair against the
set's pool block ranges. Both inline removal and final deconstruction use this
same check. It adds no public function or serialized metadata contract.

## Removal and Teardown

Removal unlinks the matching pair, decrements `elementCount`, and returns its
key as a shallow copy of the node's key slot. Standalone pairs are freed
immediately. Pooled pairs are not individually freed; their storage stays
owned by the set until deconstruction releases the complete pool blocks.

For a reference-counted owned key, the returned value carries the reference
already held by the pair. Removal does not retain or release that reference;
the caller receives it and must call `ZrCore_Ownership_ReleaseValue` once when
finished. This transfer contract does not define ownership teardown for
surviving entries: current deconstruction releases pair storage but does not
release key/value contents. Ownership cleanup for a nonempty set remains a
separate unresolved contract and is outside this Remove behavior.

The pool's `used` counters are allocation cursors, not live-element counts.
Removing a pooled pair does not decrement these cursors or introduce cell
reuse.

Deconstruction walks remaining bucket chains, frees their standalone pairs,
then frees every pool block once and releases the bucket array. Removed
pooled cells are therefore reclaimed even when no bucket still references
their block.

## Validation

`test_hash_set_dense_paths.c` covers pooled-only, standalone-only, and mixed
collision chains. A wrapper around the real runtime allocator records exact
hash-pair allocation bases and sizes. It rejects interior, duplicate, or
size-mismatched frees, checks that standalone removals free immediately, and
checks balanced allocation/free counts after deconstruction. Repeated removal
of the same key must remain a miss without changing the count. The owned-key
case initializes an ordinary managed object as unique, checks that Add stores
a retained shared key, then verifies Remove transfers that reference without
changing the count; releasing the original owner leaves the returned key
alive until the caller releases it.

The compiler integration fragmentation benchmark is the upper-layer replay:
native array removal reaches this hash-set contract after pooled string
appends. The defect already existed in baseline `c95e5387`; it is independent
of pending Shared/Weak control cleanup.
