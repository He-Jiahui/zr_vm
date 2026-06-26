# 2026-06-24 AOT 12-S2C static callable reachability graph

## Status

12-S2C is complete as a graph-input sub-slice for `docs/plans/aot/12-code-stripping.md`.

## Completed

- Added `backend_aot_reachability_function_graph.{h,c}`.
- Added `backend_aot_compute_static_callable_reachability()`.
- Rooted the graph at the entry function flat index 0.
- Scanned real AOT bytecode instructions for static callable materialization edges: `GET_CONSTANT`, `CREATE_CLOSURE`, and `GET_SUB_FUNCTION`.
- Reused the 12-S1A BFS engine to produce `SZrAotReachabilityMark` results from the collected edges.
- Added a focused `GET_SUB_FUNCTION` graph test that marks the referenced child function and leaves an unused table entry unmarked.

## RED/GREEN

- RED: `zr_vm_aot_reachability_test` failed to compile because `backend_aot_reachability_function_graph.h` did not exist.
- GREEN: the static callable graph marks root + child, records a `DIRECT_CALL` edge, rejects insufficient edge capacity, and keeps the unused function unmarked.

## Verification

- `cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test -j2` => built
- `zr_vm_aot_reachability_test` => 4 tests, 0 failures
- `ctest --test-dir build-wsl-gcc -R "aot_reachability" --output-on-failure` => 1/1 passed
- `zr_vm_aot_c_source_contracts_test` => 19 tests, 0 failures

## Remaining

This does not close full 12-S2. Default AOT C filtering remains disabled until root/export/manifest preservation, dead-function emission proof, and size statistics are in place.
