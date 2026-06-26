# 2026-06-24 AOT 12-S2E export callable roots

## Status

12-S2E is complete as an exported-callable-root preservation sub-slice for `docs/plans/aot/12-code-stripping.md`.

## Completed

- Changed `backend_aot_compute_static_callable_reachability()` so callers provide root and root-reason buffers.
- Kept entry flat index 0 as `ROOT_ENTRY`.
- Added `SZrFunctionTopLevelCallableBinding` scanning for exported callable children.
- Marked exported callable children as `ROOT_EXPORT`, deduplicating root entries before BFS.
- Updated opt-in C code stripping to allocate and pass root buffers.
- Extended `zr_vm_aot_reachability_test` with an exported unused child that remains reachable.
- Extended `zr_vm_aot_c_code_stripping_test` with a generated-C case where an exported otherwise-unused child is preserved.

## RED/GREEN

- RED: the updated reachability test failed to compile because the graph helper still owned the fixed entry-only root list and did not accept caller root buffers.
- GREEN: exported callable bindings are retained as roots, while ordinary unused children remain trim candidates.

## Verification

- `cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test zr_vm_aot_c_code_stripping_test -j2` => built
- `zr_vm_aot_reachability_test` => 5 tests, 0 failures
- `zr_vm_aot_c_code_stripping_test` => 2 tests, 0 failures
- `ctest --test-dir build-wsl-gcc -R "aot_c_code_stripping|aot_reachability" --output-on-failure` => 2/2 passed
- `zr_vm_aot_c_source_contracts_test` => 19 tests, 0 failures
- `zr_vm_aot_c_shared_library_smoke_test` => 8 tests, 0 failures

## Remaining

This does not close full 12-S2. Manifest preservation roots, default writer enablement,
trim diagnostics, and size statistics remain open.
