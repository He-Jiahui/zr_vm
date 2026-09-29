---
related_code:
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
implementation_files:
  - tests/parser/test_span_core.c
plan_sources:
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
tests:
  - tests/parser/test_span_core.c
doc_type: acceptance-record
status: accepted
---

# SSA 05.02: Array-backed Span across backing growth

## Scope

This characterizes the existing `zr.container.Array<int>` and `Span<int>`
logical-view behavior. The test records the Array's public capacity as four,
creates a Span over four items, then appends a fifth item and checks that the
public capacity is eight. The source sequence forces the current raw-int append
path to request capacity five from the capacity-four backing. The Span must keep
length four and read all four original values; the Array must read the new value
at index four.

The view resolves reads through its source Array and retains its creation-time
window. This test does not change Array mutation or loan behavior. It also does
not inspect the raw backing pointer, so its direct assertion is the public
logical capacity transition and the resulting values. Current raw-int source
uses `ZrCore_Object_SuperArrayEnsureRawIntCapacity` for the fifth element; with
required capacity five, the implementation doubles the backing from four to
eight and frees the previous buffer. This characterization does not claim that
the core `SZrContiguousView` descriptor is integrated with this runtime Array
adapter or that its storage-generation field tracks backing reallocation. The
broader 05.02 lifecycle and resize requirements remain open.

## Test inventory

`test_span_array_live_view_reads_prefix_after_backing_growth` in
`tests/parser/test_span_core.c` executes a source fixture with values 10, 20, 30,
and 40, records `initialCapacity`, creates the Span, then appends 50. Its base-100
encoded result asserts the initial/final capacities `4` and `8`, view length `4`,
view values `10`, `20`, `30`, and `40`, and `xs[4] == 50`. The expected result is
`4008041020304050`.

This is characterization coverage, so no RED is expected. No production source
is changed by this slice.

## Tooling Evidence

Validation used WSL GCC 11.4.0, CMake/CTest 3.22.1, and only the D-cache at
`/mnt/d/tmp/zr_vm/close-proxy-core-red`.

```sh
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_span_core_test -j 4
wsl.exe --exec env LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_span_core_test
wsl.exe --exec env LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_super_array_raw_int_canonical_storage_test
wsl.exe --exec env LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red --output-on-failure -R '^(parser_span_inline_receiver|super_array_raw_int_canonical_storage)$' --no-tests=error
```

The current-source Span target build exited 0 after 230/230 actions, including
the regenerated shared libraries. The full Span binary reported 25 tests and
zero failures, including the strengthened growth case. The raw-int
canonical-storage binary reported 7 tests and zero failures, including raw-int
storage across GC movement. CTest passed both `parser_span_inline_receiver` and
`super_array_raw_int_canonical_storage` (2/2); the Span selector is an adjacent
five-case subset and does not include the growth case.

## Results

The new source fixture passed on the current implementation without a
production change. The full Span suite also passed its existing compact-GC case
while a Span remains live. Expected compiler diagnostics from negative Span
tests are printed during the direct run; all corresponding Unity assertions
passed.

## Acceptance Decision

Accepted as characterization coverage for current Array-backed logical-view
behavior. The test establishes the public capacity transition and the old
view/new Array values. It does not directly assert the raw buffer address or
core descriptor generation. SSA 05.02 remains open for its wider arrays, views,
storage-generation, and lifetime requirements.
