---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/hotpatch_capability.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
tests:
  - tests/library/test_ssa_capability_validation.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
doc_type: acceptance-record
---

# SSA 08.02 Hot-patch requirement count limit

## Scope

This slice aligns the main `ZrCore_HotPatch_Validate` requirement-count
preflight with the existing capability-closure maximum. It does not claim to
complete 08.02 signature-copy/TOCTOU protection, full call-graph capability
analysis, or atomic publish isolation.

## RED

The current-source focused target built, then the direct executable returned
exit code 1 before the production change. With `requirementCount=4097` and only
one actual requirement slot, the old validator reached the deliberately
rejecting signature callback and returned signature rejection instead of a
limit result. Five checks failed: status, callback count, diagnostic status,
diagnostic expected, and diagnostic actual. The callback rejected before the
old code could enter its requirement loop, so the single-slot fixture did not
read beyond its storage. The complete 4096-entry boundary already passed.

## Implemented behavior

The main validator now checks the shared
`ZR_HOT_PATCH_CAPABILITY_MAX_REQUIREMENTS` after validating its top-level
input/output pointers and before signature verification or requirement
iteration. Counts above 4096 return `ZR_HOT_PATCH_LIMIT` with `expected=4096`
and `actual=requirementCount`; the output token is zeroed. Exactly 4096 entries
remain admissible when the entries and other validation inputs are valid. The
status name is `limit`.

The fixture uses an always-evaluated `TEST_CHECK` helper rather than placing
calls with side effects inside standard `assert`. For the rejection case it
asserts the callback count stays zero, exact structured diagnostics, and each
field of the cleared validated output. A full 4096-entry array exercises the
inclusive boundary.

## Verification

Compiled artifacts were written only to
`/mnt/d/tmp/zr_vm/close-proxy-core-red`.

```bash
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_capability_validation_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_capability_validation_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R ssa_capability_validation --output-on-failure --no-tests=error
```

- RED: focused target build exit 0; direct executable exit 1 with the five
  over-limit failures listed above.
- GCC GREEN: focused target build and direct executable exit 0; registered
  `ssa_capability_validation` CTest passed 1/1.
- NDEBUG: the test translation unit was separately compiled with `-DNDEBUG`,
  linked against the current D-cache production objects, and its direct
  executable exited 0. The checks and all validation calls still execute.
- MSVC: independent current-source target build and direct executable exited
  0; registered `ssa_capability_validation` CTest passed 1/1.

## Residual scope

The validated token still borrows artifact and manifest storage. This change
does not make that storage immutable or close a validation-to-apply TOCTOU
window. Recursive/call-graph capability derivation and publication rollback
remain open 08.02 gates.
