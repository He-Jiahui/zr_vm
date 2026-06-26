# 2026-06-24 AOT 12-S2D opt-in code stripping emitter

## Status

12-S2D is complete as an opt-in AOT C emitter integration sub-slice for `docs/plans/aot/12-code-stripping.md`.

## Completed

- Added `SZrAotWriterOptions.enableCodeStripping`.
- Added `backend_aot_option_enable_code_stripping()`.
- Integrated the 12-S2C static callable reachability graph into `ZrParser_Writer_WriteAotCFileWithOptions()` behind the opt-in flag.
- Kept the original function index space while filtering the emitted function entries.
- Added `zr_vm_aot_c_code_stripping_test` and CTest `aot_c_code_stripping`.
- Verified generated C omits an unreachable `zr_aot_fn_2` while preserving `ZR_NULL` holes in thunk and MethodInfo tables.

## RED/GREEN

- RED: the new generated-C test failed to compile because `SZrAotWriterOptions` had no `enableCodeStripping` member.
- GREEN: opt-in code stripping filters the unreachable static callable, keeps reachable root/child functions, and preserves sparse descriptor tables.

## Verification

- `cmake -S . -B build-wsl-gcc` => configured
- `cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2` => built
- `zr_vm_aot_c_code_stripping_test` => 1 test, 0 failures
- `zr_vm_aot_reachability_test` => 4 tests, 0 failures
- `zr_vm_aot_c_source_contracts_test` => 19 tests, 0 failures
- `zr_vm_aot_c_shared_library_smoke_test` => 8 tests, 0 failures
- `ctest --test-dir build-wsl-gcc -R "aot_c_code_stripping|aot_reachability" --output-on-failure` => 2/2 passed

## Remaining

This does not close full 12-S2. The default writer path still keeps full generation until export roots,
manifest preservation, trim diagnostics, and size statistics are implemented.
