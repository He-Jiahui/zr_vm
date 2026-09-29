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
  - tests/library/test_ssa_rollback_restricted.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/04-rollback-restricted.md
doc_type: acceptance
status: verified
---

# Hot-patch rollback status mapping

## Scope

This slice preserves the failure category returned by
`ZrCore_HotPatch_Generation_Rollback` through `ZrCore_HotPatch_Rollback`. It
does not alter rollback publication, generation ownership, or the separate
failure mapping after `Generation_Publish`.

## Regression fixture

The fixture uses the real generation manager APIs to Prepare and Publish two
records. Generation 1 remains retained as `RETIRED`; generation 2 is active.
It checks four public API outcomes:

- target `0` returns `APPLY_INVALID_ARGUMENT`;
- missing target `999` returns `APPLY_ROLLBACK_NOT_FOUND`;
- a full two-record manager with retained target 1 returns `APPLY_CAPACITY`;
- a three-record manager with retained target 1, one free slot, and
  `nextGeneration == UINT64_MAX` returns `APPLY_GENERATION_OVERFLOW`.

Each failure checks the status name and structured diagnostic, and verifies the
records (including lease counts), manager count, next-generation value, active
pointer, and returned handle remain unchanged. Before the mapping fix, the
capacity and overflow scenarios each failed only the status, status-name, and
diagnostic-status checks: six failed assertions total. Missing target and
invalid target already returned their expected statuses.

## Verification

All build output stayed in `D:/tmp/zr_vm/close-proxy-core-red`.

```text
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target \
  zr_vm_ssa_rollback_restricted_test \
  zr_vm_ssa_generation_publication_test \
  zr_vm_ssa_capability_validation_test -j 4
```

Build exit code: 0. All three direct GCC binaries exited 0:

- `zr_vm_ssa_rollback_restricted_test`
- `zr_vm_ssa_generation_publication_test`
- `zr_vm_ssa_capability_validation_test`

Registered CTest command:

```text
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^(ssa_rollback_restricted|ssa_generation_publication|ssa_capability_validation)$' \
  --output-on-failure --no-tests=error
```

Result: 3/3 passed. The mapping reuses existing `APPLY_CAPACITY` and
`APPLY_GENERATION_OVERFLOW` values; no public enum value was added for this
slice. Unknown generation failures map to `APPLY_ROLLBACK_FAILED`.
