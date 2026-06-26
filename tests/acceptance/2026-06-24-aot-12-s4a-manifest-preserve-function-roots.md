# 2026-06-24 AOT 12-S4A manifest preserve function roots

## Status

12-S4A is complete as the first manifest-preservation input channel for `docs/plans/aot/12-code-stripping.md`.

## Completed

- Added `SZrAotWriterOptions.manifestPreserveFunctionFlatIndices`.
- Added `SZrAotWriterOptions.manifestPreserveFunctionFlatIndexCount`.
- Extended `backend_aot_compute_static_callable_reachability()` with parsed manifest function roots.
- Marked manifest-preserved function roots with `ZR_AOT_REACHABILITY_REASON_MANIFEST`.
- Rejected invalid manifest roots instead of silently trimming or preserving the wrong function.
- Passed the parsed manifest root channel through opt-in AOT C code stripping.
- Added focused reachability coverage for a manifest-preserved otherwise-unused function.
- Added generated-C coverage proving a manifest-preserved otherwise-unused function is still emitted.

## RED/GREEN

- RED: the reachability test switched to the manifest-root helper shape while the graph helper still accepted only entry/export roots.
- GREEN: manifest roots enter the BFS with `MANIFEST` reason, invalid manifest roots are rejected, and generated C preserves the requested function.

## Verification

- `cmake --build build-wsl-gcc --target zr_vm_aot_reachability_test zr_vm_aot_c_code_stripping_test -j2` => built
- `zr_vm_aot_reachability_test` => 6 tests, 0 failures
- `zr_vm_aot_c_code_stripping_test` => 3 tests, 0 failures
- `ctest --test-dir build-wsl-gcc -R "aot_c_code_stripping|aot_reachability" --output-on-failure` => 2/2 passed
- `zr_vm_aot_c_source_contracts_test` => 19 tests, 0 failures
- `zr_vm_aot_c_shared_library_smoke_test` => 8 tests, 0 failures

## Remaining

This does not close full 12-S4. zrp manifest parsing, preserve-by-symbol/token rules,
annotation roots, feature switch evaluation, and trim diagnostics remain open.
