---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
plan_sources:
  - docs/plans/astra/syntax/ownership-object-member-separation.md
  - docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md
tests:
  - tests/core/test_execution_member_access_fast_paths.c
  - tests/core/test_execution_member_access_ownership_cases.h
  - tests/parser/test_ownership_abrupt_parity_cases.h
doc_type: module-detail
---

# Aliased Member Result Lifetime

Cached member GET can overwrite its receiver slot. In that case it reads into
a stable local value so the lookup retains a usable receiver until completion.
If the field is Shared or Weak, that local value owns a reference. Copying it
again into the destination without releasing the local leaks one reference.

The private result installer transfers stable owner bits into the destination
and resets the temporary, then releases the detached previous receiver. This
also avoids accessing a destination stack pointer after receiver Drop can
reenter and relocate the stack. Non-owning values keep the existing copy path.
No ownership wrapper allocation, public API, or artifact representation changes.

The same installer covers cache miss, single exact-pair hit, and multislot PIC
hit. Miss-path cache refresh runs while the original receiver is still alive,
before installing the result and releasing that receiver.

Six core regressions check Shared and Weak reference counts on each path.
Weak counts include the implicit weak reference held while strong count is
positive. The end-to-end weak-chain parity additionally checks final Drop,
not only the ability to access the returned object.

RED was an end-to-end graph with seven Drops instead of eight. GDB traced an
extra retain into the C-local stable result with no matching release. After the
fix, the fresh MSVC static Debug core runner reports 108 Tests, 0 Failures,
0 Ignored; Shared/Weak including VM and binary parity reports 54/54. Frozen
GCC/Clang/sanitizer and overall acceptance are recorded separately.

This is a narrow repair in the existing cached-get responsibility. The large
member-access file is not reorganized as part of the ownership supplement.
