# AOT 07-S2/S4 Generic TO_BOOL Scalar-Source Local

- Time: 2026-06-28 13:44:34 +08:00
- Status: Completed focused slice; 07/M1.5 remains in progress.

## Scope

This slice removes the generic runtime/frame boundary for `<bool> truthSource` when the source slot is already proven to hold a scalar local.

`backend_aot_write_c_direct_to_bool()` now receives the exec instruction index and tries a local-only `backend_aot_write_c_scalar_to_bool()` path before falling back to `ZrLibrary_AotRuntime_ConvertGenericToBool`. The local-only path requires:

- a bool scalar destination
- `backend_aot_c_scalar_locals_bool_result_can_skip_value_slot()`
- a source slot already written as bool, i64, u64, or f64 before the conversion

Frame-descriptor proof and scalar-local consumers now include `TO_BOOL` as a scalar source consumer, so the int constant feeding `<bool> truthSource` can stay in `zr_aot_s1` without materializing `frame.slotBase[1].value`.

## RED

The focused generic primitive conversion smoke was changed to require:

- `zr_aot_scalar_exec_to_bool dstSlot=4 srcSlot=1`
- `zr_aot_b4 = (TZrBool)(zr_aot_s1 != (TZrInt64)0);`

and forbid:

- `ZrLibrary_AotRuntime_ConvertGenericToBool(state, &frame, 4, 1)`
- `frame.slotBase[1].value`
- `frame.slotBase[4].value`
- generic bool conversion local sync markers

The old generator failed because it emitted `ConvertGenericToBool(state, &frame, 4, 1)` and materialized the source/destination value slots.

## GREEN

- WSL GCC: source contracts 22/0, frame setup contracts 1/0, shared-library smoke 13/0.
- WSL Clang: source contracts 22/0, frame setup contracts 1/0, shared-library smoke 13/0.
- Windows MSVC Debug: source contracts 22/0, frame setup contracts 1/0, shared-library smoke 0 failures / 13 ignored Unix-only.

Generated C evidence from the GCC generic conversion fixture:

- `zr_aot_s1 = (TZrInt64)7;`
- `/* zr_aot_scalar_exec_to_bool dstSlot=4 srcSlot=1 */`
- `zr_aot_b4 = (TZrBool)(zr_aot_s1 != (TZrInt64)0);`
- no `frame.slotBase` references in the generated file
- no `ZrLibrary_AotRuntime_ConvertGenericToBool`

## Remaining Work

This does not complete 07-S2/S4. Broader generic/dynamic/string boundaries, callable/closure materialization, value-level stack-copy migration, GC roots/exports/frame cleanup, wider byte-frame narrowing, performance counters, and full typed function-body zero-frame proof remain later 07 work.
