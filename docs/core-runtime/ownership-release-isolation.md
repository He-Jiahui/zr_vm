---
related_code:
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/ownership.c
plan_sources:
  - docs/plans/astra/syntax/ownership-object-member-separation.md
  - docs/plans/astra/using/review.md
tests:
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_ownership_release_domain_cases.h
doc_type: module-detail
---

# Release Isolation Domain

Shared and Weak handles belong to the state domain recorded by their control
block. `ZrCore_Ownership_ReleaseValue` must validate this identity before clearing
the caller's value storage. A foreign-domain call is rejected without changing
the kind, object/control identity, or reference counts. Its void signature is
unchanged. The caller can still release that same handle in its origin domain.

Previously the wrapper cleared its storage before lower-level ReleaseStrong or
ReleaseWeak rejected the domain mismatch. This lost the only available handle
without decrementing its count. Astra Using finding U-F2 reproduces that ordering
error. The fix moves the existing domain predicate ahead of either storage reset;
same-domain final strong release and resource Drop retain their existing contract.

The two regressions exercise Shared identity/count preservation and Weak
identity/count preservation both before and after final strong release. An alive
control block includes its implicit weak reference. Origin-domain release is
asserted after each rejected call.

Fresh GCC 11.4, Clang 14, and MSVC 19.44 static Debug executions passed these
exact two cases on the Astra overlay. The broader lifecycle/parity acceptance
is tracked separately and is not claimed complete by this domain fix.
