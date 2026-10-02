---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - docs/core-runtime/execution-state-maps.md
  - tests/cmake/ssa-tests.cmake
  - tests/parser/ssa_state_map_clone_fault_allocator.c
  - tests/parser/ssa_state_map_clone_fault_allocator.h
  - tests/parser/test_ssa_state_map_clone_fault_cases.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/parser/ssa_state_map_clone_fault_allocator.c
  - tests/parser/test_ssa_state_map_clone_fault_cases.h
doc_type: acceptance
---

# State-map clone allocation rollback

## Scope

This slice covers the clone boundary in SSA 01.04. `ZrCore_ExecIr_StateMapClone`
copies entries, live-value IDs, root IDs, and owner-state IDs into a temporary
map. A failed copy must free only that temporary storage and leave both the
source and an already populated destination readable and byte-for-byte
unchanged. The slice does not add physical frame switching or resumed
execution.

## Test boundary

The existing `zr_vm_ssa_state_maps_test` target substitutes
`ssa_state_map_clone_fault_allocator.c` for the ordinary storage translation
unit. That wrapper includes the unchanged production source with local
`malloc`/`free` interception, following the owner-analysis and deopt-aggregate
fault tests. The fault loop calls the public `ZrCore_ExecIr_StateMapClone`
entry. Other targets compile the ordinary storage source; no production API,
allocator setter, or production global state is added.

Each fixture has one logical item and one spare item in all four pools. The
snapshots own separate byte arrays for the entire map object and all capacity
bytes, so in-place writes cannot alter the expected result. Each injected call
captures its result, failure witness, allocation count, and outstanding count,
then disarms interception before assertions or cleanup.

The failure case rejects ordinals 1 through 4, corresponding to entries,
live-value IDs, root IDs, and owner-state IDs. It requires the requested failure
to occur, all successful temporary allocations to be released, and source and
pre-populated destination bytes to remain unchanged. A fifth ordinal reaches
the first successful call, checks compact destination capacities and independent
pools, and frees the source before reading the destination. Empty-map and
self-clone cases must succeed without any allocation.

## RED and GREEN evidence

The unchanged clone implementation is already transactional; this is a coverage
slice rather than a production defect fix. D-only mutation copies deliberately
remove rollback, modify the source pool at failure, or modify the destination
metadata at failure. Omitting rollback fails at the second pool's OOM with
`Expected 0 Was 1` outstanding allocation; the later cases also detect that
uncleared ledger. Modifying the source live-value ID reports `Byte 0 Expected
0x69 Was 0x6A`; modifying destination generation reports a map byte mismatch.
These mutation runs exit 4, 1, and 1 respectively. The unchanged production
source passes all four failure ordinals. No mutation is applied to the
repository's production source.

## Limitations

This evidence covers logical side-table rollback. It does not validate AOT
emission, physical frame layout, register restoration, or execution after a
resume. The source and destination byte checks cover clone storage, not semantic
checkpoint validation.

## Validation record

- WSL GCC 11.4.0 ASan/UBSan: **39 tests, 0 failures**, exit 0.
  `clone-final-gcc-{build,run}.log` records the build and run.
- WSL Clang 14.0.0 ASan/UBSan: **39 tests, 0 failures**, exit 0.
  `clone-final-clang-{build,run}.log` records the build and run.
- MSVC x64 (Visual Studio environment 17.14.40, C11 and `/utf-8`):
  **39 tests, 0 failures**, exit 0. `final-msvc-{build,run}.log` records the run.
- Mutation RED: `clone-red-{rollback,source,destination}-run.log` retains the
  expected failures. The mutation source copies and executables were deleted
  after collecting the evidence.

Both WSL builds use `-fsanitize=address,undefined -fno-omit-frame-pointer` and
`-no-pie`. Runs set `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`; neither run reports a sanitizer finding.

All compiler temporary directories, build outputs, command records, and logs are
under `D:/tmp/zr_vm/ssa-state-map-clone-oom`. Direct compiler command records use
the same focused source list as the registered CMake target. No shared build
tree is rebuilt by this slice.

Superseded prototype binaries, mutation binaries/sources, and MSVC intermediate
objects were cleaned after validation, releasing 10,033,994 bytes. The accepted
GCC/Clang/MSVC binaries and evidence occupy about 6 MiB and remain available for
the primary review. The focused CMake change is preserved separately in
`state-map-clone-cmake.patch` so the existing unrelated dirty CMake hunks can
remain outside this subtask's commit.
