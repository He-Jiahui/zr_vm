---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
tests:
  - tests/CMakeLists.txt
  - tests/cmake/ssa-hotpatch-apply-sources.cmake
  - tests/library/test_ssa_capability_validation.c
  - tests/core/test_ssa_generation_publication.c
  - tests/library/test_ssa_rollback_restricted.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
doc_type: module-detail
status: partial
---

# Hotpatch capability validation contract

Hotpatch admission is a closed-world intersection:

    required = manifest.requiredCapabilities | every requirement.requiredBits
    admissible = (required & ~hostAllowedCapabilities) == 0

The capability closure is computed before generation preparation. Every
requirement must have a non-zero token and bit set, a zero reserved field, and
must fit the bounded requirement count. Unknown manifest flags, missing rows,
and capability escalation are reported with a structured diagnostic; callers
must not turn them into a generic false/success result.

ZrCore_HotPatch_ValidateCapabilityClosure delegates identity and signature
checks to the manifest validator, then uses the transactional closure seam in
hotpatch_capability.c. The output mask is written only after all rows pass.
Repeated requirements are harmless (bitwise union), while a single
out-of-policy bit rejects the complete patch.

The host must separately validate artifact schema, ABI/profile identity,
immutable content, and machine-code policy. Capability admission does not
authorize relocation or native imports by itself. iOS/WASM restricted profiles
therefore remain interpreter-only and reject relocation sections before
execution.

Validation snapshots the `patchId` and `publicContractHash` values consumed by
Apply and generation preparation, alongside the existing content hash,
capability mask, policy hash, and target profile. Mutating the caller-owned
manifest or requirement rows after validation therefore cannot change the
registry identity or generation contract recorded by these paths. The validated
token still borrows its artifact and manifest pointers; it does not copy or pin
artifact bytes. The current generation manager records identity metadata and
does not install executable bytes, so callers must preserve byte immutability
for any later artifact consumer.

Focused CTest coverage is provided by ssa_capability_validation and
ssa_rollback_restricted. The tests exercise a valid closure, escalation,
unknown flags, signature/content failures, idempotent application, fresh
generation rollback, and restricted-section rejection. Availability is
explicit: an unsupported capability or backend is never reported as accepted.
