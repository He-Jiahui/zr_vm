# 2026-06-24 AOT 12-S2B sparse AOT index space

## Status

12-S2B is complete as a support sub-slice for `docs/plans/aot/12-code-stripping.md`.

## Completed

- Added `SZrAotFunctionTable.indexSpace` to preserve the original function-index address space.
- Added `backend_aot_function_table_index_space()`.
- Kept reachability filtering compacting only the emitted entries, not the original index space.
- Changed generated C forward declarations to use `entry->flatIndex`.
- Changed `zr_aot_function_thunks[]` and `zr_aot_method_infos[]` to emit over `functionIndexSpace` and write `ZR_NULL` for holes.
- Changed compiled-module descriptor counts to publish `functionIndexSpace`.

## RED/GREEN

- RED: `zr_vm_aot_reachability_test` linked against a missing `backend_aot_function_table_index_space()` helper.
- RED: frame setup source contract required sparse emitter text before the emitter used `functionIndexSpace`.
- GREEN: filtered tables preserve index space, and emitter contracts cover sparse thunk/method-info emission.

## Verification

- `zr_vm_aot_reachability_test` => 3 tests, 0 failures
- `zr_vm_aot_c_frame_setup_contracts_test` => 1 test, 0 failures
- `zr_vm_aot_c_source_contracts_test` => 19 tests, 0 failures
- `zr_vm_aot_c_shared_library_smoke_test` => 8 tests, 0 failures
- `ctest --test-dir build-wsl-gcc -R "aot_reachability" --output-on-failure` => 1/1 passed

## Remaining

This does not close full 12-S2. The generated C path still needs real reachability graph input, default filtering, dead-function emission proof, and size statistics.
