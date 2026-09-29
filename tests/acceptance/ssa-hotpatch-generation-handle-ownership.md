---
related_code:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
tests:
  - tests/core/test_ssa_generation_publication.c
  - tests/acceptance/ssa-hotpatch-generation-handle-ownership.md
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/03-generation-publication.md
doc_type: acceptance-record
---

# SSA 08.03 generation handle ownership

## Scope

Generation handles may be used only with the manager whose records array owns
the referenced record. This slice checks that cross-manager `Publish`,
`Resolve`, and `Release` calls return structured errors without changing
either manager's active record or lease count. A generation number passed to
`Rollback` is looked up within its explicit manager; Rollback accepts no handle
and is not part of this pointer-ownership case.

The audit also checked whether direct `Generation_Prepare` bypasses the
08.02 Apply-time byte check. Its public precondition requires the validated
borrowed content to remain stable, and Prepare only records identity metadata;
it does not install artifact bytes. Mutating the bytes before direct Prepare
violates that precondition, so this slice does not add a duplicate hash check.

## Test inventory

The new test keeps two managers and both records arrays live. Each manager
allocates generation 1. It passes the owner's prepared and leased handles to
the other manager, then checks the returned status, diagnostics, record states,
manager counts, active pointers, and lease counts. It then confirms the owner
can still resolve and release its handle. `Rollback` is excluded because it
accepts only a manager-local generation number, not a record handle.

## Baseline and RED

The focused direct test exited 1 at two checks with the original
implementation: `Publish` and `Resolve` copied the foreign record's generation
into `diagnostic.actualGeneration` instead of returning zero.

An AddressSanitizer pointer-pair run confirmed the underlying undefined
comparison. The old implementation from commit
`09445f74015b5682a3090f3218332400283b84c6` reported
`AddressSanitizer: invalid-pointer-pair` in `gen_belongs`, called by the
cross-manager `Publish` test. The trace pointed at the `r >= m->records`
comparison between the two live arrays.

## Implemented behavior

Manager membership now scans the configured slots using pointer equality,
which is defined for records in separate arrays. `Publish` and `Resolve` split
the ownership check from generation/state reads. A foreign record is rejected
without reading it and reports `actualGeneration == 0`; the existing `Release`
failure path also preserves the foreign handle and owner lease.

The equality scan costs O(capacity). `GenerationManager_Init` accepts a
caller-specified `TZrUInt32` capacity and currently imposes no smaller limit.
Repository call sites for `Resolve` and `Release` are limited to generation
tests; interpreter frame entry/exit is not yet connected. This correctness
first implementation should be revisited if those APIs become frequent
runtime lookups.

## Verification

All build outputs were kept under `/mnt/d/tmp/zr_vm/close-proxy-core-red`.
Temporary copied source and sanitizer binaries were created there and removed
after the A/B run; the reusable build cache remains in place. GCC was
`11.4.0-1ubuntu1~22.04.3`.

```bash
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_generation_publication_test \
           zr_vm_ssa_capability_validation_test \
           zr_vm_ssa_rollback_restricted_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_generation_publication_test
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_capability_validation_test
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_rollback_restricted_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^(ssa_generation_publication|ssa_capability_validation|ssa_rollback_restricted)$' \
  --output-on-failure --no-tests=error
```

The merged GCC build and all three direct executables exited 0. The registered
generation-publication, capability-validation, and restricted-rollback CTests
passed 3/3.

The sanitizer A/B commands were:

```bash
cd /mnt/e/Git/zr_vm
git show 09445f74015b5682a3090f3218332400283b84c6:zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c \
  > /mnt/d/tmp/zr_vm/close-proxy-core-red/hotpatch_generation_before_handle_fix.c
gcc -std=c11 -g -fno-omit-frame-pointer -fsanitize=address,pointer-compare \
  -Izr_vm_core/include -Izr_vm_common/include \
  tests/core/test_ssa_generation_publication.c \
  /mnt/d/tmp/zr_vm/close-proxy-core-red/hotpatch_generation_before_handle_fix.c \
  zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c \
  zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c \
  -o /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/ssa_generation_publication_before_handle_fix_asan
ASAN_OPTIONS=detect_invalid_pointer_pairs=2 \
  /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/ssa_generation_publication_before_handle_fix_asan
gcc -std=c11 -g -fno-omit-frame-pointer -fsanitize=address,pointer-compare \
  -Izr_vm_core/include -Izr_vm_common/include \
  tests/core/test_ssa_generation_publication.c \
  zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c \
  zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c \
  zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c \
  -o /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/ssa_generation_publication_handle_fix_asan
ASAN_OPTIONS=detect_invalid_pointer_pairs=2 \
  /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/ssa_generation_publication_handle_fix_asan
```

The baseline sanitizer run exited 1 with `AddressSanitizer: invalid-pointer-pair`
at `gen_belongs`; the fixed-source run exited 0. Root independently built the
current-source MSVC `zr_vm_core_static` and all three focused targets, with
exit 0. The generation-publication, capability-validation, and
restricted-rollback CTests passed 3/3.

## Acceptance decision

The GCC manager-ownership slice passes its direct, sanitizer, and registered
CTest gates; root's independent MSVC build and CTests also pass. The API
rejects cross-manager handles before foreign record reads and preserves both
managers' records and lease counts.

## Remaining scope

This change does not add a slot-index handle or cap manager capacity. It does
not make `Resolve` safe to race with `Publish`; the record state remains
non-atomic and its concurrency issue is separately documented in the API. It
also does not integrate generation handles with interpreter frame entry,
artifact installation, or the full 08.03 old/new frame lifecycle.
