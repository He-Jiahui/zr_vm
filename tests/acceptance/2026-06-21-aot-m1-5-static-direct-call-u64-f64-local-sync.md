# AOT M1.5 07-S5 static direct-call u64/f64 local sync

## Status

- Completed: 2026-06-22 03:03:12 +08:00
- Slice: M1.5 / 07-S5 static direct-call u64/f64 scalar-local sync
- State: sub-slice complete; 07-S5 and M1.5/07 remain partially complete; 08-12 not started.

## Scope

- Static direct calls now restore unsigned integer and float scalar locals through `ZrLibrary_AotRuntime_SyncUnsignedIntLocal()` and `ZrLibrary_AotRuntime_SyncFloatLocal()`.
- Generated C emits `zr_aot_direct_static_function_call_sync_u64_local_boundary` and `zr_aot_direct_static_function_call_sync_f64_local_boundary` when the destination slot has matching scalar-local declarations.
- Static direct call lowering no longer reintroduces `zr_aot_direct_call_result` payload reads for u64/f64 local restoration.
- Scalar-local analysis now marks non-tail call opcodes as result writes and back-propagates scalar kind from a stack-copy destination only when the copy source is a call-result slot.

## Evidence

- RED: call contracts failed before implementation because static direct-call lowering did not request u64/f64 local sync.
- RED: shared-library smoke then exposed that call-result temporary slots were not declared u64/f64 when the result was immediately copied into typed locals.
- Regression found: broad bidirectional stack-copy propagation incorrectly marked inline-struct value slots as scalar locals.
- Fix: destination-to-source propagation is limited to copies whose source slot is a non-tail call result destination.
- GREEN: focused call contracts 4/0 and call shared-library smoke 4/0.
- Broad focused group passed: call contracts 4/0, call shared-library smoke 4/0, shared-library smoke 8/0, value-type shared-library smoke 1/0, power contracts 2/0, power smoke 1/0, source contracts 19/0, generic numeric contracts/smoke 1/0 + 1/0, global smoke 9/0, logical contracts/smoke 4/0 + 4/0, typed scalar 1/0, return contracts 1/0, frame setup contracts 1/0.

## Generated C Checks

- Static numeric call smoke generated C contains direct static calls plus u64/f64 local-sync markers and helpers.
- The smoke executes `uint` and `float` no-arg static callees, copies results into typed locals, converts them to `int`, and returns `15`.
- Value-type call smoke remains on the inline-struct path after the targeted propagation fix.

## Follow-up

- `backend_aot_c_scalar_locals.c` remains oversized; a later slice should extract result-skip/liveness proof helpers instead of continuing to grow the same file.
