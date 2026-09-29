---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
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
doc_type: acceptance-record
---

# SSA 08.02 Validated manifest snapshot

## Scope

This slice snapshots the manifest `patchId` and `publicContractHash` values
that Apply and generation preparation publish. The validation token continues
to carry the existing requirement mask, content hash, policy hash, and target
profile snapshots. The test mutates caller-owned requirement and manifest
storage after validation without freeing it, so it does not rely on a dangling
pointer or use-after-free.

This does not add owned artifact bytes or implement executable installation.
The generation manager currently records identity metadata only.

## RED

The focused target built against the old Apply/Prepare behavior. The direct
executable exited 1 with exactly two failures:

- the registry received the mutated `patchId` 9 instead of the validated value
  1;
- the active generation received the mutated `publicContractHash` 456 instead
  of the validated value 123.

The validated capability mask and content hash remained unchanged, as expected
from their existing snapshots.

## Implemented behavior

`SZrValidatedHotPatch` now stores `patchId` and `publicContractHash` captured
only after validation succeeds. `ApplyValidated` uses the captured patch ID for
registry lookup/publication, and `Generation_Prepare` records the captured
contract hash. Both reject a token missing either required identity scalar.
Manually constructed test tokens initialize the new fields explicitly.

The capability-validation target links the production generation and rollback
sources through `tests/cmake/ssa-hotpatch-apply-sources.cmake`, keeping the
existing large SSA test include untouched.

## Verification

All generated build files and binaries were kept in
`/mnt/d/tmp/zr_vm/close-proxy-core-red`.

```bash
cmake -S /mnt/e/Git/zr_vm -B /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -DCMAKE_SUPPRESS_REGENERATION=ON
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_capability_validation_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_capability_validation_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^ssa_capability_validation$' --output-on-failure --no-tests=error
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_generation_publication_test \
           zr_vm_ssa_rollback_restricted_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_generation_publication_test
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_rollback_restricted_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^(ssa_capability_validation|ssa_generation_publication|ssa_rollback_restricted)$' \
  --output-on-failure --no-tests=error
```

- RED: focused target build exit 0; direct executable exit 1 with the two
  publication mismatches above.
- GCC GREEN: focused target/direct executable exit 0; focused CTest passed 1/1.
- Adjacent direct generation-publication and restricted-rollback executables
  both exited 0; the three registered hotpatch CTests passed 3/3.
- MSVC: root independently built the focused and adjacent targets in
  `D:/tmp/zr_vm/ssa-artifact-v6-msvc`; the capability executable exited 0,
  and the three registered CTests passed 3/3.

## Residual scope

`SZrValidatedHotPatch` still borrows the artifact and manifest pointers. Apply
and generation preparation no longer reread the captured manifest identity
fields, but the token does not copy or pin artifact bytes. The current
generation manager stores hashes and contract metadata; it does not install
artifact bytes. Byte-buffer ownership, later consumers, full capability
derivation, and atomic publication remain separate 08.02 gates.
