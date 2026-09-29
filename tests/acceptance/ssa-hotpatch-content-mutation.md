---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - tests/cmake/ssa-hotpatch-apply-sources.cmake
tests:
  - tests/library/test_ssa_capability_validation.c
  - tests/library/test_ssa_rollback_restricted.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
doc_type: acceptance-record
---

# SSA 08.02 Apply-time content mutation check

## Scope

This slice detects content bytes that remain changed between successful
validation and `ApplyValidated`. It also tests a successful verifier callback
that persistently changes the borrowed bytes before `Validate` returns. The
validated token captures the original byte address and length; Apply hashes
that captured span before reading the registry or preparing a generation.

The tests keep the original byte array alive through Apply. They change the
artifact view's `buffer` to null and its `bufferLength` to the maximum `UInt32`
after validation, proving the recheck uses the captured address and length and
cannot overread based on a later descriptor edit.

This does not copy or pin bytes, make concurrent writes atomic, or install
executable artifact bytes. Callers must preserve the captured storage through
Apply and prevent concurrent writes while it hashes that storage.

## RED

The focused target built before the Apply-time check. The direct executable
exited 1 with 22 failed checks: 11 each for post-validation mutation and
verifier-callback mutation. In both cases the old Apply returned success rather
than `ZR_HOT_PATCH_APPLY_CONTENT_MISMATCH`; the actual-hash diagnostic did not
match the mutated bytes, and registry, generation record, manager count, and
output handle showed that publication had occurred.

The unchanged `expectedHash` and original patch ID assertions passed, showing
the old path published the validation-time identity without rechecking its
borrowed byte span.

## Implemented behavior

`Validate` captures the byte pointer and length before hashing and invoking the
signature callback. `ApplyValidated` hashes exactly that captured span before
registry lookup. A mismatch returns `ZR_HOT_PATCH_APPLY_CONTENT_MISMATCH` with
`expectedHash` equal to the validated hash, `actualHash` equal to the current
span hash, the captured patch ID, and generation zero. No registry entry or
prepared generation is created. The new status name is `content-mismatch`.

The capability test exercises both mutation timings and verifies no registry
or generation state changed. The restricted rollback fixture now uses actual
byte hashes so the existing ID-collision test remains meaningful under the
new Apply check.

## Verification

All generated build files and binaries were kept in
`/mnt/d/tmp/zr_vm/close-proxy-core-red`.

```bash
cmake -S /mnt/e/Git/zr_vm -B /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -DCMAKE_SUPPRESS_REGENERATION=ON
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_capability_validation_test \
           zr_vm_ssa_generation_publication_test \
           zr_vm_ssa_rollback_restricted_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_capability_validation_test
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_generation_publication_test
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_rollback_restricted_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^(ssa_capability_validation|ssa_generation_publication|ssa_rollback_restricted)$' \
  --output-on-failure --no-tests=error
```

- RED: focused target build exit 0; direct executable exit 1 with the 22
  mutation/publication failures described above.
- GCC GREEN: merged build exit 0; all three direct executables exited 0; the
  registered hotpatch CTests passed 3/3.
- MSVC: independent builds of the three focused hotpatch targets and
  `zr_vm_core_static` exited 0; the capability-validation,
  generation-publication, and restricted-rollback CTests passed 3/3.

## Residual scope

The check detects changes still present when Apply hashes the captured span.
It does not make the read atomic against a concurrent writer, protect bytes
after Apply returns, or detect a trusted callback that changes and restores
bytes before Apply. The callback test covers a persistent mutation. Owned byte
storage and actual installation remain separate 08.02 work.
