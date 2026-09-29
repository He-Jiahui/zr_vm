---
related_code:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - tests/container/test_container_runtime.c
plan_sources:
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
tests:
  - tests/container/test_container_runtime.c
doc_type: acceptance-record
status: accepted
---

# SSA 05.02: writable Array capacity normalization

## Scope

`Array<T>.capacity` is a writable public integer. Before checking whether an
Array mutation has enough capacity, the runtime must normalize a nonpositive
value to the initial capacity of four and persist that normalized value. The
conversion to unsigned `TZrSize` must happen only after normalization. This
slice covers nonpositive capacity; positive-capacity growth still needs a
separate upper-bound check before signed doubling.

## Regression

`test_container_array_runtime_normalizes_negative_capacity_before_append`
creates an empty `Array<int>`, sets `capacity` to `-1`, appends `7`, and returns
`capacity * 100 + length * 10 + xs[0]`. The expected value is `417`, which
asserts normalized capacity `4`, length `1`, and the appended value `7`.

Before the helper fix, the current-source RED run exited 1 and reported
`Expected 417 Was -83`. The `-83` result showed that the negative capacity was
cast to an unsigned value and treated as already large enough, so the field
remained `-1`.

## Implementation

`zr_container_array_ensure_capacity` now replaces nonpositive capacity with
the initial capacity and stores it before the unsigned comparison. Capacity
growth and its existing doubling factor are otherwise unchanged.

## Verification

Validation used WSL GCC 11.4.0, CMake/CTest 3.22.1, and the D cache at
`/mnt/d/tmp/zr_vm/close-proxy-core-red`.

```sh
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_container_runtime_test -j 4
wsl.exe --exec env LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_container_runtime_test
wsl.exe --exec env LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red --output-on-failure -R '^containers$' --no-tests=error
```

The final GCC target build exited 0 after 13/13 actions. The direct Unity binary
reported 50 tests; the new capacity test passed, with three unrelated failures
in `test_container_linked_list_runtime_remove_first_preserves_pair_values_across_typed_function_boundary`,
`test_container_linked_set_map_runtime_preserves_native_call_arguments_in_fresh_state`,
and `test_reference_object_array_map_plain_index_fixture_keeps_contract_specific_miss_semantics`.
The registered `containers` CTest (1/1) failed on those same three cases.

A registration-only MSVC A/B run with the new test not registered reported 49
tests and exactly those same three failures. An independent MSVC run after the
fix reported 50 tests and the same three failures, with the new capacity test
passing. This isolates the container-suite failures from this change; the
aggregate suite is not fully green in the current tree.

## Acceptance Decision

Accepted for the writable nonpositive-capacity invariant. The focused regression
passes on GCC and MSVC, and the no-registration baseline confirms the three
aggregate failures are unchanged. Positive capacity overflow and the unrelated
container-suite failures remain outside this slice.
