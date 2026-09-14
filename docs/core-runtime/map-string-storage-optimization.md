---
related_code:
  - zr_vm_core/src/zr_vm_core/hash_set.c
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_core/src/zr_vm_core/string_builder.c
  - zr_vm_core/src/zr_vm_core/hash.c
  - zr_vm_core/include/zr_vm_core/container_storage_contract.h
  - zr_vm_core/src/zr_vm_core/object/container_storage_contract.c
  - zr_vm_library/include/zr_vm_library/container_storage_contract.h
  - zr_vm_library/src/zr_vm_library/container_storage_contract.c
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/include/zr_vm_core/string_builder.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/container_storage_contract.h
  - zr_vm_core/src/zr_vm_core/object/container_storage_contract.c
  - zr_vm_library/include/zr_vm_library/container_storage_contract.h
  - zr_vm_library/src/zr_vm_library/container_storage_contract.c
plan_sources:
  - docs/plans/ssa/05-data-layout/03-maps-strings.md
  - user: 2026-09-13 implement 05.03 maps/strings with conservative hash and ownership contracts
tests:
  - tests/core/test_ssa_maps_strings.c
  - tests/core/test_hash_set_dense_paths.c
  - tests/core/test_string_layout.c
  - tests/core/test_string_builder.c
doc_type: module-detail
status: implemented-subset
---

# Map and String Storage Optimization Contract

## Purpose

The existing runtime already owns the behavior of hash sets, interned short
strings, immutable long strings, and the native temporary string builder. This
module adds the missing proof boundary for a future compact-map or string
strategy without introducing a second container implementation. A contract is
an optimisation permission: when a fact is unknown, callers keep the existing
generic path.

## Related Files

`hash_set.h` retains the linked-bucket implementation and its GC-aware pair
pool. `string.h`/`string.c` retain short-string interning and the long-string
hash; `string_builder.h`/`string_builder.c` retain a caller-owned mutable
buffer that is frozen through `ZrCore_String_Create`. The new core contract is
in `container_storage_contract.h` and its implementation is deliberately under
the object source directory so it remains part of the core storage layer. The
library header and forwarding source expose the same scalar witness at the
public library boundary.

## Behavior Model

### Compact map witness

`SZrCompactMapCandidate` contains a fixed-width hash witness and a compact
entry/bucket layout. `ZrCore_CompactMapCandidate_Finalize` computes a layout
hash and a complete candidate hash; `Validate` recomputes both before a
consumer can use the record. Entry offsets, alignment, power-of-two bucket
capacity, and load-factor bounds are checked without dereferencing an entry.

Hash caching is allowed only when keys are immutable, hashing and equality are
pure and total, the seed/domain/version are non-zero and domain-stable, and
neither custom equality nor identity observation is present. The seed, domain,
and version are included in `ZrCore_CompactMap_HashBytes`, so a hash from
another runtime domain is never silently reused. `ProbeNeedsEquality` returns
true for a matching hash as a reminder that a collision still requires the
language equality operation; it does not invoke or bypass that operation.

The layout contract makes deletion and iteration explicit. Insertion-order
iteration requires marked tombstones; backshift deletion is rejected for that
mode because it can reorder observable entries. GC-rooted ownership must be
declared with the GC-root flag, and the ownership policy and flag must agree in
both directions. The contract contains no entry pointer, bucket pointer,
callback address, or managed object address.

### String candidates

`SZrStringStorageFacts` records immutable/escape/identity/effect facts and
measurement values as scalar fields. The eligibility helpers are intentionally
separate:

* SSO requires an immutable, non-observable byte sequence whose length fits the
  declared short-string limit.
* Interning requires a short-string byte length, a weak/GC-managed retention
  policy, and a non-zero stable intern domain. This prevents an intern table
  from becoming an unbounded strong owner and keeps the existing long-string
  path unchanged.
* Builder lowering requires non-escaping intermediates, no identity/FFI
  observation, a bounded output size, and exact equality of declared and
  replacement effect masks. Exception-order-visible or custom-equality input
  is rejected. The same exception-order/identity/escape gates apply to the
  other candidate strategies when they could change an observable allocation.
* Rope storage is only a candidate after measured copy-byte benefit is present,
  flattening is byte-budgeted, and depth/segment limits are non-zero and
  respected. No rope object or segment pointer is created by this contract.

`SelectStrategy` chooses intern before SSO (the existing short-string path is
already canonicalized by the string table), then builder or rope, and falls
back to `GENERIC` when none is proven. `BuildCandidate` copies only scalar
facts into a pointer-free strategy record and seals it with a hash.

## Design and Rationale

The runtime's current string table and builder already implement the required
ownership and immutable-freeze semantics. Reimplementing either in a new fast
path would risk duplicate intern entries, stale GC roots, or a mutable string
escaping through an identity comparison. The contract therefore records what a
future parser/ExecIR pass may assume while leaving allocation and equality in
the existing owners.

The hash transcript uses a fixed FNV-1a sequence over seed, domain, version,
length, and bytes. It is deterministic across toolchains and deliberately
separate from the process-randomized runtime hash. Candidate hashes use the
same fixed-width encoding, making malformed or cross-version cache records
fail closed.

The library layer is a thin forwarding boundary, analogous to the existing
native-call plan facade. It does not retain pointers or grant additional
capabilities; providers still have to validate the core witness and execute
the normal generic equality/GC paths.

## Edge Cases and Constraints

* Null hash data is valid only for a zero-length byte sequence; invalid input
  or a malformed (zero-seed/unknown-flag) hash witness returns hash zero.
* Non-power-of-two bucket counts, zero/overflowed offsets, unknown flags, and
  invalid ownership/iteration combinations are rejected.
* Empty strings are valid SSO candidates. Long strings are not silently
  truncated to the short limit.
* Any escape, address escape, identity observation, FFI visibility, or custom
  equality blocks the transformations that could change allocation or object
  identity.
* A rope candidate must fit its flatten byte budget before it is published;
  flattening remains a separately budgeted runtime operation.
* The records are cache-safe scalar descriptions, not proof that a particular
  managed object remains alive. Roots, leases, and final equality calls remain
  the responsibility of the owning runtime path.

## Test Coverage

`tests/core/test_ssa_maps_strings.c` exercises deterministic seeded hashing,
domain separation, collision-equality gating, map layout finalization,
custom-equality rejection, SSO/intern/builder eligibility, identity rejection,
and measured/budgeted rope candidates. Existing dense hash-set, string-layout,
and builder tests remain the semantic regression suite for the underlying
implementations. The focused fixture is intentionally standalone in this
slice; CMake/CTest registration is left to the integration owner.

## Plan Sources

This is the conservative core/library portion of
`docs/plans/ssa/05-data-layout/03-maps-strings.md`. It covers the storage
contract and negative gates in batches 1–2. Parser lowering, measured benchmark
promotion, and any actual rope representation remain separate work.

## Open Issues or Follow-up

The parent integration should register `test_ssa_maps_strings.c` as
`ssa_maps_strings` after the shared test manifest is stable, then run the
collision/Unicode/allocation benchmark matrix. A later parser pass may consume
`SZrStringStorageFacts`; it must preserve the generic path for unknown custom
equality and must not publish a builder or rope merely because a candidate
record validates.

This slice has no allocation, cancellation, or partially initialized managed
object entry point: all records are caller-owned scalar values, and failure
leaves the existing map/string implementation untouched. Those lifecycle
cases belong to the future owner that materializes a validated candidate.
