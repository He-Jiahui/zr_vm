---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_internal.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_internal.h
plan_sources:
  - docs/plans/astra/syntax/ownership-object-member-separation.md
tests:
  - tests/parser/test_ownership_member_cache_null_cases.h
  - tests/parser/test_resource_shared_weak.c
doc_type: module-detail
---

# Cold Member Cache Null Safety

The interpreter's cached read and write paths must check `cachedReceiverObject`
before taking its `super` address. A cold cache has no receiver object; forming
the member address from null is undefined behavior even if the following
comparison would reject the cache entry.

The guard makes an empty cache take the existing lookup path. It does not alter
the language's direct-null access error or turn missing members into ownership
operations. Cached reads/writes and receiver changes retain normal behavior.

Clang14 UBSAN initially stopped the Shared/Weak runner at dispatch line1891
with a null `SZrObject` member access, then exposed the same missing guard in
the profiled inline implementation at execution_internal.h572. All four inline
read/write variants and both dispatcher variants now check the cached object
before the conversion. The focused script exercises a cold
write/read, a same-receiver hit, and a changed receiver at the same access site.
The frozen r7 replay passes Shared/Weak76/76 on GCC11, Clang14 and MSVC, and
member-access108/108 under Clang14 ASAN/UBSAN. The Shared/Weak ASAN/UBSAN replay
also passes76/76 under GDB (leak detection disabled under ptrace); a separate
LSan run exposes the existing IO-source free stub and remains an open gate.
