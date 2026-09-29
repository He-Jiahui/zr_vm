---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/hotpatch_capability.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
tests:
  - tests/library/test_ssa_capability_validation.c
  - tests/acceptance/ssa-hotpatch-requirement-limit.md
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
doc_type: core-runtime-contract
status: implemented
---

# Hot-patch capability validation

`capability_manifest.h` defines the host-authoritative validation boundary for
patches. A manifest is bound to a patch id, immutable artifact content hash,
base module hash, public contract hash, ABI version, and target profile.

Validation first checks the already parsed pointer-free artifact, content/base
identity, ABI/profile and public-contract invariants. Machine-code payloads,
new native imports, and public layout/signature changes are rejected before any
publish operation. Required capabilities are checked both as an aggregate and
per token requirement; `required - hostAllowed` is an explicit escalation
error, never silently trimmed.

Both the main `ZrCore_HotPatch_Validate` entry point and the standalone
capability-closure entry point use
`ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS` (4096). Validate rejects a larger
count with `ZR_HOT_PATCH_LIMIT` before invoking the host signature callback or
iterating requirement entries. The diagnostic sets `expected` to the maximum
and `actual` to the requested count; the validated output remains zero. A
complete manifest with exactly 4096 requirements is accepted when its ordinary
identity, signature, and capability checks pass. `StatusName` reports this
status as `limit`.

Signature verification is supplied by the host through a callback, keeping
trust-root ownership outside the VM. The validated result is written only
after every check succeeds and records the policy hash, content identity, and
an immutable-content marker for the later generation publisher. Callers must
retain or copy the artifact buffer so that this identity cannot be changed
between validation and apply.
