# AOT 07-S2/S4 Generic LOGICAL_NOT Null Constant Local Branch

- Completed at: 2026-07-04 03:49:20 +08:00
- Status: completed focused sub-slice; broader AOT 07~12 work remains active.
- Scope: immediate `GET_CONSTANT null -> LOGICAL_NOT -> JUMP_IF_BOOL_FALSE` now folds to a local bool branch without writing the source null into `frame.slotBase`.
- Production changes: added `backend_aot_c_null_constant_consumed_by_local_logical_not()` and used it from constant emission, frame-descriptor local-only proof, and generic logical lowering. The optimized generated C emits `zr_aot_b1 = ZR_TRUE;` with null-constant/source-skip markers.
- Test changes: `test_aot_c_generic_logical_not_numeric_local_smoke.c` now has separate reset-null runtime fallback and null-constant local-only cases. `test_aot_c_logical_contracts.c` locks the new helper and marker while replacing stale source-kind contract text with the current destination-consumer proof.
- RED: null-constant shared-library smoke failed on missing `zr_aot_generic_logical_not_null_constant_local`.
- GREEN: WSL GCC and WSL Clang passed generic LOGICAL_NOT smoke 3/0, logical shared-library 6/0, generic JUMP_IF 3/0, generic equality 4/0, and logical contracts 4/0. Windows MSVC Debug passed the focused target with 0 failures / 3 expected Unix-only ignores and logical contracts 4/0.
