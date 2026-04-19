# 2026-04-19 Tracked Non-GC Profile Rerank After W2 Temp Receiver

## Scope

This note records the fresh tracked non-GC rerank that decided the next branch after the W2 temp
`TO_OBJECT` / `TO_STRUCT` member-call lowering fix.

The question was simple:

- keep pushing W2 quickening / type-inference hit rate first
- or return to W1 runtime hotspot work and close out the W1 acceptance line

The answer from the fresh data is the second one.

## Frozen Snapshot

Accepted snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_exact_pair_20260419`

Generated at:

- `2026-04-19T12:07:54Z`

Tracked cases included:

- `numeric_loops`
- `dispatch_loops`
- `container_pipeline`
- `matrix_add_2d`
- `map_object_access`
- `string_build`
- `call_chain_polymorphic`
- `mixed_service_loop`

All eight `ZR interp` rows report `PASS`.

## Results

Current wall-ms order:

1. `dispatch_loops = 148.978 ms`
2. `numeric_loops = 121.095 ms`
3. `container_pipeline = 119.022 ms`
4. `mixed_service_loop = 111.362 ms`
5. `map_object_access = 100.851 ms`
6. `string_build = 96.375 ms`
7. `call_chain_polymorphic = 93.887 ms`
8. `matrix_add_2d = 92.131 ms`

Aggregate tracked helpers:

- `value_copy = 5,210,463`
- `stack_get_value = 738,812`
- `get_member = 384,622`
- `precall = 178,850`
- `set_member = 174,068`

Aggregate tracked opcodes:

- `GET_MEMBER = 2`
- `GET_MEMBER_SLOT = 383,727`
- `SET_MEMBER_SLOT = 174,093`
- `FUNCTION_CALL = 327`
- `KNOWN_NATIVE_CALL = 4,167`
- `KNOWN_VM_CALL = 18,428`
- `TO_OBJECT = 24`

Aggregate tracked slowpaths:

- `protect_e = 3,981`
- `meta_fallback = 2,064`
- `callsite_cache_lookup = 640`

## Why This Rerank Changes Priority

The opcode mix shows that specialization is already landing:

- generic member fetch is effectively gone
- specialized member slot opcodes dominate
- known native and known VM calls already dwarf the remaining generic call count

So the remaining tracked non-GC cost is mostly runtime execution cost, not a missed-quickening story.

### `dispatch_loops`

From `dispatch_loops__zr_interp.hotspot.md` and `dispatch_loops__zr_interp.callgrind.annotate.txt`:

- total Ir: `368,618,055`
- top functions:
  - `ZrCore_Execute = 336,491,067 Ir`
  - `value_to_int64 = 2,764,800 Ir`
  - `execution_try_builtin_mul = 2,534,400 Ir`
- top helpers:
  - `value_copy = 2,889,069`
  - `stack_get_value = 615,436`
  - `get_member = 307,431`
- visible shared GC helper:
  - `garbage_collector_callsite_sanitize_tracing_enabled = 769,200 Ir`

This is not a "quickening miss" profile anymore. It is a runtime-body and helper-cost profile.

### `map_object_access`

From `map_object_access__zr_interp.callgrind.annotate.txt`:

- `zr_container_map_get_item_readonly_inline_fast = 871,623 Ir`
- `_int_malloc = 789,966 Ir`
- `garbage_collector_allocate_region_id = 623,335 Ir`
- `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands = 401,595 Ir`
- `zr_container_map_set_item_readonly_inline_no_result_fast = 375,136 Ir`

This case has already moved off the old member-PIC explanation and onto container/index/allocation support.

### `call_chain_polymorphic`

From `call_chain_polymorphic__zr_interp.profile.json`:

- `FUNCTION_CALL = 322`
- `callsite_cache_lookup = 640`

This is the remaining bounded W2-flavored pocket. It matters, but it is no longer the dominant explanation for the
tracked non-GC ranking.

## Decision

Accepted as the fresh tracked rerank for W1-T7/W1-T8.

The next step should return to W1 runtime work, not continue by default into more W2 quickening hit-rate chasing.

Priority from this snapshot:

1. `dispatch_loops` cached member execution bodies
2. `value_copy`
3. `stack_get_value`
4. `value_to_int64`
5. `execution_try_builtin_mul`

Bounded deferred W2 residue:

- `call_chain_polymorphic` generic `FUNCTION_CALL`
- `callsite_cache_lookup`

## Acceptance Boundary

This note claims:

- a fresh tracked non-GC rerank exists and was audited
- the rerank shows specialization already largely landed
- the highest-leverage next branch is runtime execution cost, not broader W2 hit-rate work

This note does not claim:

- a strict same-session before/after throughput delta for the temp-receiver W2 change itself

That narrower throughput delta belongs to the W2 implementation note. This note exists to record the fresh rerank and
the resulting prioritization decision.
