---
related_code:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
tests:
  - tests/library/test_ssa_capability_validation.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/03-generation-publication.md
doc_type: acceptance
status: verified
---

# Hot-patch Prepare failure status mapping

## Scope

This slice preserves the failure category returned by
`ZrCore_HotPatch_Generation_Prepare` when called through
`ZrCore_HotPatch_ApplyValidated`. It covers invalid module identity, exhausted
generation-manager storage, and generation-number overflow. It does not change
Rollback mapping, generation publication semantics, or artifact installation.

## Regression evidence

The test first creates a valid `SZrValidatedHotPatch` through the real
capability-validation API and confirms the content hash remains unchanged. It
then exercises:

- `moduleHash == 0`, expecting `APPLY_INVALID_ARGUMENT` / `invalid-argument`;
- `manager.nextGeneration == UINT64_MAX`, expecting the appended
  `APPLY_GENERATION_OVERFLOW` / `generation-overflow`;
- a full generation-manager record array with registry space remaining,
  expecting the existing `APPLY_CAPACITY` / `capacity` status.

Each case checks the structured Apply diagnostic and confirms that the registry,
manager records/count/next-generation/active pointer, and returned handle are
unchanged. Before the production mapping fix, the first two cases each failed
the status, status-name, and diagnostic-status checks: six failed assertions in
total. The true manager-capacity case already passed and remains mapped to
`APPLY_CAPACITY`.

## Verification

All build output stayed in `D:/tmp/zr_vm/close-proxy-core-red`.

```text
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target \
  zr_vm_ssa_capability_validation_test \
  zr_vm_ssa_generation_publication_test \
  zr_vm_ssa_rollback_restricted_test -j 4
```

Build exit code: 0.

Direct GCC test binaries all exited 0:

- `zr_vm_ssa_capability_validation_test`
- `zr_vm_ssa_generation_publication_test`
- `zr_vm_ssa_rollback_restricted_test`

Registered CTest command:

```text
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^(ssa_capability_validation|ssa_generation_publication|ssa_rollback_restricted)$' \
  --output-on-failure --no-tests=error
```

Result: 3/3 passed. The old Apply enum values retain their numbers; the new
generation-overflow value is appended. `Generation_Prepare` currently returns
only the three covered failure categories. An unexpected future Prepare status
fails closed as `APPLY_PUBLISH_FAILED`, never as capacity.
