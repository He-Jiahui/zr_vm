# 2026-06-24 AOT 12-S2A function table reachability filter

## Status

12-S2A is complete as a narrow sub-slice of `docs/plans/aot/12-code-stripping.md`.

## Completed

- Added `backend_aot_filter_function_table_by_reachability()` in `backend_aot_function_table.{h,c}`.
- The helper compacts an existing `SZrAotFunctionTable` in place using `SZrAotReachabilityMark` state.
- Reachable entries keep their original `flatIndex`; this preserves stable method/function numbering for later generated C integration.
- Invalid table state or a function entry whose `flatIndex` is outside the mark array is rejected.
- Extended `tests/parser/test_aot_reachability.c` with a focused function-table filtering contract.

## RED/GREEN

- RED: the new reachability test linked against a missing `backend_aot_filter_function_table_by_reachability` symbol.
- GREEN: the table `[0, 1, 2, 3]` filters to reachable entries `[0, 2]`, keeps `flatIndex` values `0` and `2`, and rejects a too-small mark array.

## Verification

- `cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test -j2`
- `./build-wsl-gcc/bin/zr_vm_aot_reachability_test` => 3 tests, 0 failures
- `ctest --test-dir build-wsl-gcc -R "aot_reachability" --output-on-failure` => 1/1 passed
- `./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test` => 19 tests, 0 failures

## Remaining

This does not close full 12-S2. The default AOT C emitter still needs to consume reachability results, prove dead functions are not emitted, and report measurable size reduction.
