# AOT M1.5 07-S5 CallStackValue u64/f64 local sync

## Status

- Completed: 2026-06-22 03:23:56 +08:00
- Slice: M1.5 / 07-S5 CallStackValue u64/f64 scalar-local sync
- State: sub-slice complete; 07-S5 and M1.5/07 remain partially complete; 08-12 not started.

## Scope

- Generic direct and dynamic `CallStackValue` lowering now receives the current `SZrAotExecIrFunction`.
- The shared `CallStackValue` core-call helper emits scalar-local restoration after `ZrLibrary_AotRuntime_CallStackValue(...)` for i64, bool, u64, and f64 destination locals.
- Generated C emits direct and dynamic `zr_aot_direct*_function_call_sync_u64_local_boundary` / `zr_aot_direct*_function_call_sync_f64_local_boundary` markers with `ZrLibrary_AotRuntime_SyncUnsignedIntLocal()` and `ZrLibrary_AotRuntime_SyncFloatLocal()` guards.
- Generated C continues to reject the old `zr_aot_direct_call_result` payload-read template for these restored locals.

## Evidence

- RED: call contracts failed before implementation because generic direct and dynamic `CallStackValue` lowering did not expose u64/f64 local-sync markers or helper calls.
- Debug finding: attempted runtime smokes with typed `fn()` return locals failed at source compile because unresolved callable return values are currently typed as `object`.
- Scope decision: callable-return typing is a language/front-end boundary and was not expanded in this AOT codegen slice.
- GREEN: focused call contracts 4/0 and call shared-library smoke 5/0.
- Broad focused group passed: call contracts 4/0, call shared-library smoke 5/0, shared-library smoke 8/0, value-type shared-library smoke 1/0, power contracts 2/0, power smoke 1/0, source contracts 19/0, generic numeric contracts/smoke 1/0 + 1/0, global smoke 9/0, logical contracts/smoke 4/0 + 4/0, typed scalar 1/0, return contracts 1/0, frame setup contracts 1/0.

## Generated C Checks

- Call contracts lock u64/f64 sync markers and helper calls for both direct and dynamic `CallStackValue` emission.
- The executable shared-library smoke covers a currently expressible StackValue local-assignment path through an indirect `fn(value)` call and returns `7`.
- The generated C contains `ZrLibrary_AotRuntime_CallStackValue(state, ...)` and rejects the old direct-call result payload-read local.

## Follow-up

- Add source-level executable u64/f64 dynamic callable-return smoke after callable return type inference can preserve typed return values instead of `object`.
