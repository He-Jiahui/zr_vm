---
related_code:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
  - zr_vm_core/src/zr_vm_core/call/call_binding.c
  - tests/library/test_native_call_binding_status.c
  - tests/cmake/native-call-binding-status-tests.cmake
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c
plan_sources:
  - user: 2026-10-16 optional diagnostic status propagation regression
tests:
  - tests/library/test_native_call_binding_status.c
  - tests/cmake/native-call-binding-status-tests.cmake
doc_type: testing-guide
---

# Native call binding validation status

ResolveCallBinding currently discards CheckContract's precise return when its
optional diagnostic argument is NULL, substituting MISSING_CONTRACT. The fixture
requires the same status with and without diagnostics. It does not modify the
resolver, register providers, or claim a production fix or actual RED result.

The executable `zr_vm_native_call_binding_status_test` links only the actual
Library and Core static targets and creates/destroys a real GlobalState. It uses
no synthetic state, registry mock, copied implementation, or fault allocator.
The host allocator only forwards realloc/free; allocation tracking is not an
oracle for this pure status regression.

All cases begin with a DIRECT contract, MEMBER_DEF RID1 target, SIGNATURE RID1,
nonzero hashes, no owner/layout, operation CALL and dispatchSlot NONE. Each case
changes only the named field. Explicit expected enum constants, not resolver
output, are checked against the real public CheckContract both with and without
diagnostics before testing the public Library resolver.

| CTest suffix under `native_call_binding_status_` | Change | Expected |
| --- | --- | --- |
| invalid_token | signatureToken=0 | INVALID_TOKEN |
| invalid_slot | dispatchSlot=0 | INVALID_SLOT |
| reserved | reserved1=1 | INVALID_ARGUMENT |
| operation | operation=META+1 | INVALID_ARGUMENT |
| missing_contract | signatureHash=0 | MISSING_CONTRACT |
| no_registry_control | valid contract, no registry installed | TARGET_NOT_FOUND |

Every resolver call begins with poisoned target bytes and must clear the entire
target on failure, including all callable/generation/slot fields. Malformed
contracts must additionally fill the expected diagnostic status. The valid
no-registry control checks return equality and target clearing, but does not
require the currently unpopulated diagnostic.status on that separate branch.
NULL-contract early-argument diagnostics and provider resolution/reload are
outside this regression. Each case runs in its own process with a 20-second
CTest timeout. GlobalState is always destroyed after the status assertions.

The independent fragment is not registered by this delivery. Root owns shared
registration, native builds, actual RED/GREEN evidence and production changes.
The child manifest lives under E:/cargo-targets/zr_vm/reports/
review-bugfix-20261016/native-call-binding-status. No temporary helper, generated
fixture, build or source snapshot is created by this fixture-only task.
