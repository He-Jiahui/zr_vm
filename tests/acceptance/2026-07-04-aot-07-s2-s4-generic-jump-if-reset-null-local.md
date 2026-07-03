# AOT 07-S2/S4 Generic JUMP_IF Reset Null Local Branch

- Completed at: 2026-07-04 04:28:50 +08:00
- Status: completed focused sub-slice; broader AOT 07~12 work remains active.
- Scope: immediate `RESET_STACK_NULL -> JUMP_IF` now folds to a direct false branch without calling the runtime reset helper for the source slot.
- Production changes: added `backend_aot_c_reset_null_consumed_by_local_jump_if()` and used it from reset emission, frame-descriptor local-only proof, and generic jump lowering. The optimized generated C emits `zr_aot_reset_null_local_jump_if_source_skip`, `zr_aot_generic_jump_if_reset_null_false`, and a direct `goto` to the known false target.
- Test changes: `test_aot_c_generic_jump_if_bool_local_smoke.c` now includes a reset-null local-only case that executes the false branch and locks the generated C markers. `test_aot_c_logical_contracts.c` locks the new helper and marker.
- RED: reset-null `JUMP_IF` shared-library smoke failed on missing `zr_aot_reset_null_local_jump_if_source_skip`.
- GREEN: WSL GCC and WSL Clang passed generic JUMP_IF smoke 5/0, generic LOGICAL_NOT smoke 3/0, logical shared-library 6/0, generic equality 4/0, logical contracts 4/0, frame setup contracts 1/0, and control contracts 2/0. Windows MSVC Debug passed the focused target with 0 failures / 5 expected Unix-only ignores and logical/frame/control contracts 4/0, 1/0, and 2/0.
