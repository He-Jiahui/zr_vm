# 2026-04-19 W1 Member Multislot Closeout

## Scope

This note closes the remaining W1 line for the current dispatch/member work:

- W1-T6: decide whether more low-risk cached-member slow-lane trimming still exists
- W1-T7: refresh the tracked non-GC `zr_interp` ranking on the accepted code line
- W1-T8: lock the W1 acceptance summary and the next optimization order honestly

This closeout is intentionally about `zr_interp`. W2 and W3 now have separate same-day acceptance notes.

## Accepted Runtime State

The exact-receiver cached-member lanes that still mattered for W1 are already front-loaded in
`zr_vm_core/src/zr_vm_core/execution/execution_member_access.c` before the generic receiver/prototype walk:

- single-slot exact-receiver pair hit
- multi-slot exact-receiver `object/member-name` own-field hit
- multi-slot exact-receiver callable hit
- multi-slot exact-receiver descriptor fallback
- version-mismatch invalidation before cached callable/descriptor reuse

That means the next non-GC performance branch is no longer "find another missed quickening lane". The current
runtime is already spending most of its remaining cost in steady-state execution bodies and shared helpers.

Coverage for the accepted cached-member behavior remains anchored by:

- `tests/core/test_execution_member_access_fast_paths.c`
- `tests/core/test_execution_dispatch_callable_metadata.c`

## Fresh Tracked Evidence

Frozen snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_exact_pair_20260419`

Report timestamp:

- `2026-04-19T12:07:54Z`

All eight tracked non-GC `ZR interp` rows report `PASS`.

Current wall-ms order:

1. `dispatch_loops = 148.978 ms`
2. `numeric_loops = 121.095 ms`
3. `container_pipeline = 119.022 ms`
4. `mixed_service_loop = 111.362 ms`
5. `map_object_access = 100.851 ms`
6. `string_build = 96.375 ms`
7. `call_chain_polymorphic = 93.887 ms`
8. `matrix_add_2d = 92.131 ms`

Tracked aggregate helper counts:

- `value_copy = 5,210,463`
- `stack_get_value = 738,812`
- `get_member = 384,622`
- `precall = 178,850`
- `set_member = 174,068`

Tracked aggregate opcode counts:

- `GET_MEMBER = 2`
- `GET_MEMBER_SLOT = 383,727`
- `SET_MEMBER_SLOT = 174,093`
- `FUNCTION_CALL = 327`
- `KNOWN_NATIVE_CALL = 4,167`
- `KNOWN_VM_CALL = 18,428`
- `TO_OBJECT = 24`

Tracked aggregate slowpaths:

- `protect_e = 3,981`
- `meta_fallback = 2,064`
- `callsite_cache_lookup = 640`

## Hotspot Conclusion

`dispatch_loops` remains the hottest tracked non-GC case. From
`dispatch_loops__zr_interp.hotspot.md` and `dispatch_loops__zr_interp.callgrind.annotate.txt` in the frozen snapshot:

- total Ir: `368,618,055`
- top functions:
  - `ZrCore_Execute = 336,491,067 Ir`
  - `value_to_int64 = 2,764,800 Ir`
  - `execution_try_builtin_mul = 2,534,400 Ir`
- top helpers:
  - `value_copy = 2,889,069`
  - `stack_get_value = 615,436`
  - `get_member = 307,431`
- visible shared helper function in callgrind:
  - `garbage_collector_callsite_sanitize_tracing_enabled = 769,200 Ir`

The acceptance decision from this rerank is:

1. specialization is already basically landing
2. the remaining non-GC runtime cost is not "more W2 quickening hit-rate"
3. the next runtime order should stay on:
   - `dispatch_loops` cached member execution bodies
   - `value_copy`
   - `stack_get_value`
   - then `value_to_int64` / `execution_try_builtin_mul`

`map_object_access` also confirms the member-PIC story has moved on. Its visible after-state cost is now led by:

- `zr_container_map_get_item_readonly_inline_fast = 871,623 Ir`
- `_int_malloc = 789,966 Ir`
- `garbage_collector_allocate_region_id = 623,335 Ir`
- `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands = 401,595 Ir`
- `zr_container_map_set_item_readonly_inline_no_result_fast = 375,136 Ir`

So W1 is no longer blocked on generalized member-cache specialization.

## Acceptance Decision

Accepted as the W1 closeout note for this wave.

What this note does claim:

- the remaining cached-member slow lanes worth taking in W1 are already in place
- a fresh tracked non-GC rerank exists on disk for the accepted code line
- the rerank clearly says "return to runtime execution cost", not "keep chasing W2 hit rate"
- there is no sign of an unexplained tracked non-GC regression banner in the accepted snapshot

What this note does not claim:

- a same-session one-slice-only before/after throughput delta for the final exact-pair/member cleanup

That missing strict A/B does not block W1 acceptance here, because the closeout criterion for this step is the
fresh hotspot rerank and the resulting prioritization decision, not a fabricated single-change benchmark delta.

## Follow-On Boundary

W1 is considered closed for this wave.

If the next runtime push continues from here, it should start with:

- `dispatch_loops` cached member execution bodies
- `value_copy`
- `stack_get_value`
- `value_to_int64`
- `execution_try_builtin_mul`

The compiler-side residual that remains worth keeping on backlog is now bounded to
`call_chain_polymorphic` generic call / `callsite_cache_lookup`, which belongs to W2 follow-up rather than W1 closeout.
