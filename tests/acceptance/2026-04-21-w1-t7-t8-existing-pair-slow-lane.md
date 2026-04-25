# 2026-04-21 W1-T7/W1-T8 Existing-Pair Slow-Lane Reuse

## Scope

This note records the retained follow-up after the earlier
`member_stack_operands_pin_skip` experiment.

The target stayed on the same W1 runtime-body line:

- `execution_member_set_by_name`
- `execution_member_set_cached`
- cached descriptor / by-name slow lanes that already proved an own member pair exists

The goal was to stop routing those existing-pair writes back through
`object_set_value_core(...)`, which was still paying a second hash lookup plus
object/value pin-unpin work even after the call path had already proved the
write was an update, not an add.

## Pre-Slice Baseline

This slice is measured against the fresh current-tree rerun:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_stack_operands_dispatch_cut_20260421-084740`

That baseline already includes the retained stack-operands specialization from:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`
- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

## Accepted Slice

### Existing-pair slow-lane direct update reuse

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_existing_pair_slow_lane_20260421-090115`

Decision:

- accepted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- object own-member lookup now has a reusable pair helper instead of value-only lookup duplication
- `SetMemberWithKey` / `TrySetMemberWithKeyFast` / `SetMemberCachedDescriptor` now directly call:
  - `ZrCore_Object_SetExistingPairValueUnchecked(...)`
  - followed by `memberVersion++`
- they only fall back to `object_set_value_core(...)` when the target pair is actually absent
- this keeps the slow lane on the W1 runtime-body line and avoids the redundant:
  - `existingValue != NULL`
  - then second hash lookup
  - then object/value ignore-registry pin-unpin

Representative callgrind totals improved again with no profile drift relative to
the pre-slice current-tree baseline:

- `numeric_loops`: `255,664,261 -> 255,610,483` (`-53,778`)
- `dispatch_loops`: `352,776,462 -> 352,730,251` (`-46,211`)
- `matrix_add_2d`: `12,198,049 -> 12,150,845` (`-47,204`)
- `map_object_access`: `22,237,595 -> 22,203,434` (`-34,161`)

Relative to the last accepted W1 tracked baseline, the representative line now
stays clearly below that older accepted state:

- `numeric_loops`: `257,035,951 -> 255,610,483` (`-1,425,468`, `-0.555%`)
- `dispatch_loops`: `354,141,124 -> 352,730,251` (`-1,410,873`, `-0.398%`)
- `matrix_add_2d`: `12,914,825 -> 12,150,845` (`-763,980`, `-5.916%`)
- `map_object_access`: `23,118,639 -> 22,203,434` (`-915,205`, `-3.959%`)

All tracked non-GC arrays stayed bit-for-bit identical relative to the
pre-slice current-tree rerun across:

- `numeric_loops`
- `dispatch_loops`
- `container_pipeline`
- `matrix_add_2d`
- `map_object_access`
- `string_build`
- `call_chain_polymorphic`
- `mixed_service_loop`

That means this is another real shared runtime-body cut, not a new opcode mix
or quickening drift.

## Regression Coverage

New regression tests kept in tree:

- `test_execution_member_set_by_name_existing_pair_slow_lane_reuses_existing_storage_without_ignore_registry_growth`
- `test_execution_member_set_cached_descriptor_existing_pair_slow_lane_reuses_existing_storage_without_ignore_registry_growth`

These force the non-stack slow lane into an ignore-registry-full environment and
prove that existing-pair updates no longer rely on fresh ignore-registry growth
to complete successfully.

## Validation

WSL gcc targeted tests pass:

- `zr_vm_value_copy_fast_paths_test`
- `zr_vm_execution_member_access_fast_paths_test`
- `zr_vm_gc_test`

WSL clang targeted tests also pass:

- `zr_vm_value_copy_fast_paths_test`
- `zr_vm_execution_member_access_fast_paths_test`
- `zr_vm_gc_test`

Windows MSVC CLI smoke rerun also passes:

- configure/build dir: `build-msvc-cli-smoke`
- command target: `zr_vm_cli_executable`
- fixture: `tests/fixtures/projects/hello_world/hello_world.zrp`
- output: `hello world`

WSL gcc benchmark rerun passes:

- `performance_report` with:
  - `ZR_VM_TEST_TIER=profile`
  - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
  - `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp`
  - `ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop`

GC smoke stays stable:

- `benchmark_gc_fragment_stress.zrp` -> `BENCH_GC_FRAGMENT_STRESS_PASS`
- checksum `829044624`

GC manual callgrind improves materially relative to the earlier stack-operands
cut, but still remains slightly above the current accepted W3 best:

- previous runtime experiment:
  - `gc_fragment_stress__manual_member_stack_operands_pin_skip_20260421 = 1,459,754,433 Ir`
- current state:
  - `gc_fragment_stress__manual_existing_pair_slow_lane_20260421 = 1,455,307,029 Ir`
  - delta vs previous: `-4,447,404` (`-0.305%`)
- current accepted W3 best:
  - `gc_fragment_stress__manual_object_short_storage_key_fast_20260421 = 1,453,661,197 Ir`
  - delta vs accepted W3 best: `+1,645,832` (`+0.113%`)

So this W1 slice is retained for the non-GC tracked line, while the W3 accepted
GC production baseline remains the earlier `object_short_storage_key_fast`
checkpoint until a later GC-side cut fully absorbs the remaining `+0.11%`.

## Rejected Follow-Up Experiments

### Member-result profiled copy

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_result_profiled_copy_20260421-121527`

Decision:

- rejected

Reason:

- relative to the accepted W1 tracked baseline:
  - `dispatch_loops`: `352,730,251 -> 353,034,674` (`+304,423`)
- relative to the fresh current-tree rerun:
  - `dispatch_loops`: `352,718,146 -> 353,034,674` (`+316,528`)
- the tracked non-GC `instructions/helpers/slowpaths` signature stayed identical
  to `performance_profile_tracked_non_gc_after_member_restore_state_local_value_copy_20260421-111522`
- this was a pure runtime-body regression inside the existing hot path, not a
  quickening or opcode-mix improvement

### Cached resolved-object execution lanes

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_resolved_object_lanes_20260421-124510`

Decision:

- rejected

Reason:

- relative to the accepted W1 tracked baseline:
  - `dispatch_loops`: `352,730,251 -> 352,993,537` (`+263,286`)
  - `map_object_access`: `22,203,434 -> 22,219,484` (`+16,050`)
- relative to the fresh current-tree rerun:
  - `dispatch_loops`: `352,718,146 -> 352,993,537` (`+275,391`)
  - `map_object_access`: `22,219,209 -> 22,219,484` (`+275`)
- the tracked non-GC `instructions/helpers/slowpaths` signature again stayed
  identical to `performance_profile_tracked_non_gc_after_member_restore_state_local_value_copy_20260421-111522`
- the extra resolved-object helper boundaries were therefore a pure runtime
  regression and were removed from the live tree

## Live-Tree Cleanup Checkpoint

### Rejected resolved-object helper residue removal

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_resolved_object_cleanup_20260421-130649`

Decision:

- retained as the live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production cleanup:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- removed the leftover exported `*ResolvedObjectUnchecked*` helper definitions
  from the rejected cached-resolved-object experiment
- after cleanup, all eight tracked non-GC cases again match the fresh
  `20260421-111522` rerun bit-for-bit in `instructions/helpers/slowpaths`
- representative callgrind totals versus that fresh current-tree rerun are:
  - `numeric_loops`: `255,594,072 -> 255,546,239` (`-47,833`)
  - `dispatch_loops`: `352,718,146 -> 352,998,644` (`+280,498`)
  - `matrix_add_2d`: `12,139,118 -> 12,128,279` (`-10,839`)
  - `map_object_access`: `22,219,209 -> 22,216,404` (`-2,805`)
- this restores the live tree to the same runtime/quickening signature used by
  the pre-rejection follow-up line, but the cleanup-only rebuild still does not
  supersede the accepted `existing_pair_slow_lane` performance checkpoint

### Exact-receiver object member-name pair backfill

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_exact_receiver_member_name_pair_backfill_20260421-132848`

Decision:

- retained as the current live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- exact-receiver-object cached get/set hits that only had `cachedMemberName`
  and no owner metadata now backfill `cachedReceiverPair`
- this converts later hits on the same receiver into the existing exact-pair hot
  fast path instead of repeating the object/member-name lane
- representative Callgrind versus the cleanup checkpoint stayed effectively
  neutral with unchanged tracked signatures:
  - `numeric_loops`: `255,546,239 -> 255,564,503` (`+18,264`)
  - `dispatch_loops`: `352,998,644 -> 353,001,431` (`+2,787`)
  - `matrix_add_2d`: `12,128,279 -> 12,134,289` (`+6,010`)
  - `map_object_access`: `22,216,404 -> 22,213,329` (`-3,075`)
- since the runtime body stayed on the same tracked signature and this lane now
  leaves the slot in a strictly cheaper follow-up state, the change is kept as
  the live-tree continuation point even though it does not justify replacing the
  accepted W1 checkpoint on aggregate totals

### Callable receiver-object backfill

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_callable_receiver_object_backfill_20260421-133549`

Decision:

- rejected
- reverted from the live tree

Reason:

- relative to the current live-tree continuation checkpoint:
  - `numeric_loops`: `255,564,503 -> 255,570,731` (`+6,228`)
  - `dispatch_loops`: `353,001,431 -> 353,023,212` (`+21,781`)
  - `matrix_add_2d`: `12,134,289 -> 12,127,975` (`-6,314`)
  - `map_object_access`: `22,213,329 -> 22,201,514` (`-11,815`)
- the tracked non-GC `instructions/helpers/slowpaths` signature stayed identical
  again, so the callable receiver-object backfill only added runtime-body cost
  without moving opcode mix or quickening coverage
- this attempt was therefore discarded before the next W1 runtime cut

### Multi-slot exact-receiver fast pass

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_exact_receiver_fast_pass_20260421-142454`

Decision:

- retained as the current live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- multi-slot cached get/set now run an exact-receiver-object fast pass before the
  prototype-hit scan
- this lets later exact slots win even when an earlier slot on the same
  prototype has already gone stale, instead of clearing the entire entry before
  the exact object/member-name, callable, or descriptor lane gets a chance to
  hit
- the new exact pass also keeps those multi-slot exact hits off the
  `receiverPrototype` resolution path unless they actually miss
- representative Callgrind versus the previous live continuation checkpoint is:
  - `numeric_loops`: `255,564,503 -> 255,542,028` (`-22,475`)
  - `dispatch_loops`: `353,001,431 -> 353,001,371` (`-60`)
  - `matrix_add_2d`: `12,134,289 -> 12,139,931` (`+5,642`)
  - `map_object_access`: `22,213,329 -> 22,196,979` (`-16,350`)
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to
  `performance_profile_tracked_non_gc_after_exact_receiver_member_name_pair_backfill_20260421-132848`
- this is therefore another retained runtime-body continuation cut, but still
  not strong enough on aggregate totals to replace the accepted
  `existing_pair_slow_lane` baseline

Validation refresh:

- WSL gcc targeted tests pass again:
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_gc_test`
- WSL clang targeted tests pass again with the same three binaries
- Windows MSVC CLI smoke rerun still passes:
  - build dir: `build-msvc-cli-smoke`
  - target: `zr_vm_cli_executable`
  - fixture: `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output: `hello world`
- fresh tracked rerun passes again with:
  - `ZR_VM_TEST_TIER=profile`
  - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
  - `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp`
  - `ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop`

### Value-copy plain heap-object fast path

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_value_copy_plain_heap_fast_path_20260421-144633`

Decision:

- retained as the current live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/include/zr_vm_core/value.h`
- `zr_vm_core/src/zr_vm_core/value.c`
- `tests/core/test_value_copy_fast_paths.c`

Reason:

- `ZrCore_Value_TryCopyFastNoProfile(...)` now keeps plain non-struct heap
  objects on the fast copy path instead of routing them through
  `ZrCore_Ownership_AssignValue(...)`
- `ZR_VALUE_TYPE_OBJECT` sources only fall back to the slow lane when the
  runtime object is actually a struct instance that still requires clone
  semantics
- the new regression coverage proves:
  - plain heap objects now hit `TryCopyFastNoProfile(...)`
  - plain struct objects still miss the fast path and stay on the clone lane

Representative Callgrind versus the previous live continuation checkpoint is:

- `numeric_loops`: `255,542,028 -> 255,546,665` (`+4,637`)
- `dispatch_loops`: `353,001,371 -> 352,984,829` (`-16,542`)
- `matrix_add_2d`: `12,139,931 -> 12,125,100` (`-14,831`)
- `map_object_access`: `22,196,979 -> 22,213,946` (`+16,967`)

All eight tracked non-GC cases again stayed bit-for-bit identical in
`instructions/helpers/slowpaths` relative to
`performance_profile_tracked_non_gc_after_multi_slot_exact_receiver_fast_pass_20260421-142454`:

- `numeric_loops`
- `dispatch_loops`
- `container_pipeline`
- `matrix_add_2d`
- `map_object_access`
- `string_build`
- `call_chain_polymorphic`
- `mixed_service_loop`

So this remains the same W1 runtime-body line, not a new opcode-mix or
quickening-coverage change. The representative totals are slightly mixed but
aggregate-neutral while improving the target `value_copy` lane with minimal
extra complexity, so the slice is kept as the new live continuation point.

Validation refresh:

- WSL gcc targeted tests pass:
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_gc_test`
- WSL clang targeted tests pass with the same three binaries
- Windows MSVC CLI smoke rerun still passes:
  - build dir: `build-msvc-cli-smoke`
  - target: `zr_vm_cli_executable`
  - fixture: `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output: `hello world`
- fresh tracked rerun passes with:
  - `ZR_VM_TEST_TIER=profile`
  - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
  - `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp`
  - `ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop`

### Value-copy fast-path body-cost cleanup

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_value_copy_fast_path_body_dedup_20260421-192519`

Decision:

- retained as the current live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/include/zr_vm_core/value.h`

Reason:

- `ZrCore_Value_TryCopyFastNoProfile(...)` now computes the normalized
  no-ownership gate once, returns immediately for the dominant non-GC or
  non-object plain-value lane, and only calls
  `ZrCore_Value_CanFastCopyPlainHeapObject(...)` when the source is actually a
  GC object value
- this is a body-cost cleanup on the already-retained `value_copy` fast path,
  not a semantic expansion:
  - plain heap object vs struct-object behavior stays unchanged
  - the helper/instruction/slowpath signature remains identical

Pre-slice fresh current-tree rerun totals, recorded immediately before this
body-cost cleanup and later overwritten by the after-reruns, were:

- `numeric_loops`: `256,032,138`
- `dispatch_loops`: `353,484,658`
- `matrix_add_2d`: `12,128,293`
- `map_object_access`: `22,198,445`

First rerun after the cleanup:

- `numeric_loops`: `256,050,991`
- `dispatch_loops`: `352,825,539`
- `matrix_add_2d`: `12,114,098`
- `map_object_access`: `22,106,827`

Confirming rerun captured by the retained snapshot:

- `numeric_loops`: `256,024,944`
- `dispatch_loops`: `352,813,372`
- `matrix_add_2d`: `12,118,899`
- `map_object_access`: `22,110,124`

Representative delta from the pre-slice rerun to the confirming rerun is:

- `numeric_loops`: `256,032,138 -> 256,024,944` (`-7,194`)
- `dispatch_loops`: `353,484,658 -> 352,813,372` (`-671,286`)
- `matrix_add_2d`: `12,128,293 -> 12,118,899` (`-9,394`)
- `map_object_access`: `22,198,445 -> 22,110,124` (`-88,321`)

Tracked signature check:

- the tracked non-GC `instructions/helpers/slowpaths` signature stayed
  identical across the pre-slice rerun, the first rerun, and the confirming
  rerun
- this is therefore another W1 runtime-body reduction, not a quickening hit-rate
  change or opcode mix shift

Commands used on this slice:

- rebuild benchmark release:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./scripts/benchmark/build_benchmark_release.sh"`
- tracked non-GC rerun:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop ctest --test-dir build/benchmark-gcc-release -R '^performance_report$' --output-on-failure"`

Validation refresh:

- WSL gcc targeted tests pass:
  - `zr_vm_instruction_execution_test`
  - `zr_vm_postcall_fast_paths_test`
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_object_call_known_native_fast_path_test`
- WSL clang targeted tests pass with the same five binaries
- `build/benchmark-gcc-release` rebuild succeeds before the tracked reruns

Next hotspot order after locking this slice:

- `stack_get_value`
- then `value_to_int64`
- then `execution_try_builtin_mul`

## W1-T7/W1-T8 Follow-Up Line

### Stack-get-value concat-growth follow-up

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_stack_get_value_concat_growth_20260421-204413`

Decision:

- retained as a bounded helper cleanup
- not promoted to a new accepted W1 performance baseline
- later superseded on the live tree by the `MUL` follow-up line

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c`
- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- several stack-anchor restore paths now use non-profile stack-slot reloads where
  the helper count was pure bookkeeping noise, so the tracked helper signature
  drops only in `stack_get_value`
- helper-count reductions were confirmed across all eight tracked non-GC cases:
  - `numeric_loops`: `37 -> 31`
  - `dispatch_loops`: `49 -> 43`
  - `container_pipeline`: `17614 -> 13512`
  - `matrix_add_2d`: `48 -> 42`
  - `map_object_access`: `30 -> 24`
  - `string_build`: `416 -> 410`
  - `call_chain_polymorphic`: `5166 -> 5160`
  - `mixed_service_loop`: `40 -> 34`
- representative Callgrind stayed mixed relative to the prior live continuation
  `performance_profile_tracked_non_gc_after_value_copy_fast_path_body_dedup_20260421-192519`:
  - `numeric_loops`: `256,024,944 -> 256,018,665` (`-6,279`)
  - `dispatch_loops`: `352,813,372 -> 352,821,495` (`+8,123`)
  - `matrix_add_2d`: `12,118,899 -> 12,132,586` (`+13,687`)
  - `map_object_access`: `22,110,124 -> 22,152,101` (`+41,977`)
- this made the slice worth keeping as low-risk bookkeeping cleanup, but not as
  the next accepted W1 performance checkpoint

### Mixed numeric/bool `MUL` runtime lane

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mul_mixed_numeric_runtime_lane_20260421-210437`

Decision:

- retained as the next live-tree continuation checkpoint on the numeric/runtime line
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c`
- `zr_vm_core/src/zr_vm_core/execution/execution_internal.h`
- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- `tests/instructions/test_instructions.c`

Reason:

- `MUL` generic exact numeric pairs already had their fast path, but mixed
  `int/uint/bool` cases were still falling through the slower meta/runtime lane
- the runtime now has an out-of-line mixed numeric/bool `MUL` helper that keeps
  these bounded mixed integral cases off the generic meta fallback without
  broadening the inline dispatch body
- new regression tests kept in tree:
  - `test_mul_generic_signed_bool_returns_int64_product`
  - `test_mul_generic_unsigned_bool_returns_int64_product`
  - `test_mul_generic_signed_unsigned_returns_int64_product`
- relative to the stack-get-value follow-up snapshot:
  - `numeric_loops`: `256,018,665 -> 256,012,334` (`-6,331`)
  - `dispatch_loops`: `352,821,495 -> 352,828,045` (`+6,550`)
  - `matrix_add_2d`: `12,132,586 -> 12,132,339` (`-247`)
  - `map_object_access`: `22,152,101 -> 22,115,142` (`-36,959`)
- all eight tracked non-GC cases stayed bit-for-bit identical in
  `instructions/helpers/slowpaths`, so this remained the same W1 runtime-body
  line rather than a quickening or opcode-mix change

Validation on this slice:

- WSL gcc targeted tests pass:
  - `zr_vm_execution_add_stack_relocation_test`
  - `zr_vm_instruction_execution_test`
  - `zr_vm_instructions_test`
- WSL clang targeted tests pass with the same three binaries
- `zr_vm_instructions_test` still keeps the two pre-existing baseline failures:
  - `test_get_member_slot_instruction_minor_gc_rewrites_forwarded_young_receiver_pair_before_permanent_pic_prune`
  - `test_set_member_slot_instruction_minor_gc_rewrites_forwarded_young_receiver_pair_before_permanent_pic_prune`

### Rejected mixed integral `ADD` direct lane

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_add_mixed_integral_direct_lane_20260421-212347`

Decision:

- rejected
- reverted after verification

Reason:

- the broadened direct-lane attempt pushed more mixed integral `ADD` work into
  the runtime body, but the representative totals regressed relative to the
  retained `MUL` checkpoint:
  - `numeric_loops`: `256,012,334 -> 256,039,618` (`+27,284`)
  - `dispatch_loops`: `352,828,045 -> 352,826,926` (`-1,119`)
  - `matrix_add_2d`: `12,132,339 -> 12,151,743` (`+19,404`)
  - `map_object_access`: `22,115,142 -> 22,118,694` (`+3,552`)
- the tracked non-GC `instructions/helpers/slowpaths` signature stayed
  identical again, so this was a pure runtime-body regression, not profile drift

### Cached-member pair direct backfill

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_member_pair_direct_backfill_20260421-215628`

Decision:

- retained as the current live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- exact-receiver object/member-name hits that already proved the correct own
  string pair no longer re-enter the full `execution_member_store_pic_slot(...)`
  path just to backfill `cachedReceiverObject/cachedReceiverPair`
- when the slot still has `cachedMemberName` but both the descriptor name and
  function member-entry symbol are gone, the refresh now preserves that last
  known cached name and directly backfills the pair instead of losing the exact
  receiver fast path
- this closes a real cached-member correctness boundary while still keeping the
  runtime on the same tracked W1 line

New regression tests kept in tree:

- `test_member_get_cached_exact_receiver_object_backfills_pair_from_slot_cached_name_without_descriptor_or_symbol`
- `test_member_set_cached_exact_receiver_object_backfills_pair_from_slot_cached_name_without_descriptor_or_symbol`

Representative Callgrind versus the retained `MUL` checkpoint is:

- `numeric_loops`: `256,012,334 -> 255,994,967` (`-17,367`)
- `dispatch_loops`: `352,828,045 -> 352,848,509` (`+20,464`)
- `matrix_add_2d`: `12,132,339 -> 12,109,809` (`-22,530`)
- `map_object_access`: `22,115,142 -> 22,113,292` (`-1,850`)

Tracked signature check:

- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to
  `performance_profile_tracked_non_gc_after_mul_mixed_numeric_runtime_lane_20260421-210437`
- this is therefore another retained runtime-body continuation cut, not a new
  quickening hit-rate change
- because `dispatch_loops` still nudges slightly positive, this remains a live
  continuation/correctness cleanup checkpoint instead of replacing the accepted
  `existing_pair_slow_lane` W1 baseline

Validation on this slice:

- WSL gcc targeted tests pass:
  - `zr_vm_execution_member_access_fast_paths_test`
- WSL clang targeted tests also pass:
  - `zr_vm_execution_member_access_fast_paths_test`
  - note: `--clean-first` rebuild was required once to invalidate a stale
    no-op build that initially reused an old binary
- Windows MSVC CLI smoke rerun passes:
  - build dir: `build-msvc-cli-smoke`
  - target: `zr_vm_cli_executable`
  - fixture: `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output: `hello world`
- fresh tracked non-GC benchmark rerun passes with:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/benchmark-gcc-release --parallel 16 && ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop ctest --test-dir build/benchmark-gcc-release -R '^performance_report$' --output-on-failure"`

### Rejected object_set_value_core pre-pin existing-pair probes

Snapshots:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_existing_pair_pre_pin_probe_20260421-*`

Decision:

- rejected and fully reverted

Reason:

- both attempted variants tried to pre-probe existing pairs inside the generic
  `ZrCore_Object_SetValue(...)` line before the normal pin/normalize/add flow
- the tracked non-GC `instructions/helpers/slowpaths` signature stayed
  identical to the retained `224708` continuation, so this was a pure
  runtime-body experiment rather than a quickening/opcode drift
- the wide version regressed representative totals versus `224708` by:
  - `numeric_loops`: `+296,266`
  - `dispatch_loops`: `+311,499`
  - `matrix_add_2d`: `+149,094`
  - `map_object_access`: `+139,397`
- the narrower cached-pair-only version still regressed versus `224708` by:
  - `numeric_loops`: `+70,586`
  - `dispatch_loops`: `+102,198`
  - `matrix_add_2d`: `+53,658`
  - `map_object_access`: `+54,013`
- this line is therefore closed for W1 unless a materially different design
  appears later

### String equality hash guard continuation

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_string_equal_hash_guard_20260421-232527`

Decision:

- retained as the new live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/src/zr_vm_core/string.c`

Reason:

- `ZrCore_String_Equal(...)` now returns `ZR_FALSE` immediately when
  `string1->super.hash != string2->super.hash`
- this is a shared runtime-body cut on the member/object-write line because
  `object_set_value_core(...)` and related cached member slow lanes still spend
  visible time on string key equality after the earlier member-path cuts
- relative to the prior live continuation
  `performance_profile_tracked_non_gc_after_cached_instance_field_set_after_fast_miss_20260421-224708`,
  representative Callgrind moved by:
  - `numeric_loops`: `256,010,216 -> 256,004,921` (`-5,295`)
  - `dispatch_loops`: `352,821,973 -> 352,676,235` (`-145,738`)
  - `matrix_add_2d`: `12,122,363 -> 12,059,892` (`-62,471`)
  - `map_object_access`: `22,055,016 -> 22,047,282` (`-7,734`)
- function-level movement on the intended line versus `224708`:
  - `dispatch_loops`:
    - `ZrCore_String_Equal`: `552,116 -> 501,696` (`-50,420`)
    - `object_set_value_core`: `845,008 -> 847,737` (`+2,729`)
- all eight tracked non-GC cases remained bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to `224708`
- it is kept as the continuation checkpoint because it improves every tracked
  representative case on the same runtime signature, but it does not replace
  the accepted W1 baseline because `numeric_loops` still remains above the
  accepted `existing_pair_slow_lane` checkpoint

Validation on this slice:

- WSL gcc targeted tests pass:
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_gc_test`
- WSL clang targeted tests pass:
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_gc_test`
- Windows MSVC CLI smoke rerun passes:
  - build dir: `build-msvc-cli-smoke`
  - target: `zr_vm_cli_executable`
  - fixture: `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output: `hello world`
- fresh tracked non-GC benchmark rerun passes with:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/benchmark-gcc-release --parallel 16 && ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop ctest --test-dir build/benchmark-gcc-release -R '^performance_report$' --output-on-failure"`

### Rejected unpin disarm helper probe

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_unpin_disarm_probe_20260421-233835`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe tried to reduce `UnignoreObject(...)` by adding a generic
  post-barrier disarm helper inside `object_set_value_core(...)`
- the idea was directionally correct, but it put extra bookkeeping back onto
  the hottest generic object-write body
- relative to the retained live continuation
  `performance_profile_tracked_non_gc_after_string_equal_hash_guard_20260421-232527`,
  representative Callgrind moved by:
  - `numeric_loops`: `256,004,921 -> 256,009,649` (`+4,728`)
  - `dispatch_loops`: `352,676,235 -> 352,687,772` (`+11,537`)
  - `matrix_add_2d`: `12,059,892 -> 12,075,859` (`+15,967`)
  - `map_object_access`: `22,047,282 -> 22,039,206` (`-8,076`)
- function-level movement on the targeted line versus `232527` shows why it was
  rejected:
  - `dispatch_loops`:
    - `object_set_value_core`: `847,737 -> 876,504` (`+28,767`)
    - `ZrCore_GarbageCollector_UnignoreObject`: `284,984 -> 278,404`
      (`-6,580`)
  - `map_object_access`:
    - `object_set_value_core`: `478,794 -> 493,502` (`+14,708`)
    - `ZrCore_GarbageCollector_UnignoreObject`: `154,828 -> 151,048`
      (`-3,780`)
- all eight tracked non-GC cases still matched bit-for-bit in
  `instructions/helpers/slowpaths` relative to `232527`
- this was therefore a pure runtime-body regression, not profile drift
- conclusion: reducing `UnignoreObject(...)` is still worthwhile, but not by
  adding generic disarm work back into `object_set_value_core(...)`

### Unpin ignored-registry sentinel guard continuation

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`

Decision:

- retained as the new live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- `object_unpin_raw_object(...)` now returns immediately when
  `object->garbageCollectMark.ignoredRegistryIndex == ZR_MAX_SIZE`
- this keeps the improvement at the actual unpin site instead of pushing new
  generic bookkeeping back into `object_set_value_core(...)`
- in the common retained case, the write barrier has already released the
  temporary root, so the later unconditional `UnignoreObject(...)` was
  redundant
- relative to the previous live continuation
  `performance_profile_tracked_non_gc_after_string_equal_hash_guard_20260421-232527`,
  representative Callgrind moved by:
  - `numeric_loops`: `256,004,921 -> 255,988,666` (`-16,255`)
  - `dispatch_loops`: `352,676,235 -> 352,649,083` (`-27,152`)
  - `matrix_add_2d`: `12,059,892 -> 12,046,676` (`-13,216`)
  - `map_object_access`: `22,047,282 -> 22,018,491` (`-28,791`)
- function-level movement on the intended line versus `232527`:
  - `dispatch_loops`:
    - `object_set_value_core`: `847,737 -> 844,081` (`-3,656`)
    - `ZrCore_GarbageCollector_UnignoreObject`: `284,984 -> 278,404`
      (`-6,580`)
    - `ZrCore_String_Equal`: `501,696 -> 496,584` (`-5,112`)
  - `map_object_access`:
    - `object_set_value_core`: `478,794 -> 474,281` (`-4,513`)
    - `ZrCore_GarbageCollector_UnignoreObject`: `154,828 -> 151,048`
      (`-3,780`)
    - `ZrCore_String_Equal`: `114,608 -> 108,936` (`-5,672`)
- all eight tracked non-GC cases again remained bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to `232527`
- relative to the accepted W1 tracked checkpoint
  `performance_profile_tracked_non_gc_after_existing_pair_slow_lane_20260421-090115`,
  the representative line is now:
  - `numeric_loops`: `255,610,483 -> 255,988,666` (`+378,183`)
  - `dispatch_loops`: `352,730,251 -> 352,649,083` (`-81,168`)
  - `matrix_add_2d`: `12,150,845 -> 12,046,676` (`-104,169`)
  - `map_object_access`: `22,203,434 -> 22,018,491` (`-184,943`)
- because `numeric_loops` still remains above the accepted baseline, this is
  retained as the current live continuation rather than replacing the accepted
  `existing_pair_slow_lane` W1 checkpoint

Validation on this slice:

- WSL gcc targeted tests pass:
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_gc_test`
- WSL clang targeted tests pass:
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_gc_test`
- Windows MSVC CLI smoke rerun passes:
  - build dir: `build-msvc-cli-smoke`
  - target: `zr_vm_cli_executable`
  - fixture: `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output: `hello world`
- fresh tracked non-GC benchmark rerun passes with:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/benchmark-gcc-release --parallel 16 && ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop ctest --test-dir build/benchmark-gcc-release -R '^performance_report$' --output-on-failure"`

### Rejected existing-pair inline helper wrapper probe

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_existing_pair_inline_helper_20260422-022957`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe tried to inline the body of
  `ZrCore_Object_SetExistingPairValueUnchecked(...)` into its internal hot
  callers while keeping the exported API as a thin wrapper
- all eight tracked non-GC cases remained bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to the current live continuation
  `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`
- representative Callgrind versus that live continuation moved by:
  - `numeric_loops`: `255,988,666 -> 255,965,972` (`-22,694`)
  - `dispatch_loops`: `352,649,083 -> 352,688,548` (`+39,465`)
  - `matrix_add_2d`: `12,046,676 -> 12,047,880` (`+1,204`)
  - `map_object_access`: `22,018,491 -> 22,020,510` (`+2,019`)
- function-level movement on the targeted write path shows the wrapper removal
  did not help the real hot bodies:
  - `dispatch_loops`:
    - `object_set_value_core`: `844,081 -> 851,737` (`+7,656`)
    - `ZrCore_String_Equal`: `496,584 -> 507,552` (`+10,968`)
    - `ZrCore_GarbageCollector_Barrier`: unchanged at `364,097`
  - `map_object_access`:
    - `object_set_value_core`: `474,281 -> 475,678` (`+1,397`)
    - `ZrCore_Object_SetExistingPairValueUnchecked`: `192,743 -> 192,512`
      (`-231`)
    - `ZrCore_String_Equal`: `108,936 -> 109,720` (`+784`)
- this was therefore another pure runtime-body regression; the live tree stays
  at `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`

### Rejected deferred long-string storage-key canonicalization probe

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_deferred_long_string_storage_key_20260422-024617`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- this probe deferred long-string storage-key canonicalization until after an
  existing-pair lookup miss, keeping the normal pin/unpin flow unchanged
- the TDD regression used during the probe proved the intended behavior change:
  an equal long-string update could avoid the long-string canonicalization
  allocation path when a pair already existed
- however, fresh tracked non-GC data still showed another pure runtime-body
  regression on the same live signature:
  - `numeric_loops`: `255,988,666 -> 256,038,493` (`+49,827`)
  - `dispatch_loops`: `352,649,083 -> 352,662,260` (`+13,177`)
  - `matrix_add_2d`: `12,046,676 -> 12,057,886` (`+11,210`)
  - `map_object_access`: `22,018,491 -> 22,034,480` (`+15,989`)
- all eight tracked non-GC cases again remained bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to the live continuation
- function-level movement versus `235849` shows why it was rejected:
  - `dispatch_loops`:
    - `object_set_value_core`: `844,081 -> 866,213` (`+22,132`)
    - `ZrCore_String_Equal`: `496,584 -> 495,760` (`-824`)
    - `string_create_short`: `353,576 -> 351,431` (`-2,145`)
  - `map_object_access`:
    - `object_set_value_core`: `474,281 -> 487,944` (`+13,663`)
    - `ZrCore_String_Equal`: `108,936 -> 110,424` (`+1,488`)
    - `string_create_short`: `189,249 -> 187,268` (`-1,981`)
- the saved string-creation work was smaller than the added branch/body cost in
  the hottest generic write path, so both the probe and its temporary test were
  removed

### Cached instance-field set after-fast-miss reuse

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_instance_field_set_after_fast_miss_20260421-224708`

Decision:

- retained as the current live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`
- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- extracted `ZrCore_Object_SetExistingPairValueAfterFastMissUnchecked(...)`
  so callers that already proved the plain existing-pair fast path missed no
  longer pay the same `object_try_set_existing_pair_plain_value_fast_unchecked(...)`
  body a second time inside `ZrCore_Object_SetExistingPairValueUnchecked(...)`
- cached instance-field set now routes both:
  - exact-receiver pair hits
  - exact receiver object/member-name hits
  through that shared after-fast-miss body
- the object/member-name cached slow lane also now reuses the existing
  `NON_HIDDEN_STRING_PAIR_FAST_SET` PIC slot flag before falling back, so the
  same non-hidden fast set path already used by the exact-pair lane can trigger
  even when the slot has not yet backfilled `cachedReceiverPair`

New regression test kept in tree:

- `test_object_set_existing_pair_value_after_fast_miss_updates_value_with_hidden_items_cached_state`

Representative Callgrind versus the prior live continuation
`performance_profile_tracked_non_gc_after_cached_member_pair_direct_backfill_20260421-215628`
is:

- `numeric_loops`: `255,994,967 -> 256,010,216` (`+15,249`)
- `dispatch_loops`: `352,848,509 -> 352,821,973` (`-26,536`)
- `matrix_add_2d`: `12,109,809 -> 12,122,363` (`+12,554`)
- `map_object_access`: `22,113,292 -> 22,055,016` (`-58,276`)

Tracked signature check:

- all eight tracked non-GC cases stayed bit-for-bit identical in
  `instructions/helpers/slowpaths`
- this keeps the tree on the same W1 runtime-body line rather than introducing
  any new opcode/quickening drift

Function-level callgrind on the targeted write path moved in the intended
direction:

- `dispatch_loops`:
  - `object_set_value_core`: `849,813 -> 845,008` (`-4,805`)
  - `ZrCore_String_Equal`: `557,296 -> 552,116` (`-5,180`)
- `map_object_access`:
  - `ZrCore_Object_SetExistingPairValueUnchecked`: `254,522 -> 192,743`
    (`-61,779`)
  - `object_set_value_core`: `475,367 -> 474,868` (`-499`)

Because the representative totals still stay mixed, and the current live tree
is still above the accepted `existing_pair_slow_lane` checkpoint on
`numeric_loops` / `dispatch_loops`, this cut is retained as another live W1
continuation instead of replacing the accepted baseline.

Validation on this slice:

- WSL gcc targeted tests pass:
  - `zr_vm_execution_member_access_fast_paths_test`
- WSL clang targeted tests pass:
  - `zr_vm_execution_member_access_fast_paths_test`
- Windows MSVC CLI smoke rerun passes:
  - build dir: `build-msvc-cli-smoke`
  - target: `zr_vm_cli_executable`
  - fixture: `tests/fixtures/projects/hello_world/hello_world.zrp`
  - output: `hello world`
- fresh tracked non-GC benchmark rerun passes with:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/benchmark-gcc-release --parallel 16 && ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop ctest --test-dir build/benchmark-gcc-release -R '^performance_report$' --output-on-failure"`

### Rejected non-hidden after-fast-miss cached-set helper probe

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_nonhidden_after_fast_miss_20260422-032252`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

Test coverage kept in tree:

- `test_member_set_cached_instance_field_pair_non_hidden_slow_lane_does_not_touch_hidden_items_literal_cache`
- `test_member_set_cached_exact_receiver_object_non_hidden_slow_lane_does_not_touch_hidden_items_literal_cache`

Reason:

- this probe tried to make `NON_HIDDEN_STRING_PAIR_FAST_SET` miss paths in
  cached instance-field set lanes jump directly into a narrower
  non-hidden-only after-fast-miss helper, instead of falling back through the
  generic existing-pair setter body
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to the live continuation
  `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`
- fresh representative Callgrind versus `235849` moved by:
  - `numeric_loops`: `255,988,666 -> 255,978,982` (`-9,684`)
  - `dispatch_loops`: `352,649,083 -> 352,680,821` (`+31,738`)
  - `matrix_add_2d`: `12,046,676 -> 12,062,847` (`+16,171`)
  - `map_object_access`: `22,018,491 -> 22,010,794` (`-7,697`)
- the targeted write-path bodies also moved the wrong way in the hottest
  dispatch case:
  - `dispatch_loops`:
    - `object_set_value_core`: `844,081 -> 848,659` (`+4,578`)
    - `ZrCore_String_Equal`: `496,584 -> 504,568` (`+7,984`)
    - `ZrCore_GarbageCollector_Barrier`: unchanged at `364,097`
  - `map_object_access`:
    - `object_set_value_core`: `474,281 -> 474,892` (`+611`)
    - `ZrCore_Object_SetExistingPairValueUnchecked`: unchanged at `192,743`
    - `ZrCore_String_Equal`: `108,936 -> 108,064` (`-872`)
- this is therefore another pure runtime-body/code-layout regression on the
  same tracked signature; the production helper was removed and the live tree
  stays at `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`

Validation after revert:

- WSL gcc targeted tests pass:
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_value_copy_fast_paths_test`
- WSL clang targeted tests pass:
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_value_copy_fast_paths_test`

### Rejected multi-slot exact/prototype single-pass probe

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multislot_single_pass_20260422-051840`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Reason:

- this probe merged the multi-slot cached get/set exact-receiver pass and the
  later prototype pass into a single scan, while still trying to preserve the
  later exact-slot precedence over an earlier prototype slot
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to the live continuation
  `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`
- fresh representative Callgrind versus `235849` still came back mixed:
  - `numeric_loops`: `255,988,666 -> 255,978,597` (`-10,069`)
  - `dispatch_loops`: `352,649,083 -> 352,650,858` (`+1,775`)
  - `matrix_add_2d`: `12,046,676 -> 12,048,092` (`+1,416`)
  - `map_object_access`: `22,018,491 -> 22,026,382` (`+7,891`)
- the targeted object/string bodies also moved the wrong way in representative
  cases:
  - `dispatch_loops`:
    - `object_set_value_core`: `844,081 -> 844,649` (`+568`)
    - `ZrCore_String_Equal`: `496,584 -> 497,544` (`+960`)
    - `ZrCore_GarbageCollector_Barrier`: unchanged at `364,097`
  - `matrix_add_2d`:
    - `object_set_value_core`: `472,277 -> 474,322` (`+2,045`)
    - `ZrCore_String_Equal`: `159,840 -> 160,528` (`+688`)
  - `map_object_access`:
    - `object_set_value_core`: `474,281 -> 475,909` (`+1,628`)
    - `ZrCore_Object_SetExistingPairValueUnchecked`: unchanged at `192,743`
    - `ZrCore_String_Equal`: `108,936 -> 111,216` (`+2,280`)
- function-level extraction from the cached access helpers themselves did not
  move at all:
  - `dispatch_loops`:
    - `execution_member_get_cached`: unchanged at `2,606`
    - `execution_member_set_cached`: unchanged at `864`
    - `execution_member_try_cached_get`: unchanged at `936`
    - `execution_member_try_cached_set`: unchanged at `296`
  - `map_object_access`:
    - `execution_member_get_cached`: unchanged at `306`
    - `execution_member_try_cached_get`: unchanged at `108`
- this is therefore another pure runtime-body/code-layout regression without a
  measurable win on the actual cached get/set hotspot; the live tree stays at
  `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`

### Rejected dispatch-local inline fast-copy predicate probe

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_inline_fast_copy_predicate_20260422-043959`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Test coverage kept in tree:

- `test_execute_get_constant_plain_heap_object_reuses_original_object`
- `test_execute_get_constant_struct_object_still_clones_result`

Reason:

- this probe replaced the hot dispatch copy helpers' ownership-only bit check
  with a local object-aware inline predicate so interpreter stack/ret copies
  could keep plain non-struct heap objects on the same inlined lane without
  routing through the broader `TryCopyFastNoProfile(...)` helper body
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to the live continuation
  `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`
- fresh representative Callgrind versus `235849` moved by:
  - `numeric_loops`: `255,988,666 -> 263,164,005` (`+7,175,339`)
  - `dispatch_loops`: `352,649,083 -> 364,021,996` (`+11,372,913`)
  - `matrix_add_2d`: `12,046,676 -> 12,043,785` (`-2,891`)
  - `map_object_access`: `22,018,491 -> 22,293,618` (`+275,127`)
- the intended value-copy / cached-access functions did not improve at all:
  - `dispatch_loops`:
    - `ZrCore_Value_CanFastCopyPlainHeapObject`: unchanged at `10,591`
    - `execution_member_get_cached`: unchanged at `2,606`
    - `execution_member_set_cached`: unchanged at `864`
    - `execution_member_try_cached_get`: unchanged at `936`
    - `execution_member_try_cached_set`: unchanged at `296`
  - `numeric_loops`:
    - `ZrCore_Value_CanFastCopyPlainHeapObject`: unchanged at `10,319`
- representative object/string bodies instead stayed flat or rose:
  - `dispatch_loops`:
    - `object_set_value_core`: `844,081 -> 844,386` (`+305`)
    - `ZrCore_String_Equal`: `496,584 -> 496,216` (`-368`)
  - `map_object_access`:
    - `object_set_value_core`: `474,281 -> 475,958` (`+1,677`)
    - `ZrCore_String_Equal`: `108,936 -> 110,808` (`+1,872`)
- this is therefore another pure runtime-body/code-layout regression on the
  same tracked signature; the production change was removed and the live tree
  stays at `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`

Validation after revert:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`

### Rejected integral/bool unchecked numeric extraction probe

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_numeric_integral_bool_int64_unchecked_20260422-050322`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c`

Test coverage kept in tree:

- `tests/core/test_execution_numeric_fast_paths.c`
- `tests/CMakeLists.txt`

Reason:

- this probe introduced an unchecked integral/bool-to-`int64` helper and reused
  it inside `value_to_int64(...)`, the mixed integral/bool lane in
  `try_builtin_add(...)`, and `execution_try_builtin_mul_mixed_numeric_fast(...)`
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to the live continuation
  `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`
- the first fresh tracked rerun versus `235849` looked mildly positive overall:
  - `numeric_loops`: `255,988,666 -> 255,986,465` (`-2,201`)
  - `dispatch_loops`: `352,649,083 -> 352,636,781` (`-12,302`)
  - `matrix_add_2d`: `12,046,676 -> 12,036,495` (`-10,181`)
  - `map_object_access`: `22,018,491 -> 22,029,797` (`+11,306`)
- but the confirm rerun on the same tree did not hold that direction:
  - `numeric_loops`: `255,988,666 -> 255,970,447` (`-18,219`)
  - `dispatch_loops`: `352,649,083 -> 352,645,443` (`-3,640`)
  - `matrix_add_2d`: `12,046,676 -> 12,055,141` (`+8,465`)
  - `map_object_access`: `22,018,491 -> 22,036,080` (`+17,589`)
- direct callgrind extraction did not surface the target numeric helpers as
  standalone retained symbols after inlining, so the decision fell back to the
  tracked signature plus representative case stability
- confirm function-level movement in representative object/string bodies was
  also mixed:
  - `dispatch_loops`:
    - `object_set_value_core`: `844,081 -> 843,383` (`-698`)
    - `ZrCore_String_Equal`: `496,584 -> 495,072` (`-1,512`)
  - `map_object_access`:
    - `object_set_value_core`: `474,281 -> 478,700` (`+4,419`)
    - `ZrCore_String_Equal`: `108,936 -> 113,824` (`+4,888`)
- this is therefore another pure runtime-body/code-layout probe without a
  stable retained win; the production code was removed and the live tree stays
  at `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`

Validation after revert:

- WSL gcc targeted build/test pass:
  - `cmake --build build-wsl-gcc --target zr_vm_execution_numeric_fast_paths_test zr_vm_execution_add_stack_relocation_test --parallel 8`
  - `build-wsl-gcc/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_add_stack_relocation_test`
- WSL clang targeted build/test pass:
  - `cmake --build build-wsl-clang --target zr_vm_execution_numeric_fast_paths_test zr_vm_execution_add_stack_relocation_test --parallel 8`
  - `build-wsl-clang/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_add_stack_relocation_test`

### Retained `ZrCore_Object_GetValue(...)` ready-map direct lookup probe

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_object_get_value_ready_map_direct_20260422-053411`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_object_get_value_ready_map_direct_confirm_20260422-053733`

Decision:

- retained as the new live-tree continuation checkpoint
- not promoted to a new accepted W1 performance baseline

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- `ZrCore_Object_GetValue(...)` now does one `object_node_map_is_ready(...)`
  guard, uses `object_get_own_value_unchecked(...)` directly on the ready-map
  lane, and chooses the fallback prototype once before the prototype walk
- this avoids routing ready own-lookups back through the broader
  `object_get_own_value(...)` wrapper while preserving the existing null-key,
  map-not-ready, and prototype fallback semantics
- both fresh tracked non-GC reruns stayed bit-for-bit identical to the prior
  live continuation
  `performance_profile_tracked_non_gc_after_unpin_ignored_registry_sentinel_guard_20260421-235849`
  in `instructions/helpers/slowpaths` across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- first rerun representative Callgrind versus `235849`:
  - `numeric_loops`: `255,988,666 -> 255,968,054` (`-20,612`)
  - `dispatch_loops`: `352,649,083 -> 352,647,833` (`-1,250`)
  - `matrix_add_2d`: `12,046,676 -> 12,046,912` (`+236`)
  - `map_object_access`: `22,018,491 -> 22,007,295` (`-11,196`)
- confirm rerun versus `235849`:
  - `numeric_loops`: `255,988,666 -> 255,967,657` (`-21,009`)
  - `dispatch_loops`: `352,649,083 -> 352,634,707` (`-14,376`)
  - `matrix_add_2d`: `12,046,676 -> 12,045,010` (`-1,666`)
  - `map_object_access`: `22,018,491 -> 22,016,834` (`-1,657`)
- confirm function-level movement stays aligned with the intended read-side cut:
  - `dispatch_loops`: `ZrCore_Object_GetValue 202,736 -> 197,224` (`-5,512`)
  - `map_object_access`: `ZrCore_Object_GetValue 110,671 -> 108,374` (`-2,297`)
- the net gain is small, so this remains a live continuation / runtime-body
  cleanup rather than a new accepted W1 checkpoint, but unlike the rejected
  probes above it stayed directionally positive across both reruns

Validation after keep:

- WSL gcc targeted build/test pass:
  - `cmake --build build-wsl-gcc --target zr_vm_execution_member_access_fast_paths_test zr_vm_container_temp_value_root_test --parallel 8`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_container_temp_value_root_test`
- WSL clang targeted build/test pass:
  - `cmake --build build-wsl-clang --target zr_vm_execution_member_access_fast_paths_test zr_vm_container_temp_value_root_test --parallel 8`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_container_temp_value_root_test`

### Accepted `SET_STACK` materialized none-ownership fast lane

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_stack_materialized_none_fast_lane_20260422-055136`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_stack_materialized_none_fast_lane_confirm_20260422-055330`

Decision:

- accepted
- promoted as the new accepted W1 tracked checkpoint

Production change:

- `zr_vm_core/include/zr_vm_core/value.h`
- `tests/core/test_execution_dispatch_callable_metadata.c`

Reason:

- `ZrCore_Value_AssignMaterializedStackValueNoProfile(...)` now front-loads the
  common `ownershipKind == NONE` lane:
  - when both source and destination already have normalized no-ownership
    state, non-object values copy by bits immediately
  - plain non-struct heap objects also copy by bits immediately
  - only transfer/weak/object-clone cases keep falling through to the existing
    transfer or slow-copy body
- because this helper is only used by `SET_STACK` materialization in the
  interpreter, the cut stays narrowly scoped to the hottest dispatch copy body
  rather than reopening the rejected broad `value_copy` helper probes
- new integration guard rails keep the exact runtime path covered:
  - `test_execute_get_constant_then_set_stack_plain_heap_object_reuses_original_object`
  - `test_execute_get_constant_then_set_stack_struct_object_still_clones_result`
- both tracked non-GC reruns stayed bit-for-bit identical to the prior live
  continuation
  `performance_profile_tracked_non_gc_after_object_get_value_ready_map_direct_confirm_20260422-053733`
  in `instructions/helpers/slowpaths` across all eight tracked non-GC cases
- first rerun representative Callgrind versus `053733`:
  - `numeric_loops`: `255,967,657 -> 255,463,777` (`-503,880`)
  - `dispatch_loops`: `352,634,707 -> 350,341,172` (`-2,293,535`)
  - `matrix_add_2d`: `12,045,010 -> 12,039,408` (`-5,602`)
  - `map_object_access`: `22,016,834 -> 21,909,735` (`-107,099`)
- confirm rerun versus `053733`:
  - `numeric_loops`: `255,967,657 -> 255,474,138` (`-493,519`)
  - `dispatch_loops`: `352,634,707 -> 350,330,462` (`-2,304,245`)
  - `matrix_add_2d`: `12,045,010 -> 12,053,512` (`+8,502`)
  - `map_object_access`: `22,016,834 -> 21,931,485` (`-85,349`)
- even with the small confirm-only `matrix_add_2d` wobble, the new checkpoint
  cleanly stays below the previous accepted W1 baseline on all four
  representative cases:
  - `numeric_loops`: `255,610,483 -> 255,474,138` (`-136,345`)
  - `dispatch_loops`: `352,730,251 -> 350,330,462` (`-2,399,789`)
  - `matrix_add_2d`: `12,150,845 -> 12,053,512` (`-97,333`)
  - `map_object_access`: `22,203,434 -> 21,931,485` (`-271,949`)
- the retained top-function movement also stays aligned with a dispatch-body
  cut instead of another allocator/object-write side effect:
  - `dispatch_loops`: `ZrCore_Execute 335,335,414 -> 333,029,805` (`-2,305,609`)
  - `numeric_loops`: `ZrCore_Execute 243,429,510 -> 242,944,358` (`-485,152`)
  - `map_object_access`: `ZrCore_Execute 9,087,994 -> 9,011,589` (`-76,405`)
- this is therefore the first post-`090115` W1 continuation slice that holds a
  clearly better accepted tracked state rather than only a live-tree cleanup

Validation after accept:

- WSL gcc targeted build/test pass:
  - `cmake --build build-wsl-gcc --target zr_vm_execution_dispatch_callable_metadata_test zr_vm_value_copy_fast_paths_test --parallel 8`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang targeted build/test pass:
  - `cmake --build build-wsl-clang --target zr_vm_execution_dispatch_callable_metadata_test zr_vm_value_copy_fast_paths_test --parallel 8`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
- WSL gcc tracked non-GC benchmark reruns pass:
  - `cmake --build build/benchmark-gcc-release --parallel 16`
  - `cmake --build build/benchmark-gcc-release --target run_performance_suite --parallel 1`
    with:
    - `ZR_VM_TEST_TIER=profile`
    - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
    - `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp`
    - `ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop`

### Rejected descriptorless callable same-prototype prototype-hit probe

Snapshots:

- live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_descriptorless_callable_prototype_hit_20260422-093531`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_descriptorless_callable_prototype_hit_confirm_20260422-093658`
- third rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_descriptorless_callable_prototype_hit_rerun_20260422-093826`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Reason:

- this probe tried to let cached descriptorless prototype callables hit on a
  different receiver object that shares the same prototype, as long as the
  receiver does not own-shadow that member name
- all three reruns again stayed bit-for-bit identical in tracked
  `instructions/helpers/slowpaths` versus both kept live `092432` and accepted
  `055330`, so this was another pure runtime-body/code-layout experiment
- representative totals would not stabilize:
  - first versus kept live `092432`:
    - `numeric_loops`: `-3,639`
    - `dispatch_loops`: `-11,093`
    - `matrix_add_2d`: `+4,099`
    - `map_object_access`: `-1,566`
  - confirm versus kept live `092432`:
    - `numeric_loops`: `+53,324`
    - `dispatch_loops`: `-6,025`
    - `matrix_add_2d`: `+14,878`
    - `map_object_access`: `-16,546`
  - third versus kept live `092432`:
    - `numeric_loops`: `-5,958`
    - `dispatch_loops`: `-2,778`
    - `matrix_add_2d`: `+8,769`
    - `map_object_access`: `+16,049`
- because the probe kept flipping between `numeric_loops` and
  `map_object_access` regressions on the same tracked signature, it does not
  hold a stable retained win and should not stay in the live tree

Validation after revert:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

### Rejected cached-descriptor descriptor-key-stable shortcut

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_descriptor_key_stable_20260422-122946`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_descriptor_key_stable_confirm_20260422-123126`
- third rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_descriptor_key_stable_rerun_20260422-123318`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Temporary TDD coverage reverted with the probe:

- `tests/core/test_execution_member_access_fast_paths.c`
  - `test_execution_member_set_cached_descriptor_stack_operands_add_path_reuses_descriptor_key_without_ignore_registry_growth`

Reason:

- this probe narrowed the cached-descriptor add path in
  `object_set_member_cached_descriptor_unchecked_core(...)` so
  `descriptor->name` would be treated as prototype-owned stable metadata and
  skip the temporary key-root lane
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` versus both kept live `092432` and accepted
  `055330`, so this remained a pure runtime-body/code-layout experiment
- representative Callgrind totals versus kept live `092432` would not
  stabilize:
  - first:
    - `numeric_loops`: `255,475,212 -> 255,474,003` (`-1,209`)
    - `dispatch_loops`: `350,049,401 -> 350,028,819` (`-20,582`)
    - `matrix_add_2d`: `12,037,266 -> 12,030,059` (`-7,207`)
    - `map_object_access`: `21,957,884 -> 21,934,636` (`-23,248`)
  - confirm:
    - `numeric_loops`: `255,475,212 -> 255,488,709` (`+13,497`)
    - `dispatch_loops`: `350,049,401 -> 350,024,313` (`-25,088`)
    - `matrix_add_2d`: `12,037,266 -> 12,042,596` (`+5,330`)
    - `map_object_access`: `21,957,884 -> 21,938,757` (`-19,127`)
  - third:
    - `numeric_loops`: `255,475,212 -> 255,497,333` (`+22,121`)
    - `dispatch_loops`: `350,049,401 -> 350,064,332` (`+14,931`)
    - `matrix_add_2d`: `12,037,266 -> 12,032,270` (`-4,996`)
    - `map_object_access`: `21,957,884 -> 21,962,389` (`+4,505`)
- the targeted write-side functions also flipped back above kept live on the
  third rerun:
  - `dispatch_loops`: `object_set_value_core 848,437 -> 849,212`
  - `dispatch_loops`: `ZrCore_String_Equal 501,072 -> 504,832`
  - `dispatch_loops`: `ZrCore_Object_GetValue 199,336 -> 202,028`
- because the shortcut never held a stable retained win and the tie-break rerun
  regressed both representative totals and target write-side bodies, it is not
  worth carrying as another cached-descriptor write-path experiment

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

## 2026-04-22 Numeric Runtime Continuation

This addendum records the kept numeric/runtime follow-ups that happened after
accepted W1 checkpoint `055330`. The accepted W1 baseline is still locked at
`055330`; the slices below only update the live continuation state.

### Rejected compare caller-body known-type conversion

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_compare_known_type_numeric_conversions_20260422-220249`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_compare_known_type_numeric_conversions_confirm_20260422-220730`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c`

Guard coverage kept after revert:

- `tests/core/test_execution_numeric_fast_paths.c`
  - `test_execution_apply_binary_numeric_compare_mixed_uint_and_bool_returns_true`
  - `test_execution_apply_binary_numeric_compare_mixed_float_and_bool_returns_false`

Reason:

- both reruns stayed bit-for-bit identical to live `210253` across all eight
  tracked non-GC `instructions/helpers/slowpaths`
- representative totals versus live `210253` never produced a meaningful kept
  win:
  - first: `numeric +3,951`, `dispatch -6,955`, `matrix -1,444`, `map -2,332`
  - confirm: `numeric +7,135`, `dispatch +153`, `matrix -1,922`, `map +140`
- the tracked instruction mix also showed no generic logical-compare opcodes in
  any of the eight tracked cases, so this was a low-hit runtime-body/code-layout
  probe rather than a real hotspot cut

### Rejected shared float-fallback known-type conversion

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_float_fallback_known_type_numeric_conversions_20260422-222437`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c`

Guard coverage kept after revert:

- `tests/core/test_execution_numeric_fast_paths.c`
  - `test_execution_try_binary_numeric_float_fallback_add_mixed_float_and_bool_returns_double_sum`
  - `test_execution_try_binary_numeric_float_fallback_mod_mixed_uint_and_bool_returns_double_result`

Reason:

- the rerun again stayed bit-for-bit identical to live `210253` across all
  eight tracked non-GC `instructions/helpers/slowpaths`
- representative totals versus live `210253` regressed three of four
  representative cases immediately:
  - `numeric +15,409`, `dispatch +15,815`, `matrix -2,356`, `map +6,441`
- because the change widened the helper body without holding any representative
  win, it was reverted without a confirm rerun

### Retained generic MOD mixed-int use-site known-type conversion

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mod_mixed_int_known_type_numeric_conversions_20260422-223306`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mod_mixed_int_known_type_numeric_conversions_confirm_20260422-223520`

Decision:

- retained as the new live continuation
- not promoted to a new accepted W1 checkpoint

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- the generic `MOD` mixed-int lane now reads the already-known signed/unsigned
  payloads directly instead of paying two `value_to_int64(...)` calls at that
  use site
- both reruns stayed bit-for-bit identical to live `210253` across all eight
  tracked non-GC `instructions/helpers/slowpaths`
- representative totals versus live `210253` held a stable dispatch-side win:
  - first: `numeric +43,006`, `dispatch -176,056`, `matrix +9,068`, `map -3,411`
  - confirm: `numeric +40,417`, `dispatch -152,007`, `matrix +9,711`, `map +4,008`
- relative to accepted `055330`, the confirm rerun stays:
  - `numeric_loops`: `255,474,138 -> 255,515,437` (`+41,299`)
  - `dispatch_loops`: `350,330,462 -> 349,886,874` (`-443,588`)
  - `matrix_add_2d`: `12,053,512 -> 12,052,239` (`-1,273`)
  - `map_object_access`: `21,931,485 -> 21,945,227` (`+13,742`)
- this is therefore worth carrying as the latest live continuation because it
  stably lowers hottest `dispatch_loops`, but it is still not strong enough to
  replace accepted `055330` while `numeric_loops` / `map_object_access` remain
  above accepted

Validation after keep:

- WSL gcc targeted tests pass:
  - `build-wsl-gcc/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang targeted tests also pass:
  - `build-wsl-clang/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
- `tests_generated/performance/` now reflects kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mod_mixed_int_known_type_numeric_conversions_confirm_20260422-223520`

### Retained `GETUPVAL` / `SETUPVAL` dispatch-local `stack_get_value` cut

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_getupval_setupval_stack_get_no_profile_20260422-154828`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_getupval_setupval_stack_get_no_profile_confirm_20260422-155645`

Decision:

- kept in the live tree as the current live continuation checkpoint
- not promoted to a new accepted W1 checkpoint

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Test coverage:

- `tests/core/test_execution_dispatch_callable_metadata.c`
  - `test_getupval_open_capture_path_avoids_stack_get_value_helpers`
  - `test_setupval_open_capture_path_avoids_stack_get_value_helpers`

Reason:

- this cut removes the profiled `stack_get_value` helper from the steady-state
  `GETUPVAL` / `SETUPVAL` path by:
  - reading the current VM closure from `base - 1` through a dispatch-local
    no-profile helper
  - reading open/closed closure values through a dispatch-local no-profile
    helper instead of `ZrCore_ClosureValue_GetValue(...)`
- the tracked non-GC signature versus kept live `092432` only changes in the
  intended target case:
  - `call_chain_polymorphic` helper counts:
    - `stack_get_value`: `5160 -> 1324` (`-3836`)
  - the other seven tracked non-GC cases keep bit-for-bit identical
    `instructions/helpers/slowpaths`
- representative Callgrind totals versus kept live `092432` stayed bounded but
  mixed:
  - first:
    - `numeric_loops`: `255,475,212 -> 255,498,580` (`+23,368`)
    - `dispatch_loops`: `350,049,401 -> 350,047,399` (`-2,002`)
    - `matrix_add_2d`: `12,037,266 -> 12,039,798` (`+2,532`)
    - `map_object_access`: `21,957,884 -> 21,950,237` (`-7,647`)
  - confirm:
    - `numeric_loops`: `255,475,212 -> 255,523,047` (`+47,835`)
    - `dispatch_loops`: `350,049,401 -> 350,053,272` (`+3,871`)
    - `matrix_add_2d`: `12,037,266 -> 12,040,804` (`+3,538`)
    - `map_object_access`: `21,957,884 -> 21,955,286` (`-2,598`)
- unlike the rejected runtime-body probes from the same line, the first and
  confirm reruns stabilize to the same isolated helper-count delta instead of
  flipping between wholly identical tracked signatures
- `call_chain_polymorphic` benchmark wall time also moved down on both reruns:
  - `88.262 ms -> 86.292 ms -> 86.882 ms`
- because this is a real targeted helper-count reduction on the user-priority
  `stack_get_value` line, and the four representative cases only move by a
  bounded amount, it is worth carrying as the new live continuation
- because the confirm rerun still leaves `numeric_loops` / `dispatch_loops` /
  `matrix_add_2d` slightly above kept live `092432`, it still does not justify
  replacing accepted `055330`

Validation after keep:

- `tests_generated/performance/` now reflects kept live confirm:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_getupval_setupval_stack_get_no_profile_confirm_20260422-155645`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`

### Retained mul-only known-type numeric conversions

Snapshots:

- live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_getupval_setupval_stack_get_no_profile_confirm_20260422-155645`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mul_known_type_numeric_conversions_20260422-203205`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mul_known_type_numeric_conversions_confirm_20260422-203533`

Decision:

- kept in the live tree as the current live continuation checkpoint
- not promoted to a new accepted W1 checkpoint

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c`

Test coverage:

- `tests/core/test_execution_numeric_fast_paths.c`
  - `test_execution_try_builtin_mul_mixed_float_and_bool_returns_double_product`

Reason:

- this probe keeps the earlier rejected numeric-helper idea scoped to one
  concrete caller: `execution_try_builtin_mul_mixed_numeric_fast(...)`
- the mixed-float lane now converts through file-local known-type `double`
  extraction, and the mixed integral/bool lane now converts through file-local
  known-type `int64` extraction, instead of routing back through the generic
  `value_to_double(...)` / out-param helper path on every hit
- unlike the already rejected combined `MOD + helper` bundle, both mul-only
  reruns stay bit-for-bit identical to live `155645` across all eight tracked
  non-GC `instructions/helpers/slowpaths`, so this is another bounded
  runtime-body cut rather than a quickening/opcode-mix change
- representative Callgrind totals versus live `155645` stayed mildly positive
  overall:
  - first:
    - `numeric_loops`: `255,523,047 -> 255,497,573` (`-25,474`)
    - `dispatch_loops`: `350,053,272 -> 350,022,410` (`-30,862`)
    - `matrix_add_2d`: `12,040,804 -> 12,053,358` (`+12,554`)
    - `map_object_access`: `21,955,286 -> 21,963,479` (`+8,193`)
  - confirm:
    - `numeric_loops`: `255,523,047 -> 255,487,229` (`-35,818`)
    - `dispatch_loops`: `350,053,272 -> 350,046,793` (`-6,479`)
    - `matrix_add_2d`: `12,040,804 -> 12,044,105` (`+3,301`)
    - `map_object_access`: `21,955,286 -> 21,946,414` (`-8,872`)
- relative to accepted `055330`, the confirm rerun still does not justify
  replacing the accepted W1 checkpoint:
  - `numeric_loops`: `255,474,138 -> 255,487,229` (`+13,091`)
  - `dispatch_loops`: `350,330,462 -> 350,046,793` (`-283,669`)
  - `matrix_add_2d`: `12,053,512 -> 12,044,105` (`-9,407`)
  - `map_object_access`: `21,931,485 -> 21,946,414` (`+14,929`)
- so this slice is worth carrying as the next live continuation on the
  user-priority `value_to_int64` / `execution_try_builtin_mul` line, but not
  worth elevating over the accepted `SET_STACK` checkpoint

Validation after keep:

- `tests_generated/performance/` now reflects kept live confirm:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mul_known_type_numeric_conversions_confirm_20260422-203533`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`

### Retained add caller-body known-type numeric conversions

Snapshots:

- live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mul_known_type_numeric_conversions_confirm_20260422-203533`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_add_known_type_numeric_conversions_20260422-210101`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_add_known_type_numeric_conversions_confirm_20260422-210253`

Decision:

- kept in the live tree as the current live continuation checkpoint
- not promoted to a new accepted W1 checkpoint

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c`

Test coverage:

- `tests/core/test_execution_numeric_fast_paths.c`
  - `test_try_builtin_add_mixed_float_and_bool_returns_double_sum`

Reason:

- this probe applies the same single-caller rule as the kept mul-only cut, but
  only to `try_builtin_add(...)`
- the mixed-float lane now converts through file-local known-type `double`
  extraction, the mixed signed/bool lane converts through file-local known-type
  `int64` extraction, and the unsigned-only lane directly sums the already
  known `nativeUInt64` payloads
- both reruns again stay bit-for-bit identical to live `203533` across all
  eight tracked non-GC `instructions/helpers/slowpaths`, so this is another
  bounded runtime-body cut rather than a quickening/opcode-mix change
- representative Callgrind totals versus live `203533` stayed positive:
  - first:
    - `numeric_loops`: `255,487,229 -> 255,487,619` (`+390`)
    - `dispatch_loops`: `350,046,793 -> 350,035,926` (`-10,867`)
    - `matrix_add_2d`: `12,044,105 -> 12,044,271` (`+166`)
    - `map_object_access`: `21,946,414 -> 21,943,106` (`-3,308`)
  - confirm:
    - `numeric_loops`: `255,487,229 -> 255,475,020` (`-12,209`)
    - `dispatch_loops`: `350,046,793 -> 350,038,881` (`-7,912`)
    - `matrix_add_2d`: `12,044,105 -> 12,042,528` (`-1,577`)
    - `map_object_access`: `21,946,414 -> 21,941,219` (`-5,195`)
- relative to accepted `055330`, the confirm rerun still does not justify
  replacing the accepted W1 checkpoint:
  - `numeric_loops`: `255,474,138 -> 255,475,020` (`+882`)
  - `dispatch_loops`: `350,330,462 -> 350,038,881` (`-291,581`)
  - `matrix_add_2d`: `12,053,512 -> 12,042,528` (`-10,984`)
  - `map_object_access`: `21,931,485 -> 21,941,219` (`+9,734`)
- so this slice is worth carrying on top of the kept mul-only cut, but it still
  does not unseat the accepted `SET_STACK` checkpoint

Validation after keep:

- `tests_generated/performance/` now reflects kept live confirm:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_add_known_type_numeric_conversions_confirm_20260422-210253`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`

### Rejected store-time receiver own-lookup dedup

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_store_receiver_own_lookup_dedup_20260422-143856`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Reason:

- this probe deduped repeated receiver own-string-pair-by-name lookups inside
  `execution_member_store_pic_slot(...)` for descriptorless/callable/instance-field
  store refresh
- the tracked non-GC `instructions/helpers/slowpaths` signature stayed
  bit-for-bit identical versus kept live `092432`, so this was again only a
  runtime-body/code-layout experiment
- representative Callgrind totals versus kept live `092432` did not justify
  keeping it:
  - `numeric_loops`: `255,475,212 -> 255,496,352` (`+21,140`)
  - `dispatch_loops`: `350,049,401 -> 350,040,465` (`-8,936`)
  - `matrix_add_2d`: `12,037,266 -> 12,049,345` (`+12,079`)
  - `map_object_access`: `21,957,884 -> 21,960,841` (`+2,957`)
- the tiny `dispatch_loops` win was outweighed by regressions on the other
  three representative cases, so this did not clear the retained-win bar

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

### Rejected SET_STACK none-lane direct slow-copy fallback

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_stack_none_slow_direct_copy_20260422-151245`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/include/zr_vm_core/value.h`

Reason:

- this probe changed
  `ZrCore_Value_AssignMaterializedStackValueNoProfile(...)` so the
  none-ownership fallback went straight to `ZrCore_Value_CopySlow(...)`
  instead of `ZrCore_Value_CopyNoProfile(...)`
- the tracked non-GC `instructions/helpers/slowpaths` signature again stayed
  bit-for-bit identical versus kept live `092432`, so the change was pure
  runtime-body/code-layout churn rather than a semantic or quickening shift
- representative totals versus kept live `092432` regressed broadly:
  - `numeric_loops`: `255,475,212 -> 255,982,545` (`+507,333`)
  - `dispatch_loops`: `350,049,401 -> 350,821,451` (`+772,050`)
  - `matrix_add_2d`: `12,037,266 -> 12,055,781` (`+18,515`)
  - `map_object_access`: `21,957,884 -> 21,970,292` (`+12,408`)
- this is therefore a clear reject; the live tree should keep the retained
  none-ownership fast lane from accepted `055330` / live continuation `080005`
  without this direct slow-copy shortcut

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

### Rejected refresh-side owner/descriptor hint reuse

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_refresh_cached_owner_descriptor_hint_20260422-134012`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_refresh_cached_owner_descriptor_hint_confirm_20260422-134336`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Temporary TDD coverage used during the probe was reverted with the production change.

Reason:

- this probe let `execution_member_refresh_cache(...)` try to reuse owner /
  descriptor metadata already present in the callsite PIC entry, or a
  previously found descriptorless callable owner, instead of rescanning the
  receiver prototype chain from scratch on every refresh
- both reruns stayed bit-for-bit identical in tracked
  `instructions/helpers/slowpaths` versus both kept live `092432` and accepted
  `055330`, so this remained another pure runtime-body/code-layout experiment
- representative Callgrind totals versus kept live `092432` did not hold:
  - first:
    - `numeric_loops`: `255,475,212 -> 255,507,160` (`+31,948`)
    - `dispatch_loops`: `350,049,401 -> 350,026,866` (`-22,535`)
    - `matrix_add_2d`: `12,037,266 -> 12,067,647` (`+30,381`)
    - `map_object_access`: `21,957,884 -> 21,928,552` (`-29,332`)
  - confirm:
    - `numeric_loops`: `255,475,212 -> 255,539,091` (`+63,879`)
    - `dispatch_loops`: `350,049,401 -> 350,027,010` (`-22,391`)
    - `matrix_add_2d`: `12,037,266 -> 12,044,729` (`+7,463`)
    - `map_object_access`: `21,957,884 -> 21,948,582` (`-9,302`)
- confirm function-level movement in `map_object_access` regressed the target
  write/string bodies overall:
  - `object_set_value_core`: `474,095 -> 476,576`
  - `ZrCore_String_Equal`: `108,096 -> 110,232`
  - `ZrCore_Object_GetValue`: `108,300 -> 108,119`
- because the refresh-side metadata reuse kept the same tracked signature but
  could not hold `numeric_loops` / `matrix_add_2d` below live and still pushed
  the target write/string bodies the wrong way on confirm, it is not worth
  carrying in the live tree

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

### Rejected namescan pointer-equality guard

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_namescan_pointer_guard_20260422-135416`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_namescan_pointer_guard_confirm_20260422-135536`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe added pointer-equality guards before selected
  `ZrCore_String_Equal(...)` calls inside descriptor/name scans so same-pointer
  names could early-return without paying the full string compare body
- both reruns again stayed bit-for-bit identical in tracked
  `instructions/helpers/slowpaths` versus both kept live `092432` and accepted
  `055330`, so this was again a pure runtime-body/code-layout experiment
- representative Callgrind totals versus kept live `092432` stayed mixed:
  - first:
    - `numeric_loops`: `255,475,212 -> 255,485,988` (`+10,776`)
    - `dispatch_loops`: `350,049,401 -> 350,028,875` (`-20,526`)
    - `matrix_add_2d`: `12,037,266 -> 12,045,113` (`+7,847`)
    - `map_object_access`: `21,957,884 -> 21,969,421` (`+11,537`)
  - confirm:
    - `numeric_loops`: `255,475,212 -> 255,499,129` (`+23,917`)
    - `dispatch_loops`: `350,049,401 -> 350,029,851` (`-19,550`)
    - `matrix_add_2d`: `12,037,266 -> 12,050,138` (`+12,872`)
    - `map_object_access`: `21,957,884 -> 21,948,918` (`-8,966`)
- the probe kept flipping between `map_object_access` and
  `numeric_loops` / `matrix_add_2d` regressions on the same tracked signature,
  so the pointer guard did not translate into a stable retained win

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
- `tests_generated/performance/` was restored to the kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct GC smoke also passes on the source benchmark project:
  - `./build/benchmark-gcc-release/bin/zr_vm_cli ./tests/benchmarks/cases/gc_fragment_stress/zr/benchmark_gc_fragment_stress.zrp`
  - output:
    - `BENCH_GC_FRAGMENT_STRESS_PASS`
    - checksum `857265678`

## Post-`055330` Rejected Follow-Up Probes

These follow-ups were all measured against the accepted/live W1 tracked
checkpoint:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_stack_materialized_none_fast_lane_confirm_20260422-055330`

All four again stayed bit-for-bit identical in
`instructions/helpers/slowpaths` across:

- `numeric_loops`
- `dispatch_loops`
- `container_pipeline`
- `matrix_add_2d`
- `map_object_access`
- `string_build`
- `call_chain_polymorphic`
- `mixed_service_loop`

So each result below is another pure runtime-body/code-layout probe, not a
quickening or opcode-mix change.

### Rejected member-get read-copy none fast lane

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_read_copy_none_fast_lane_20260422-061831`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_read_copy_none_fast_lane_confirm_20260422-062256`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Test coverage kept in tree:

- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- this probe tried to narrow the extra stable result-copy work on the
  `execution_member_get_by_name(...)` / `execution_member_get_cached(...)`
  hot read lane
- first rerun vs accepted `055330`:
  - `numeric_loops`: `255,474,138 -> 255,503,497` (`+29,359`)
  - `dispatch_loops`: `350,330,462 -> 350,338,997` (`+8,535`)
  - `matrix_add_2d`: `12,053,512 -> 12,066,828` (`+13,316`)
  - `map_object_access`: `21,931,485 -> 21,938,124` (`+6,639`)
- confirm rerun still did not hold a retained win:
  - `numeric_loops`: `255,474,138 -> 255,499,289` (`+25,151`)
  - `dispatch_loops`: `350,330,462 -> 350,357,059` (`+26,597`)
  - `matrix_add_2d`: `12,053,512 -> 12,041,361` (`-12,151`)
  - `map_object_access`: `21,931,485 -> 21,917,478` (`-14,007`)
- since the hottest two representative cases stayed worse on the confirm rerun,
  this cut was removed from the live tree

### Rejected cached string-pair-by-name pointer-hit

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_string_pair_name_pointer_hit_20260422-063216`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_string_pair_name_pointer_hit_confirm_20260422-063324`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

Reason:

- this probe added a cached string-pair-by-name pointer-hit fast path below the
  cached member/object lookup line
- first rerun vs accepted `055330`:
  - `numeric_loops`: `255,474,138 -> 255,512,148` (`+38,010`)
  - `dispatch_loops`: `350,330,462 -> 350,378,775` (`+48,313`)
  - `matrix_add_2d`: `12,053,512 -> 12,041,917` (`-11,595`)
  - `map_object_access`: `21,931,485 -> 21,927,529` (`-3,956`)
- confirm rerun stayed mixed in the wrong direction:
  - `numeric_loops`: `255,474,138 -> 255,485,575` (`+11,437`)
  - `dispatch_loops`: `350,330,462 -> 350,350,629` (`+20,167`)
  - `matrix_add_2d`: `12,053,512 -> 12,053,207` (`-305`)
  - `map_object_access`: `21,931,485 -> 21,914,165` (`-17,320`)
- confirm function movement in `dispatch_loops` vs accepted `055330` also
  showed the target shared bodies getting worse:
  - `ZrCore_String_Equal`: `495,448 -> 501,252`
  - `object_set_value_core`: `843,557 -> 848,580`
  - `ZrCore_Object_GetValue`: `197,043 -> 198,531`
- the production change was therefore removed from the live tree

### Rejected cached receiver-prototype reuse

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_receiver_prototype_reuse_20260422-064226`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_receiver_prototype_reuse_confirm_20260422-064345`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Reason:

- this probe reused the already-resolved `receiverObject` to derive
  `receiverPrototype` inside cached get/set, instead of resolving prototype
  from `receiver` again
- first rerun vs accepted `055330`:
  - `numeric_loops`: `255,474,138 -> 255,487,931` (`+13,793`)
  - `dispatch_loops`: `350,330,462 -> 350,333,455` (`+2,993`)
  - `matrix_add_2d`: `12,053,512 -> 12,029,952` (`-23,560`)
  - `map_object_access`: `21,931,485 -> 21,912,514` (`-18,971`)
- confirm rerun regressed three of the four representative cases:
  - `numeric_loops`: `255,474,138 -> 255,533,142` (`+59,004`)
  - `dispatch_loops`: `350,330,462 -> 350,338,622` (`+8,160`)
  - `matrix_add_2d`: `12,053,512 -> 12,061,401` (`+7,889`)
  - `map_object_access`: `21,931,485 -> 21,926,329` (`-5,156`)
- confirm function movement in `dispatch_loops` vs accepted `055330` again
  stayed directionally wrong:
  - `object_set_value_core`: `843,557 -> 844,525`
  - `ZrCore_Object_GetValue`: `197,043 -> 198,123`
  - `ZrCore_String_Equal`: `495,448 -> 497,704`
- this helper was therefore removed as another pure runtime-body regression

### Rejected cached descriptor/callable receiver-object reuse

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_callable_receiver_object_reuse_20260422-070123`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_callable_receiver_object_reuse_confirm_20260422-070250`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

Reason:

- this probe threaded the already-resolved `receiverObject` through cached
  callable/descriptor get/set wrappers so those lanes could skip re-resolving
  the same object on the slow path
- first rerun vs accepted `055330` was mixed from the start:
  - `numeric_loops`: `255,474,138 -> 255,473,666` (`-472`)
  - `dispatch_loops`: `350,330,462 -> 350,333,152` (`+2,690`)
  - `matrix_add_2d`: `12,053,512 -> 12,051,209` (`-2,303`)
  - `map_object_access`: `21,931,485 -> 21,916,240` (`-15,245`)
- confirm rerun flipped back on the hottest shared-runtime line:
  - `numeric_loops`: `255,474,138 -> 255,525,451` (`+51,313`)
  - `dispatch_loops`: `350,330,462 -> 350,326,143` (`-4,319`)
  - `matrix_add_2d`: `12,053,512 -> 12,047,024` (`-6,488`)
  - `map_object_access`: `21,931,485 -> 21,937,689` (`+6,204`)
- because the tracked opcode/helper signature stayed identical while the confirm
  rerun regressed `numeric_loops` and `map_object_access`, this was another
  pure runtime-body/code-layout regression and was removed from the live tree

### Rejected cached slow-lane pointer use-site hits

Snapshots:

- fresh live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_20260422-071738`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_slow_lane_pointer_use_site_hits_20260422-072758`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_slow_lane_pointer_use_site_hits_confirm_20260422-072958`
- restored live baseline after revert:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_cached_slow_lane_pointer_use_site_hits_revert_20260422-073554`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

Reason:

- this probe deliberately stayed narrower than the earlier rejected generic
  pointer-hit/body-layout attempts: it only front-loaded exact pointer hits at
  specific cached slow-lane use sites
- the touched lanes were:
  - exact cached object/member-name get/set
  - cached callable target lookup
  - cached callable wrapper own-member shadow check
  - cached descriptor wrapper own-member shadow check
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths`, so this was still another pure
  runtime-body/code-layout experiment
- first rerun vs fresh live baseline `071738` was mixed:
  - `numeric_loops`: `255,487,980 -> 255,483,499` (`-4,481`)
  - `dispatch_loops`: `350,349,249 -> 350,336,653` (`-12,596`)
  - `matrix_add_2d`: `12,039,993 -> 12,053,372` (`+13,379`)
  - `map_object_access`: `21,903,617 -> 21,924,185` (`+20,568`)
- first rerun vs accepted `055330` was also only near-neutral:
  - `numeric_loops`: `255,474,138 -> 255,483,499` (`+9,361`)
  - `dispatch_loops`: `350,330,462 -> 350,336,653` (`+6,191`)
  - `matrix_add_2d`: `12,053,512 -> 12,053,372` (`-140`)
  - `map_object_access`: `21,931,485 -> 21,924,185` (`-7,300`)
- confirm rerun did not hold a retained win:
  - vs fresh live baseline `071738`:
    - `numeric_loops`: `255,487,980 -> 255,488,490` (`+510`)
    - `dispatch_loops`: `350,349,249 -> 350,334,273` (`-14,976`)
    - `matrix_add_2d`: `12,039,993 -> 12,061,032` (`+21,039`)
    - `map_object_access`: `21,903,617 -> 21,914,950` (`+11,333`)
  - vs accepted `055330`:
    - `numeric_loops`: `255,474,138 -> 255,488,490` (`+14,352`)
    - `dispatch_loops`: `350,330,462 -> 350,334,273` (`+3,811`)
    - `matrix_add_2d`: `12,053,512 -> 12,061,032` (`+7,520`)
    - `map_object_access`: `21,931,485 -> 21,914,950` (`-16,535`)
- the function-level direction explains the mixed aggregate:
  - `dispatch_loops` vs `071738` confirm rerun:
    - `object_set_value_core`: `845,459 -> 845,240`
    - `ZrCore_String_Equal`: `499,664 -> 497,184`
    - `ZrCore_Object_GetValue`: `199,646 -> 196,968`
  - `map_object_access` vs `071738` confirm rerun:
    - `object_set_value_core`: `472,766 -> 475,435`
    - `ZrCore_String_Equal`: `106,024 -> 108,576`
    - `ZrCore_Object_GetValue`: `106,751 -> 106,768`
- so this cut improved the targeted dispatch-side shared bodies a little, but
  paid more back in `map_object_access`/layout cost than it saved; it was
  removed from the live tree instead of being retained as another noisy
  continuation

Validation after the last revert:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

The fresh live rebaseline after this revert is now:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_cached_slow_lane_pointer_use_site_hits_revert_20260422-073554`

That restored snapshot stays bit-for-bit identical in tracked
`instructions/helpers/slowpaths` relative to `071738`, and only shows normal
rerun drift:

- `numeric_loops`: `255,487,980 -> 255,498,573` (`+10,593`)
- `dispatch_loops`: `350,349,249 -> 350,350,319` (`+1,070`)
- `matrix_add_2d`: `12,039,993 -> 12,043,074` (`+3,081`)
- `map_object_access`: `21,903,617 -> 21,920,536` (`+16,919`)

So after these five post-`055330` rejects:

- the accepted W1 tracked checkpoint still remains
  `performance_profile_tracked_non_gc_after_set_stack_materialized_none_fast_lane_confirm_20260422-055330`
- the generated `tests_generated/performance/` directory is no longer stale; it
  again reflects the reverted live tree
- the next W1 step should stay below cached get/set wrapper names and continue
  on the lower shared runtime bodies they feed, rather than adding more
  pointer-only pre-probes at the same cached slow-lane use sites

### Retained value-copy direct none-ownership fast lane

Snapshots:

- fresh live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_cached_slow_lane_pointer_use_site_hits_revert_20260422-073554`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_value_copy_direct_none_ownership_fast_lane_20260422-075702`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_value_copy_direct_none_ownership_fast_lane_confirm_20260422-080005`

Decision:

- retained as the new live-tree continuation checkpoint
- not promoted to a new accepted W1 tracked checkpoint

Production change:

- `zr_vm_core/include/zr_vm_core/value.h`

Test coverage kept in tree:

- `tests/core/test_value_copy_fast_paths.c`
  - `test_value_try_copy_fast_rejects_null_heap_object_payload`

Reason:

- this probe narrows `ZrCore_Value_TryCopyFastNoProfile(...)` to direct
  normalized `ownershipKind == NONE` checks and mirrors the same common
  none-ownership early-return lane inside
  `ZrCore_Value_AssignMaterializedStackValueNoProfile(...)`
- it intentionally does not broaden the plain-heap-object fast-copy reach; it
  only removes repeated helper/body work on the already-accepted fast-copy /
  materialized-stack lane
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to both live `073554` and accepted
  `055330`, so this is still a pure runtime-body/code-layout cut rather than a
  quickening / opcode-mix shift
- first rerun vs live `073554`:
  - `numeric_loops`: `255,498,573 -> 255,509,426` (`+10,853`)
  - `dispatch_loops`: `350,350,319 -> 350,051,270` (`-299,049`)
  - `matrix_add_2d`: `12,043,074 -> 12,047,506` (`+4,432`)
  - `map_object_access`: `21,920,536 -> 21,950,497` (`+29,961`)
- confirm rerun vs live `073554`:
  - `numeric_loops`: `255,498,573 -> 255,480,646` (`-17,927`)
  - `dispatch_loops`: `350,350,319 -> 350,028,074` (`-322,245`)
  - `matrix_add_2d`: `12,043,074 -> 12,053,438` (`+10,364`)
  - `map_object_access`: `21,920,536 -> 21,939,329` (`+18,793`)
- confirm rerun vs accepted `055330`:
  - `numeric_loops`: `255,474,138 -> 255,480,646` (`+6,508`)
  - `dispatch_loops`: `350,330,462 -> 350,028,074` (`-302,388`)
  - `matrix_add_2d`: `12,053,512 -> 12,053,438` (`-74`)
  - `map_object_access`: `21,931,485 -> 21,939,329` (`+7,844`)
- confirm function-level movement shows the retained win still lives in the
  interpreter dispatch body instead of another helper-mix change:
  - `dispatch_loops` vs `055330`:
    - `ZrCore_Execute`: `333,029,805 -> 332,722,377` (`-307,428`)
    - `object_set_value_core`: `843,557 -> 845,072` (`+1,515`)
    - `ZrCore_String_Equal`: `495,448 -> 496,608` (`+1,160`)
    - `ZrCore_Object_GetValue`: `197,043 -> 197,984` (`+941`)
  - `numeric_loops` vs `055330`:
    - `ZrCore_Execute`: unchanged at `242,944,358`
    - `object_set_value_core`: `830,508 -> 832,159` (`+1,651`)
    - `ZrCore_String_Equal`: `114,012 -> 115,012` (`+1,000`)
  - `map_object_access` vs `055330`:
    - `ZrCore_Execute`: `9,011,589 -> 8,993,751` (`-17,838`)
    - `ZrCore_String_Equal`: `108,272 -> 107,696` (`-576`)
- because the confirm rerun still holds a large `dispatch_loops` reduction and
  stays net-below both `073554` and `055330` across the four representative
  cases, the probe is worth keeping in the live tree
- because `numeric_loops` and `map_object_access` remain slightly above accepted
  `055330`, this stays a live continuation / W1-T7/W1-T8 evidence cut instead
  of a replacement accepted baseline

Validation after keep:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
- WSL gcc tracked non-GC benchmark reruns pass twice:
  - `cmake --build build/benchmark-gcc-release --target run_performance_suite --parallel 1`
    with:
  - `ZR_VM_TEST_TIER=profile`
    - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
    - `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp`
    - `ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop`

### Rejected multi-slot exact-receiver pair sibling backfill

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_exact_receiver_pair_broadcast_20260422-100623`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_exact_receiver_pair_broadcast_confirm_20260422-101128`
- third rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_exact_receiver_pair_broadcast_rerun_20260422-101251`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Temporary test coverage removed with revert:

- `tests/core/test_execution_member_access_fast_paths.c`
  - `test_member_get_cached_multi_slot_exact_receiver_object_backfills_exact_pair_to_empty_sibling_slot`
  - `test_member_set_cached_multi_slot_exact_receiver_object_backfills_exact_pair_to_empty_sibling_slot`

Reason:

- this probe tried to let a multi-slot cached instance-field object/member-name
  hit backfill the resolved exact receiver pair into sibling instance-field PIC
  slots that still had no exact receiver binding, so later passes could reach an
  earlier exact-pair hit without reopening `function->memberEntries`
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` versus both kept live `092432` and accepted
  `055330`, so the probe never changed opcode mix or helper counts
- representative Callgrind versus kept live `092432` did not stabilize:
  - first:
    - `numeric_loops`: `255,475,212 -> 255,493,365` (`+18,153`)
    - `dispatch_loops`: `350,049,401 -> 350,050,738` (`+1,337`)
    - `matrix_add_2d`: `12,037,266 -> 12,052,830` (`+15,564`)
    - `map_object_access`: `21,957,884 -> 21,935,246` (`-22,638`)
  - confirm:
    - `numeric_loops`: `255,475,212 -> 255,481,997` (`+6,785`)
    - `dispatch_loops`: `350,049,401 -> 350,034,434` (`-14,967`)
    - `matrix_add_2d`: `12,037,266 -> 12,049,297` (`+12,031`)
    - `map_object_access`: `21,957,884 -> 21,934,979` (`-22,905`)
  - third:
    - `numeric_loops`: `255,475,212 -> 255,496,498` (`+21,286`)
    - `dispatch_loops`: `350,049,401 -> 350,026,650` (`-22,751`)
    - `matrix_add_2d`: `12,037,266 -> 12,046,670` (`+9,404`)
    - `map_object_access`: `21,957,884 -> 21,960,849` (`+2,965`)
- the confirm rerun looked directionally reasonable on the targeted read/write
  bodies:
  - `dispatch_loops`: `object_set_value_core 848,437 -> 846,734`, `ZrCore_String_Equal 501,072 -> 497,792`, `ZrCore_Object_GetValue 199,336 -> 196,766`
  - `map_object_access`: `object_set_value_core 474,095 -> 473,882`, `ZrCore_String_Equal 108,096 -> 107,320`, `ZrCore_Object_GetValue 108,300 -> 107,648`
- but the third rerun flipped `map_object_access` back above live, including the
  main write/string bodies:
  - `object_set_value_core 474,095 -> 475,352`
  - `ZrCore_String_Equal 108,096 -> 109,200`
  - `ZrCore_Object_GetValue 108,300 -> 108,107`
- because the probe added more cache-state churn than it consistently paid
  back, it is rejected and the live tree stays on the retained
  `multi_slot_cached_member_name_broadcast` continuation instead

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

### Rejected existing-pair after-fast-miss normalized-none gate

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_existing_pair_after_fast_miss_normalized_none_gate_20260422-105756`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_existing_pair_after_fast_miss_normalized_none_gate_confirm_20260422-110217`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe narrowed `ZrCore_Object_SetExistingPairValueAfterFastMissUnchecked(...)`
  to the existing normalized-none helper so the dominant none-ownership update
  lane would check only `ownershipKind == NONE` instead of re-checking the
  normalized null ownership fields
- the tracked non-GC `instructions/helpers/slowpaths` signature again stayed
  bit-for-bit identical versus both kept live `092432` and accepted `055330`, so
  this was another pure runtime-body/code-layout experiment
- first rerun versus kept live `092432` was already mixed:
  - `numeric_loops`: `255,475,212 -> 255,480,951` (`+5,739`)
  - `dispatch_loops`: `350,049,401 -> 350,031,254` (`-18,147`)
  - `matrix_add_2d`: `12,037,266 -> 12,041,806` (`+4,540`)
  - `map_object_access`: `21,957,884 -> 21,966,850` (`+8,966`)
- confirm rerun then regressed all four representative totals versus kept live:
  - `numeric_loops`: `255,475,212 -> 255,494,338` (`+19,126`)
  - `dispatch_loops`: `350,049,401 -> 350,059,171` (`+9,770`)
  - `matrix_add_2d`: `12,037,266 -> 12,048,691` (`+11,425`)
  - `map_object_access`: `21,957,884 -> 21,974,397` (`+16,513`)
- the first rerun briefly improved the target `dispatch_loops` bodies:
  - `object_set_value_core`: `848,437 -> 846,001`
  - `ZrCore_String_Equal`: `501,072 -> 497,264`
  - `ZrCore_Object_GetValue`: `199,336 -> 197,414`
- but the same probe simultaneously pushed the write/read/string bodies the
  wrong way in `map_object_access`, and the confirm rerun lost even the
  `dispatch_loops` advantage:
  - `dispatch_loops`: `object_set_value_core 848,437 -> 847,755`, `ZrCore_String_Equal 501,072 -> 500,752`, `ZrCore_Object_GetValue 199,336 -> 199,581`
  - `map_object_access`: `object_set_value_core 474,095 -> 477,857`, `ZrCore_String_Equal 108,096 -> 112,064`, `ZrCore_Object_GetValue 108,300 -> 108,881`
- because the confirm rerun is broad regression on the same tracked signature,
  this helper-shrink does not justify another retained live cut

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

### Rejected object-read copy `TryCopyFastNoProfile` lane

Snapshots:

- live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_value_copy_direct_none_ownership_fast_lane_confirm_20260422-080005`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_object_read_copy_try_copy_fast_lane_20260422-081923`
- confirm rerun:
  - transient rerun on the same tree before revert; no standalone snapshot was kept because the probe was immediately rolled back and rebaselined
- restored live baseline after revert:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_object_read_copy_try_copy_fast_lane_revert_20260422-082401`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Test coverage kept in tree:

- `tests/core/test_execution_member_access_fast_paths.c`
  - `test_object_get_member_cached_descriptor_plain_heap_object_reuses_original_object`
  - `test_object_get_member_cached_descriptor_struct_object_still_clones_result`

Reason:

- this probe replaced `object_copy_value_profiled(...)`'s
  `ZrCore_Value_CopyNoProfile(...)` wrapper call with direct
  `ZrCore_Value_TryCopyFastNoProfile(...) -> ZrCore_Value_CopySlow(...)` in the
  shared object read path used by:
  - `ZrCore_Object_GetValue(...)`
  - `ZrCore_Object_GetMemberWithKeyUnchecked(...)`
  - `ZrCore_Object_TryGetMemberWithKeyFastUnchecked(...)`
  - cached callable / descriptor reads
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to both live `080005` and accepted
  `055330`, so this was another pure runtime-body/code-layout experiment
- first rerun vs live `080005` was mildly positive:
  - `numeric_loops`: `255,480,646 -> 255,484,988` (`+4,342`)
  - `dispatch_loops`: `350,028,074 -> 350,015,600` (`-12,474`)
  - `matrix_add_2d`: `12,053,438 -> 12,050,419` (`-3,019`)
  - `map_object_access`: `21,939,329 -> 21,927,513` (`-11,816`)
- but the confirm rerun flipped that direction and lost the net win versus the
  current live tree:
  - vs live `080005`:
    - `numeric_loops`: `255,480,646 -> 255,483,533` (`+2,887`)
    - `dispatch_loops`: `350,028,074 -> 350,012,575` (`-15,499`)
    - `matrix_add_2d`: `12,053,438 -> 12,071,765` (`+18,327`)
    - `map_object_access`: `21,939,329 -> 21,944,811` (`+5,482`)
  - vs accepted `055330`:
    - `numeric_loops`: `255,474,138 -> 255,483,533` (`+9,395`)
    - `dispatch_loops`: `350,330,462 -> 350,012,575` (`-317,887`)
    - `matrix_add_2d`: `12,053,512 -> 12,071,765` (`+18,253`)
    - `map_object_access`: `21,931,485 -> 21,944,811` (`+13,326`)
- confirm function-level movement versus live `080005` explains why it was
  rejected:
  - `dispatch_loops`:
    - `object_set_value_core`: `845,072 -> 844,100`
    - `ZrCore_String_Equal`: `496,608 -> 493,000`
    - `ZrCore_Object_GetValue`: `197,984 -> 195,000`
  - `matrix_add_2d`:
    - `object_set_value_core`: `474,927 -> 475,808`
    - `ZrCore_String_Equal`: `162,408 -> 163,808`
    - `ZrCore_Object_GetValue`: `111,335 -> 111,871`
  - `map_object_access`:
    - `object_set_value_core`: `2,144 -> 2,112`
    - `ZrCore_String_Equal`: `107,696 -> 109,576`
    - `ZrCore_Object_GetValue`: `107,833 -> 108,295`
- so the first rerun's read-side win on `dispatch_loops` / `map_object_access`
  did not survive the confirm pass; once `matrix_add_2d` and
  `map_object_access` drifted back up, the probe no longer paid for itself on
  the current live tree

Validation after revert:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
- restored live rebaseline after revert again stays bit-for-bit identical in
  tracked `instructions/helpers/slowpaths` relative to `080005`, with normal
  rerun drift:
  - `numeric_loops`: `255,480,646 -> 255,492,537` (`+11,891`)
  - `dispatch_loops`: `350,028,074 -> 350,060,516` (`+32,442`)
  - `matrix_add_2d`: `12,053,438 -> 12,044,082` (`-9,356`)
  - `map_object_access`: `21,939,329 -> 21,946,704` (`+7,375`)

### Rejected cached descriptor by-name fallback / lazy member-key probe

Snapshots:

- live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_object_read_copy_try_copy_fast_lane_revert_20260422-082401`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_by_name_fallback_20260422-084010`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_by_name_fallback_confirm_20260422-084232`
- third rerun used to break the tie:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_by_name_fallback_rerun_20260422-084503`
- restored live baseline after revert:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_cached_descriptor_by_name_fallback_revert_20260422-084826`

Decision:

- rejected and reverted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

Test coverage kept in tree:

- `tests/core/test_execution_member_access_fast_paths.c`
  - `test_object_get_member_cached_descriptor_prototype_plain_heap_object_reuses_original_object`
  - `test_object_get_member_cached_descriptor_prototype_struct_object_still_clones_result`

Reason:

- this probe tried to keep cached descriptor fallback fully on by-name helpers:
  - `Get` side walked prototype storage by descriptor name instead of materializing
    a temporary `memberKey`
  - `Set` side used by-name pair lookups and deferred `memberKey` materialization
    until an actual write path was unavoidable
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` relative to both live `082401` and accepted
  `055330`, so this was another pure runtime-body/code-layout experiment
- first rerun versus live `082401` was mixed:
  - `numeric_loops`: `255,492,537 -> 255,517,713` (`+25,176`)
  - `dispatch_loops`: `350,060,516 -> 350,040,498` (`-20,018`)
  - `matrix_add_2d`: `12,044,082 -> 12,052,961` (`+8,879`)
  - `map_object_access`: `21,946,704 -> 21,939,282` (`-7,422`)
- confirm rerun flipped different representative cases:
  - `numeric_loops`: `255,492,537 -> 255,479,402` (`-13,135`)
  - `dispatch_loops`: `350,060,516 -> 350,049,512` (`-11,004`)
  - `matrix_add_2d`: `12,044,082 -> 12,051,596` (`+7,514`)
  - `map_object_access`: `21,946,704 -> 21,947,691` (`+987`)
- the third rerun confirmed the regression stayed unstable instead of converging:
  - `numeric_loops`: `255,492,537 -> 255,465,054` (`-27,483`)
  - `dispatch_loops`: `350,060,516 -> 350,053,778` (`-6,738`)
  - `matrix_add_2d`: `12,044,082 -> 12,073,451` (`+29,369`)
  - `map_object_access`: `21,946,704 -> 21,955,097` (`+8,393`)
- the targeted bodies in representative read/write cases only moved by small
  amounts, which is exactly why this stayed a layout-only mixed probe instead
  of a retained runtime win:
  - `dispatch_loops` confirm versus live `082401`:
    - `object_set_value_core`: `849,551 -> 847,725`
    - `ZrCore_String_Equal`: `503,168 -> 501,664`
    - `ZrCore_Object_GetValue`: `200,539 -> 200,394`
  - `map_object_access` confirm versus live `082401`:
    - `object_set_value_core`: `476,179 -> 476,024`
    - `ZrCore_String_Equal`: `109,984 -> 109,872`
    - `ZrCore_Object_GetValue`: `108,085 -> 108,380`
- because the first/confirm/third reruns never held a stable representative win
  on the same signature, the probe is rejected and the live tree stays on the
  retained `080005` continuation instead of carrying another ambiguous body-cut

Validation after revert:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
- restored live rebaseline after revert again stays bit-for-bit identical in
  tracked `instructions/helpers/slowpaths` relative to `082401`, with normal
  rerun drift:
  - `numeric_loops`: `255,492,537 -> 255,503,392` (`+10,855`)
  - `dispatch_loops`: `350,060,516 -> 350,051,148` (`-9,368`)
  - `matrix_add_2d`: `12,044,082 -> 12,061,441` (`+17,359`)
  - `map_object_access`: `21,946,704 -> 21,942,568` (`-4,136`)

### Retained multi-slot cached member-name sibling backfill

Snapshots:

- live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_cached_descriptor_by_name_fallback_revert_20260422-084826`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_20260422-091519`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_confirm_20260422-092256`
- third rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`

Decision:

- kept in the live tree as a bounded cached-get/set continuation / correctness cleanup
- not promoted to a new accepted W1 checkpoint

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Test coverage:

- `tests/core/test_execution_member_access_fast_paths.c`
  - `test_member_get_cached_multi_slot_exact_receiver_object_backfills_cached_member_name_to_sibling_slot`
  - `test_member_set_cached_multi_slot_exact_receiver_object_backfills_cached_member_name_to_sibling_slot`

Reason:

- this probe makes multi-slot cached instance-field get/set broadcast a resolved
  `cachedMemberName` across sibling PIC slots once any slot has to materialize
  the callsite member name
- the new guards prove the intended retained behavior:
  - if one exact-receiver sibling slot resolves the member name first, another
    exact slot that only has `cachedReceiverObject` can still keep hitting after
    `function->memberEntries` are removed
- all eight tracked non-GC cases stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` versus both live `084826` and accepted
  `055330`, so this is still a pure runtime-body/code-layout cut, not a
  quickening or opcode-mix change
- representative Callgrind totals versus live `084826` stayed mixed but tighter
  than the rejected probes:
  - first:
    - `numeric_loops`: `255,503,392 -> 255,489,331` (`-14,061`)
    - `dispatch_loops`: `350,051,148 -> 350,014,633` (`-36,515`)
    - `matrix_add_2d`: `12,061,441 -> 12,037,819` (`-23,622`)
    - `map_object_access`: `21,942,568 -> 21,937,931` (`-4,637`)
  - confirm:
    - `numeric_loops`: `255,503,392 -> 255,478,850` (`-24,542`)
    - `dispatch_loops`: `350,051,148 -> 350,058,542` (`+7,394`)
    - `matrix_add_2d`: `12,061,441 -> 12,045,369` (`-16,072`)
    - `map_object_access`: `21,942,568 -> 21,949,878` (`+7,310`)
  - third:
    - `numeric_loops`: `255,503,392 -> 255,475,212` (`-28,180`)
    - `dispatch_loops`: `350,051,148 -> 350,049,401` (`-1,747`)
    - `matrix_add_2d`: `12,061,441 -> 12,037,266` (`-24,175`)
    - `map_object_access`: `21,942,568 -> 21,957,884` (`+15,316`)
- relative to accepted `055330`, this cut still stays safely below accepted on
  `dispatch_loops` / `matrix_add_2d`, nearly neutral on `numeric_loops`, but
  it is consistently above accepted on `map_object_access`
- because the probe adds a real cache-retention behavior improvement, holds the
  tracked signature, and never shows the broad multi-case regression pattern of
  the rejected runtime-body probes, it is worth keeping in the live tree
- because `map_object_access` kept drifting upward and the three reruns never
  converged to a uniform representative win, it is still not strong enough to
  replace accepted `055330`

Validation after keep:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
- WSL gcc tracked non-GC benchmark reruns pass three times:
  - `cmake --build build/benchmark-gcc-release --target run_performance_suite --parallel 1`
    with:
    - `ZR_VM_TEST_TIER=profile`
    - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
    - `ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp`
    - `ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop`

### Rejected multi-slot exact-receiver version-mismatch object/member-name repair

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multislot_exact_receiver_version_mismatch_object_repair_20260422-103046`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multislot_exact_receiver_version_mismatch_object_repair_confirm_20260422-103233`
- third rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multislot_exact_receiver_version_mismatch_object_repair_rerun_20260422-103533`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Temporary TDD coverage reverted with the probe:

- `tests/core/test_execution_member_access_fast_paths.c`
  - `test_member_get_cached_multi_slot_exact_receiver_object_version_mismatch_repairs_from_member_name_hit`
  - `test_member_set_cached_multi_slot_exact_receiver_object_version_mismatch_repairs_from_member_name_hit`

Restored kept coverage after revert:

- `tests/core/test_execution_member_access_fast_paths.c`
  - `test_member_get_cached_multi_slot_exact_receiver_object_version_mismatch_clears_slot_and_falls_back`
  - `test_member_set_cached_multi_slot_exact_receiver_object_version_mismatch_clears_slot_and_falls_back`

Reason:

- this probe moved the multi-slot exact-receiver object/member-name lane ahead of
  the version-mismatch clear so an own-field hit could repair the slot instead
  of immediately dropping the cache entry
- all eight tracked non-GC cases again stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` versus both kept live `092432` and accepted
  `055330`, so this remained a pure runtime-body/code-layout experiment
- representative Callgrind totals versus kept live `092432` would not
  stabilize:
  - first:
    - `numeric_loops`: `255,475,212 -> 255,491,331` (`+16,119`)
    - `dispatch_loops`: `350,049,401 -> 350,031,705` (`-17,696`)
    - `matrix_add_2d`: `12,037,266 -> 12,039,059` (`+1,793`)
    - `map_object_access`: `21,957,884 -> 21,969,704` (`+11,820`)
  - confirm:
    - `numeric_loops`: `255,475,212 -> 255,473,585` (`-1,627`)
    - `dispatch_loops`: `350,049,401 -> 350,035,960` (`-13,441`)
    - `matrix_add_2d`: `12,037,266 -> 12,046,205` (`+8,939`)
    - `map_object_access`: `21,957,884 -> 21,932,373` (`-25,511`)
  - third:
    - `numeric_loops`: `255,475,212 -> 255,486,441` (`+11,229`)
    - `dispatch_loops`: `350,049,401 -> 350,041,923` (`-7,478`)
    - `matrix_add_2d`: `12,037,266 -> 12,044,645` (`+7,379`)
    - `map_object_access`: `21,957,884 -> 21,935,143` (`-22,741`)
- the target bodies in `dispatch_loops` did keep moving the intended direction
  on confirm/third:
  - `object_set_value_core`: `848,437 -> 846,361 -> 846,213`
  - `ZrCore_String_Equal`: `501,072 -> 498,712 -> 498,656`
  - `ZrCore_Object_GetValue`: `199,336 -> 198,806 -> 198,653`
- but `matrix_add_2d` stayed above live on all three reruns, and the first
  rerun still pushed `map_object_access` above live by `+11,820`, so this did
  not meet the retained-win bar for the current W1 line
- relative to accepted `055330`, the third rerun also remained above accepted
  on:
  - `numeric_loops`: `255,474,138 -> 255,486,441` (`+12,303`)
  - `map_object_access`: `21,931,485 -> 21,935,143` (`+3,658`)
- the probe is therefore not worth carrying as another ambiguous exact-receiver
  repair experiment

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_multi_slot_cached_member_name_broadcast_rerun_20260422-092432`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

### Retained cached-get null-key / descriptor-name cache cut

Snapshots:

- live baseline before probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_mod_mixed_int_known_type_numeric_conversions_confirm_20260422-223520`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_20260422-232114`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`

Decision:

- kept in the live tree
- not promoted to accepted W1 checkpoint

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

Reason:

- this cut narrows the cached get-side body in two places only:
  - `execution_member_get_by_name(...)` now lets the object fast helper run
    with `memberKey == NULL` instead of always building the key first
  - cached descriptor prototype fallback now walks by descriptor name through
    prototype storage caches instead of materializing a temporary `memberKey`
- all eight tracked non-GC cases stayed bit-for-bit identical in
  `instructions/helpers/slowpaths` versus live `223520`, so this remains a
  pure runtime-body cut, not a quickening / opcode-mix change
- representative Callgrind totals versus live `223520`:
  - first:
    - `numeric_loops`: `255,515,437 -> 255,500,849` (`-14,588`)
    - `dispatch_loops`: `349,886,874 -> 349,880,840` (`-6,034`)
    - `matrix_add_2d`: `12,052,239 -> 12,038,000` (`-14,239`)
    - `map_object_access`: `21,945,227 -> 21,940,724` (`-4,503`)
  - confirm:
    - `numeric_loops`: `255,515,437 -> 255,479,313` (`-36,124`)
    - `dispatch_loops`: `349,886,874 -> 349,867,883` (`-18,991`)
    - `matrix_add_2d`: `12,052,239 -> 12,044,572` (`-7,667`)
    - `map_object_access`: `21,945,227 -> 21,949,857` (`+4,630`)
- relative to accepted `055330`, this still is not enough to replace the W1
  checkpoint:
  - `numeric_loops`: `255,474,138 -> 255,479,313` (`+5,175`)
  - `dispatch_loops`: `350,330,462 -> 349,867,883` (`-462,579`)
  - `matrix_add_2d`: `12,053,512 -> 12,044,572` (`-8,940`)
  - `map_object_access`: `21,931,485 -> 21,949,857` (`+18,372`)
- the gain therefore gets locked as live continuation only

Validation after keep:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`

### Rejected cached descriptor get/set symmetry follow-up

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_cached_descriptor_get_set_name_cache_20260422-233724`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_cached_descriptor_get_set_name_cache_confirm_20260422-234352`

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this follow-up extended the same name-cache/lazy-key idea into
  `object_set_member_cached_descriptor_unchecked_core(...)`
- both reruns again kept tracked `instructions/helpers/slowpaths` bit-for-bit
  identical versus live `223520`
- relative to live `223520`, confirm was still bounded:
  - `numeric_loops`: `255,515,437 -> 255,475,303` (`-40,134`)
  - `dispatch_loops`: `349,886,874 -> 349,884,417` (`-2,457`)
  - `matrix_add_2d`: `12,052,239 -> 12,042,982` (`-9,257`)
  - `map_object_access`: `21,945,227 -> 21,945,590` (`+363`)
- but the real decision point is relative to the already kept `232733`
  get-only continuation:
  - `numeric_loops`: `255,479,313 -> 255,475,303` (`-4,010`)
  - `dispatch_loops`: `349,867,883 -> 349,884,417` (`+16,534`)
  - `matrix_add_2d`: `12,044,572 -> 12,042,982` (`-1,590`)
  - `map_object_access`: `21,949,857 -> 21,945,590` (`-4,267`)
- that trade gives back too much of the highest-priority `dispatch_loops`
  win just to shave smaller amounts off `numeric_loops` / `map_object_access`
- by the current W1 priority order, this follow-up is not worth carrying

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`

### Rejected dispatch-local `GET_CLOSURE` / `SET_CLOSURE` open-capture helper cut

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- `tests/core/test_execution_dispatch_callable_metadata.c`

Reason:

- this probe only swapped `GET_CLOSURE` / `SET_CLOSURE` onto the already-kept
  no-profile closure-value helper used by `GETUPVAL` / `SETUPVAL`
- the fresh tracked non-GC rerun again stayed bit-for-bit identical to kept
  live `232733` in `instructions/helpers/slowpaths` across all eight tracked
  cases, so there was still no opcode-mix or helper-signature change
- representative totals versus kept live `232733` were not acceptable under the
  current W1 priority order:
  - `numeric_loops`: `255,479,313 -> 255,470,849` (`-8,464`)
  - `dispatch_loops`: `349,867,883 -> 349,878,487` (`+10,604`)
  - `matrix_add_2d`: `12,044,572 -> 12,046,518` (`+1,946`)
  - `map_object_access`: `21,949,857 -> 21,970,127` (`+20,270`)
- because the slice regressed the two higher-priority representative cases
  (`dispatch_loops`, `map_object_access`) on the same tracked signature, it was
  reverted instead of being kept as another ambiguous closure-site cleanup

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_precall_frame_slot_reset_test`
  - WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_precall_frame_slot_reset_test`

### Accepted dispatch member exact-pair checked-object entry

Decision:

- accepted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_20260424-continue`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_internal.h`
- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- `GET_MEMBER_SLOT` / `SET_MEMBER_SLOT` already reject non-object receivers
  before attempting the dispatch-local exact receiver-pair fast path
- the new checked-object helpers keep the existing generic helpers for direct
  unit-test coverage and non-dispatch callers, but let the opcode arm skip the
  duplicate object/array type guard after the opcode receiver gate
- string `GET_MEMBER_SLOT` receivers are preserved by gating the checked helper
  call and falling through to `execution_member_get_cached(...)`
- representative totals relative to live
  `performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`:
  - first:
    - `numeric_loops`: `255,466,030 -> 255,449,599` (`-16,431`)
    - `dispatch_loops`: `349,798,765 -> 347,095,401` (`-2,703,364`)
    - `matrix_add_2d`: `12,008,069 -> 12,052,569` (`+44,500`)
    - `map_object_access`: `21,699,329 -> 21,743,373` (`+44,044`)
  - confirm:
    - `numeric_loops`: `255,466,030 -> 255,450,339` (`-15,691`)
    - `dispatch_loops`: `349,798,765 -> 347,092,198` (`-2,706,567`)
    - `matrix_add_2d`: `12,008,069 -> 12,026,781` (`+18,712`)
    - `map_object_access`: `21,699,329 -> 21,745,969` (`+46,640`)
- the four checked cases kept identical instruction/helper/slowpath signatures;
  the small matrix/map layout cost is accepted because the target
  `dispatch_loops` case repeatedly drops about 2.7M Ir

Validation:

- WSL gcc:
  - `zr_vm_execution_member_access_fast_paths_test`: `102 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `17 Tests 0 Failures`
- WSL clang:
  - `zr_vm_execution_member_access_fast_paths_test`: `102 Tests 0 Failures`
  - `zr_vm_execution_dispatch_callable_metadata_test`: `17 Tests 0 Failures`

### Rejected `GET_MEMBER_SLOT` non-string checked-object gate probe

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_get_member_slot_non_string_checked_gate_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- this probe tried to replace the accepted checked-object `(object || array)`
  dispatch gate with `opA->type != ZR_VALUE_TYPE_STRING`, relying on the outer
  opcode receiver gate to have already rejected every other type
- the four checked tracked non-GC cases kept identical
  `instructions/helpers/slowpaths` signatures versus accepted checked-object
  confirm live
- representative totals versus
  `performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`:
  - `numeric_loops`: `255,450,339 -> 255,438,995` (`-11,344`)
  - `dispatch_loops`: `347,092,198 -> 348,660,382` (`+1,568,184`)
  - `matrix_add_2d`: `12,026,781 -> 12,018,735` (`-8,046`)
  - `map_object_access`: `21,745,969 -> 21,742,780` (`-3,189`)
- because the target `dispatch_loops` workload regressed by about 1.57M Ir
  without semantic signature movement, this is classified as a
  branch-shape/layout regression

After revert:

- `tests_generated/performance/` was restored to accepted checked-object live:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`

### Rejected dispatch exact-pair set `AfterFastMiss` fallback probe

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_dispatch_exact_pair_set_after_fast_miss_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_internal.h`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- this probe tried to call
  `ZrCore_Object_SetExistingPairValueAfterFastMissUnchecked(...)` after the
  dispatch exact receiver-pair set helper's own plain-value fast set attempts
  had already missed, avoiding the wrapper-level repeat check in
  `ZrCore_Object_SetExistingPairValueUnchecked(...)`
- a checked-object slow-lane test covered hidden-items cached state plus unique
  ownership assignment during the probe
- focused validation passed before the perf rerun:
  - WSL gcc:
    - `zr_vm_execution_member_access_fast_paths_test`: `103 Tests 0 Failures`
    - `zr_vm_execution_dispatch_callable_metadata_test`: `17 Tests 0 Failures`
  - WSL clang:
    - `zr_vm_execution_member_access_fast_paths_test`: `103 Tests 0 Failures`
    - `zr_vm_execution_dispatch_callable_metadata_test`: `17 Tests 0 Failures`
- tracked non-GC signatures stayed identical versus accepted checked-object
  confirm live, but all representative totals regressed:
  - `numeric_loops`: `255,450,339 -> 255,452,070` (`+1,731`)
  - `dispatch_loops`: `347,092,198 -> 347,098,416` (`+6,218`)
  - `matrix_add_2d`: `12,026,781 -> 12,035,300` (`+8,519`)
  - `map_object_access`: `21,745,969 -> 21,756,786` (`+10,817`)
- conclusion: even though the source-level path removes a duplicate
  fast-miss attempt, the representative runtime body got slightly worse, so the
  probe was not kept

After revert:

- `tests_generated/performance/` was restored to accepted checked-object live:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`

### Rejected `KNOWN_VM_MEMBER_CALL` cached receiver object direct-use probe

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_known_vm_member_cached_receiver_object_direct_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- `execution_try_resolve_known_vm_member_exact_single_slot_fast(...)` already
  checks that the receiver raw pointer exactly equals
  `slot->cachedReceiverObject`
- the probe used `slot->cachedReceiverObject` directly after that pointer match,
  skipping `ZR_CAST_OBJECT(ZR_NULL, receiverValue->value.object)` and the
  following null check while keeping the module/prototype/version/closure gates
- `execution_dispatch.c` was not split for this probe because the file is
  already large but the change only replaced one expression inside an existing
  dispatch helper and did not add a responsibility
- focused validation passed before perf:
  - WSL gcc:
    - `zr_vm_execution_dispatch_callable_metadata_test`: `17 Tests 0 Failures`
    - `zr_vm_execution_member_access_fast_paths_test`: `102 Tests 0 Failures`
  - WSL clang:
    - `zr_vm_execution_dispatch_callable_metadata_test`: `17 Tests 0 Failures`
    - `zr_vm_execution_member_access_fast_paths_test`: `102 Tests 0 Failures`
- tracked non-GC signatures stayed identical versus accepted checked-object
  confirm live, but representative totals did not hold:
  - `numeric_loops`: `255,450,339 -> 255,487,713` (`+37,374`)
  - `dispatch_loops`: `347,092,198 -> 347,102,397` (`+10,199`)
  - `matrix_add_2d`: `12,026,781 -> 12,029,056` (`+2,275`)
  - `map_object_access`: `21,745,969 -> 21,744,804` (`-1,165`)
- conclusion: target `dispatch_loops` and `numeric_loops` both regressed, and
  the small `map_object_access` drop is not enough to keep the change

After revert:

- `tests_generated/performance/` was restored to accepted checked-object live:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`

### Rejected Map readonly-inline get callback plain-value direct-copy probe

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_map_get_plain_value_direct_copy_20260424-continue`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_map_get_plain_value_direct_copy_confirm_20260424-continue`

Production probe:

- `zr_vm_lib_container/src/zr_vm_lib_container/module.c`

Reason:

- the probe tried to bypass `ZrCore_Value_CopyNoProfile(...)` inside
  `zr_container_map_get_item_readonly_inline_fast(...)` for none-ownership
  non-GC-object mapped values
- both reruns kept checked tracked non-GC `instructions/helpers/slowpaths`
  signatures identical to current live
  `performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`
- relative to that current live evidence:
  - first rerun:
    - `numeric_loops`: `255,466,030 -> 255,412,571` (`-53,459`)
    - `dispatch_loops`: `349,798,765 -> 349,806,573` (`+7,808`)
    - `matrix_add_2d`: `12,008,069 -> 11,998,284` (`-9,785`)
    - `map_object_access`: `21,699,329 -> 21,705,231` (`+5,902`)
  - confirm rerun:
    - `numeric_loops`: `255,466,030 -> 255,434,387` (`-31,643`)
    - `dispatch_loops`: `349,798,765 -> 349,836,205` (`+37,440`)
    - `matrix_add_2d`: `12,008,069 -> 11,999,303` (`-8,766`)
    - `map_object_access`: `21,699,329 -> 21,702,667` (`+3,338`)
- function-level annotate showed the local callback body got smaller:
  - `zr_container_map_get_item_readonly_inline_fast`: `871,617 -> 863,430` Ir
- because `map_object_access` total still regressed in both runs and
  `dispatch_loops` moved the wrong way, the local direct-copy branch is treated
  as another runtime-body/code-layout regression rather than a retained W1 cut

### Rejected object-local `value_reset_null` profile-counting probe

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_object_reset_null_local_profile_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- the probe replaced object-local `ZrCore_Value_ResetAsNull(...)` calls with
  state-local helper counting plus `ZrCore_Value_ResetAsNullNoProfile(...)`
- the intent was to mirror the dispatch-local helper-counting style and avoid
  TLS current-profile lookup without changing helper totals
- checked tracked non-GC `instructions/helpers/slowpaths` signatures stayed
  identical to current live
  `performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`
- relative to that current live evidence:
  - `numeric_loops`: `255,466,030 -> 255,416,386` (`-49,644`)
  - `dispatch_loops`: `349,798,765 -> 349,816,294` (`+17,529`)
  - `matrix_add_2d`: `12,008,069 -> 11,991,207` (`-16,862`)
  - `map_object_access`: `21,699,329 -> 21,697,762` (`-1,567`)
- the intended reset hotspot did not move:
  - `dispatch_loops` top helper stayed `ZrCore_Value_ResetAsNull = 139,740` Ir
  - `object_set_value_core`: `843,728 -> 844,463` Ir
- because the hottest representative dispatch case regressed and the target
  symbol did not shrink, the probe was reverted without a confirm rerun

### Rejected `GET_STACK` / `GET_CONSTANT` profile-mode offset-copy probe

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_stack_constant_profile_offset_copy_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- the probe changed `EXECUTE_GET_STACK_BODY_FAST` and
  `EXECUTE_GET_CONSTANT_BODY_FAST` only on the `recordHelpers` path
- it counted `ZR_PROFILE_HELPER_VALUE_COPY` directly and used the destination
  offset to copy to stack/ret, instead of preparing `destination` and calling
  `execution_copy_value_fast(...)`
- checked tracked non-GC `instructions/helpers/slowpaths` signatures stayed
  identical to current live
  `performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`
- relative to that current live evidence:
  - `numeric_loops`: `255,466,030 -> 255,409,667` (`-56,363`)
  - `dispatch_loops`: `349,798,765 -> 349,819,223` (`+20,458`)
  - `matrix_add_2d`: `12,008,069 -> 11,999,457` (`-8,612`)
  - `map_object_access`: `21,699,329 -> 21,708,824` (`+9,495`)
- `dispatch_loops` did not show a real interpreter-body win:
  - `ZrCore_Execute`: unchanged at `332,568,516` Ir
  - `object_set_value_core`: `843,728 -> 846,456` Ir
- the probe was therefore classified as runtime-body/layout churn and reverted
  without a confirm rerun

### Rejected dispatch-side KnownObject index fast-entry follow-up

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_index_known_object_fast_probe_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

Reason:

- this probe moved the object payload cast from the readonly-inline
  stack-operands helper into the `GET_BY_INDEX` / `SET_BY_INDEX` dispatch
  branches, then called new `ZrCore_Object_Try*ByIndexReadonlyInlineFastStackOperandsKnownObject(...)`
  entry points
- the goal was to avoid reloading `receiver->value.object` in the object helper
  after dispatch had already established the receiver type
- focused WSL gcc/clang guards passed before profiling, and checked tracked
  non-GC profile signatures stayed identical to the current live receiver-type
  guard-skip baseline
- representative totals relative to current live
  `performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`:
  - `numeric_loops`: `255,466,030 -> 255,433,283` (`-32,747`)
  - `dispatch_loops`: `349,798,765 -> 349,829,906` (`+31,141`)
  - `matrix_add_2d`: `12,008,069 -> 12,006,235` (`-1,834`)
  - `map_object_access`: `21,699,329 -> 21,802,724` (`+103,395`)
- annotate also showed the target helper got heavier instead of lighter:
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperandsKnownObject`: `385,197` Ir
    versus the current live helper's `368,811` Ir
  - `ZrCore_Object_TrySetByIndexReadonlyInlineFastStackOperandsKnownObject`: `188,591` Ir
    versus the current live helper's `159,894` Ir
  - `ZrCore_Execute`: `9,026,536` Ir versus current live `8,981,450` Ir
- because both the target `map_object_access` and hottest dispatch body moved
  the wrong way, and the new direct helpers were larger, this was rejected
  without a confirm rerun

Validation after revert:

- restored `tests_generated/performance/` to current live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`
- WSL gcc direct binaries pass:
  - `build/codex-wsl-gcc-debug/bin/zr_vm_object_call_known_native_fast_path_test` (`57 Tests 0 Failures`)
  - `build/codex-wsl-gcc-debug/bin/zr_vm_container_runtime_test` (`39 Tests 0 Failures`)
  - `build/codex-wsl-gcc-debug/bin/zr_vm_execution_dispatch_callable_metadata_test` (`17 Tests 0 Failures`)
- WSL clang direct binaries pass:
  - `build/codex-wsl-clang-debug/bin/zr_vm_object_call_known_native_fast_path_test` (`57 Tests 0 Failures`)
  - `build/codex-wsl-clang-debug/bin/zr_vm_container_runtime_test` (`39 Tests 0 Failures`)
  - `build/codex-wsl-clang-debug/bin/zr_vm_execution_dispatch_callable_metadata_test` (`17 Tests 0 Failures`)

### Accepted stack-operands readonly-inline receiver type guard skip

Decision:

- keep as current live continuation

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_20260424-continue`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- `GET_BY_INDEX` / `SET_BY_INDEX` dispatch checks the stack receiver type before
  entering the internal readonly-inline stack-operands helpers
- the helper null/value guards still reject missing `state`, `receiver`, `key`,
  `result`/`value`, missing receiver object payload, and integer keys, but no
  longer repeat the object/array receiver type gate
- both first and confirm reruns kept the checked tracked non-GC
  `instructions/helpers/slowpaths` signatures identical to the current live
  baseline:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_index_profile_runtime_local_rerun_20260424-continue`
- first rerun totals versus current live:
  - `numeric_loops`: `255,453,211 -> 255,439,209` (`-14,002`)
  - `dispatch_loops`: `349,834,504 -> 349,813,549` (`-20,955`)
  - `matrix_add_2d`: `12,000,740 -> 11,992,115` (`-8,625`)
  - `map_object_access`: `21,766,383 -> 21,739,637` (`-26,746`)
- confirm rerun totals versus current live:
  - `numeric_loops`: `255,453,211 -> 255,466,030` (`+12,819`)
  - `dispatch_loops`: `349,834,504 -> 349,798,765` (`-35,739`)
  - `matrix_add_2d`: `12,000,740 -> 12,008,069` (`+7,329`)
  - `map_object_access`: `21,766,383 -> 21,699,329` (`-67,054`)
- the target helper itself stayed lower in annotate:
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`:
    `401,595 -> 368,811` Ir
- conclusion:
  - the target `map_object_access` improvement repeated, `dispatch_loops`
    improved in both guard-skip runs, and profile signatures stayed stable
  - bounded non-target numeric/matrix movement is accepted as run-to-run noise
    for this narrow index helper body cleanup

Validation:

- WSL gcc direct binaries passed before the perf reruns:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
- WSL clang direct binaries passed before the perf reruns:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
- Windows MSVC CLI smoke passed after retaining the probe:
  - `cmake --build build/codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable`
  - `build/codex-msvc-cli-debug/bin/Debug/zr_vm_cli.exe tests/fixtures/projects/hello_world/hello_world.zrp`
  - output: `hello world`

### Rejected stack-operands readonly-inline null guard assertion

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_null_guard_assert_20260424-continue`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_null_guard_assert_confirm_20260424-continue`
- tie-break rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_null_guard_assert_rerun_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe followed the accepted receiver-type guard skip by changing the
  remaining stack-operands helper argument null checks to `ZR_ASSERT(...)`
  preconditions
- the runtime branch still kept the checks that can change normal dispatch
  behavior:
  - missing `receiver->value.object`
  - integer keys that should stay on the super-array/direct-index lane
- WSL gcc and clang focused object/container binaries passed before profiling
- all three reruns kept the checked tracked non-GC
  `instructions/helpers/slowpaths` signatures identical to the receiver-type
  guard-skip live baseline
- relative to
  `performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`,
  the totals were:
  - first:
    - `numeric_loops`: `255,466,030 -> 255,442,905` (`-23,125`)
    - `dispatch_loops`: `349,798,765 -> 349,841,450` (`+42,685`)
    - `matrix_add_2d`: `12,008,069 -> 11,991,642` (`-16,427`)
    - `map_object_access`: `21,699,329 -> 21,637,175` (`-62,154`)
  - confirm:
    - `numeric_loops`: `255,466,030 -> 255,428,957` (`-37,073`)
    - `dispatch_loops`: `349,798,765 -> 349,821,732` (`+22,967`)
    - `matrix_add_2d`: `12,008,069 -> 12,003,239` (`-4,830`)
    - `map_object_access`: `21,699,329 -> 21,650,751` (`-48,578`)
  - tie-break:
    - `numeric_loops`: `255,466,030 -> 255,412,572` (`-53,458`)
    - `dispatch_loops`: `349,798,765 -> 349,810,385` (`+11,620`)
    - `matrix_add_2d`: `12,008,069 -> 12,010,146` (`+2,077`)
    - `map_object_access`: `21,699,329 -> 21,661,273` (`-38,056`)
- although `map_object_access` and the direct stack-operands helper body
  improved, `dispatch_loops` regressed in all three runs
- conclusion:
  - reject and revert
  - do not retry this null-guard assertion variant while W1 keeps repeated
    `dispatch_loops`回吐 as a rejection signal

Validation after revert:

- restored `tests_generated/performance/` to:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`
- WSL gcc direct binaries pass after revert:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
- WSL clang direct binaries pass after revert:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`
- Windows MSVC CLI smoke passes after revert:
  - `cmake --build build/codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable`
  - `build/codex-msvc-cli-debug/bin/Debug/zr_vm_cli.exe tests/fixtures/projects/hello_world/hello_world.zrp`
  - output: `hello world`

### Rejected Map readonly-inline callback receiver type guard skip

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_map_readonly_inline_callback_type_guard_skip_20260424-continue`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_map_readonly_inline_callback_type_guard_skip_confirm_20260424-continue`
- tie-break rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_map_readonly_inline_callback_type_guard_skip_rerun_20260424-continue`

Production probe:

- `zr_vm_lib_container/src/zr_vm_lib_container/module.c`

Reason:

- this probe removed the duplicate object/array `selfValue` type check inside
  the Map readonly-inline get/set callbacks:
  - `zr_container_map_get_item_readonly_inline_fast(...)`
  - `zr_container_map_set_item_readonly_inline_no_result_fast(...)`
- the outer index dispatch and object stack-operands helper already check the
  receiver type before entering the callback on the hot path
- WSL gcc and clang focused object/container binaries passed before profiling
- all checked tracked non-GC `instructions/helpers/slowpaths` signatures stayed
  identical to current live
  `performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`
- relative to current live, totals were:
  - first:
    - `numeric_loops`: `255,466,030 -> 255,439,019` (`-27,011`)
    - `dispatch_loops`: `349,798,765 -> 349,795,746` (`-3,019`)
    - `matrix_add_2d`: `12,008,069 -> 12,009,328` (`+1,259`)
    - `map_object_access`: `21,699,329 -> 21,657,936` (`-41,393`)
  - confirm:
    - `numeric_loops`: `255,466,030 -> 255,443,009` (`-23,021`)
    - `dispatch_loops`: `349,798,765 -> 349,870,134` (`+71,369`)
    - `matrix_add_2d`: `12,008,069 -> 11,997,240` (`-10,829`)
    - `map_object_access`: `21,699,329 -> 21,657,387` (`-41,942`)
  - tie-break:
    - `numeric_loops`: `255,466,030 -> 255,425,568` (`-40,462`)
    - `dispatch_loops`: `349,798,765 -> 349,829,923` (`+31,158`)
    - `matrix_add_2d`: `12,008,069 -> 12,010,752` (`+2,683`)
    - `map_object_access`: `21,699,329 -> 21,654,060` (`-45,269`)
- the direct callback body stayed lower:
  - `zr_container_map_get_item_readonly_inline_fast`: `871,617 -> 822,455` Ir
- however the confirm and tie-break runs both regressed `dispatch_loops`
- conclusion:
  - reject and revert
  - keep the Map callback-local receiver type guard while W1 is still using
    repeated `dispatch_loops`回吐 as a rejection signal

Validation after revert:

- restored `tests_generated/performance/` to:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`
- WSL gcc direct binary passes after revert:
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
- WSL clang direct binary passes after revert:
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`

### Accepted dispatch-local readonly-index profile helper count

Decision:

- accepted as the current live continuation
- not promoted to a new accepted W1 checkpoint by itself

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_index_profile_runtime_local_20260424-continue`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_index_profile_runtime_local_confirm_20260424-continue`
- tie-break rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_index_profile_runtime_local_rerun_20260424-continue`

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- the `GET_BY_INDEX` / `SET_BY_INDEX` readonly-inline stack-operands hit path
  already executes inside a dispatch loop that has cached `profileRuntime` and
  `recordHelpers`
- the previous hit path still called
  `ZrCore_Profile_RecordHelperFromState(...)`, which reloaded
  `state->global->profileRuntime` through the exported
  `ZrCore_Profile_FromState(...)` helper
- this cut records the same helper counts through the dispatch-local runtime
  pointer:
  - `ZR_PROFILE_HELPER_GET_BY_INDEX`
  - `ZR_PROFILE_HELPER_SET_BY_INDEX`
- all three reruns stayed bit-for-bit identical to kept live
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`
  across the eight tracked non-GC `instructions/helpers/slowpaths`, so the
  change is a pure runtime-body/profile-counting use-site cleanup
- representative totals versus kept live `162914` were:
  - first rerun:
    - `numeric_loops`: `255,428,731 -> 255,424,772` (`-3,959`)
    - `dispatch_loops`: `349,832,789 -> 349,833,727` (`+938`)
    - `matrix_add_2d`: `12,009,340 -> 12,008,362` (`-978`)
    - `map_object_access`: `21,881,343 -> 21,795,581` (`-85,762`)
  - confirm rerun:
    - `numeric_loops`: `255,428,731 -> 255,427,864` (`-867`)
    - `dispatch_loops`: `349,832,789 -> 349,856,055` (`+23,266`)
    - `matrix_add_2d`: `12,009,340 -> 12,005,608` (`-3,732`)
    - `map_object_access`: `21,881,343 -> 21,748,105` (`-133,238`)
  - tie-break rerun:
    - `numeric_loops`: `255,428,731 -> 255,453,211` (`+24,480`)
    - `dispatch_loops`: `349,832,789 -> 349,834,504` (`+1,715`)
    - `matrix_add_2d`: `12,009,340 -> 12,000,740` (`-8,600`)
    - `map_object_access`: `21,881,343 -> 21,766,383` (`-114,960`)
- the target readonly-index benchmark improved on all three reruns, while the
  hottest `dispatch_loops` movement was mixed and small enough to keep this as
  a live continuation rather than reject it
- `map_object_access` Callgrind no longer lists `ZrCore_Profile_FromState`
  after the cut:
  - before: `98,352 Ir`
  - after tie-break: absent from the annotate top list

Validation on the kept tree:

- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
- Windows MSVC CLI smoke passes:
  - `cmake --build build/codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable`
  - `build/codex-msvc-cli-debug/bin/Debug/zr_vm_cli.exe tests/fixtures/projects/hello_world/hello_world.zrp`
    prints `hello world`
- `tests_generated/performance/` now reflects kept live tie-break evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_index_profile_runtime_local_rerun_20260424-continue`

### Rejected dispatch-local GET_BY_INDEX stable-result reset count

Decision:

- rejected and reverted without confirm

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_get_index_reset_local_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- this follow-up changed only the `GET_BY_INDEX` dispatch-local
  `stableResult` initialization:
  - record `ZR_PROFILE_HELPER_VALUE_RESET_NULL` through the dispatch-local
    `profileRuntime`
  - call `ZrCore_Value_ResetAsNullNoProfile(&stableResult)`
- WSL gcc and clang focused binaries passed before profiling:
  - `zr_vm_object_call_known_native_fast_path_test`: 57/57
  - `zr_vm_container_runtime_test`: 39/39
  - `zr_vm_execution_dispatch_callable_metadata_test`: 17/17
- the first tracked rerun improved the representative Callgrind totals versus
  kept dispatch-local live
  `performance_profile_tracked_non_gc_after_dispatch_index_profile_runtime_local_rerun_20260424-continue`:
  - `numeric_loops`: `255,453,211 -> 255,416,043` (`-37,168`)
  - `dispatch_loops`: `349,834,504 -> 349,811,548` (`-22,956`)
  - `matrix_add_2d`: `12,000,740 -> 11,999,644` (`-1,096`)
  - `map_object_access`: `21,766,383 -> 21,758,407` (`-7,976`)
- however the tracked helper signatures changed:
  - `map_object_access/value_reset_null`: `4,601 -> 12,797` (`+8,196`)
  - `string_build/value_reset_null`: `6,734 -> 6,897` (`+163`)
- because the current W1 line is constrained to runtime-body cuts that preserve
  the tracked profile signature, this profile-counting change was discarded
  despite the favorable first Callgrind totals

Validation after revert:

- restored `tests_generated/performance/` to kept dispatch-local live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_index_profile_runtime_local_rerun_20260424-continue`
- WSL gcc focused direct binaries pass after revert:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`

### Rejected get readonly-inline fast callback early-return cleanup

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_return_early_20260424-continue`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_return_early_confirm_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe only changed the get readonly-inline fast callback result handling
  in two stack-oriented helpers:
  - `object_try_call_hot_cached_known_native_get_by_index_readonly_inline_stack_operands(...)`
  - `object_try_call_cached_known_native_get_by_index_readonly_inline_stack_operands(...)`
- the semantics were unchanged: a callback miss with fine thread status still
  reset the result to null and succeeded; a non-fine thread status still failed
- WSL gcc and clang focused tests passed before profiling:
  - `zr_vm_object_call_known_native_fast_path_test`: 57/57
  - `zr_vm_execution_member_access_fast_paths_test`: 100/100
- all eight tracked non-GC profile signatures stayed bit-for-bit identical in
  `instructions/helpers/slowpaths`
- the local helper body improved consistently, but the target case regressed:
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`:
    `401,595 -> 393,398` (`-8,197`)
- representative totals versus kept get-side live
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`:
  - first rerun:
    - `numeric_loops`: `255,428,731 -> 255,422,034` (`-6,697`)
    - `dispatch_loops`: `349,832,789 -> 349,821,424` (`-11,365`)
    - `matrix_add_2d`: `12,009,340 -> 11,993,722` (`-15,618`)
    - `map_object_access`: `21,881,343 -> 21,892,549` (`+11,206`)
  - confirm rerun:
    - `numeric_loops`: `255,428,731 -> 255,418,495` (`-10,236`)
    - `dispatch_loops`: `349,832,789 -> 349,834,811` (`+2,022`)
    - `matrix_add_2d`: `12,009,340 -> 11,990,405` (`-18,935`)
    - `map_object_access`: `21,881,343 -> 21,906,998` (`+25,655`)
- because `map_object_access` regressed on both reruns and got worse on confirm,
  the cleanup was discarded despite the smaller local helper body

Validation after revert:

- restored `tests_generated/performance/` to kept get-side live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`

### Rejected hot get readonly-inline fast callback early-return cleanup

Decision:

- rejected and reverted without confirm

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_hot_get_return_early_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this narrower follow-up changed only
  `object_try_call_hot_cached_known_native_get_by_index_readonly_inline_stack_operands(...)`
  to return early from the callback miss branch
- WSL gcc and clang focused tests passed before profiling:
  - `zr_vm_object_call_known_native_fast_path_test`: 57/57
  - `zr_vm_execution_member_access_fast_paths_test`: 100/100
- all eight tracked non-GC profile signatures stayed bit-for-bit identical in
  `instructions/helpers/slowpaths`
- the local exported helper body again improved:
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`:
    `401,595 -> 393,398` (`-8,197`)
- representative totals versus kept get-side live
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`:
  - `numeric_loops`: `255,428,731 -> 255,423,556` (`-5,175`)
  - `dispatch_loops`: `349,832,789 -> 349,813,275` (`-19,514`)
  - `matrix_add_2d`: `12,009,340 -> 12,002,899` (`-6,441`)
  - `map_object_access`: `21,881,343 -> 21,913,227` (`+31,884`)
- because the target `map_object_access` regression was larger than the prior
  two-helper probe, the narrower version was discarded without spending a
  confirm run

### Rejected reset-null static value assignment

Decision:

- rejected and reverted without confirm

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_reset_null_static_value_assign_20260424-continue`

Production probe:

- `zr_vm_core/include/zr_vm_core/value.h`

Reason:

- this probe changed `ZrCore_Value_ResetAsNullNoProfile(...)` from explicit
  field stores to assigning a static `SZrTypeValue` null template
- WSL gcc and clang focused builds/tests passed before profiling:
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_numeric_fast_paths_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_object_call_known_native_fast_path_test`
- all eight tracked non-GC profile signatures stayed bit-for-bit identical in
  `instructions/helpers/slowpaths`
- the exported reset helper did not improve in representative Callgrind:
  - `numeric_loops`: `ZrCore_Value_ResetAsNull` stayed `135,830`
  - `dispatch_loops`: stayed `139,740`
  - `matrix_add_2d`: stayed `77,418`
  - `map_object_access`: stayed `78,234`
- representative totals versus kept get-side live
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`:
  - `numeric_loops`: `255,428,731 -> 255,432,570` (`+3,839`)
  - `dispatch_loops`: `349,832,789 -> 349,859,712` (`+26,923`)
  - `matrix_add_2d`: `12,009,340 -> 12,000,405` (`-8,935`)
  - `map_object_access`: `21,881,343 -> 21,901,805` (`+20,462`)
- because the reset helper body did not shrink and the two hottest target
  totals regressed, the template assignment shape was discarded

### Rejected map readonly-inline cached-pair get callback inline

Decision:

- rejected and reverted

Snapshot:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_map_get_cached_pair_inline_20260424-171624`

Production probe:

- `zr_vm_lib_container/src/zr_vm_lib_container/module.c`

Reason:

- this probe only touched the `Map` readonly-inline get callback:
  - when `entryObject->cachedCapacityPair` was already available,
    `zr_container_map_get_item_readonly_inline_fast(...)` copied
    `cachedPair->value` directly and returned
  - uncached entries still used `zr_container_pair_get_second_fast(...)`
- the goal was to remove the tiny
  `zr_container_map_entry_get_second_value_fast(...)` helper boundary from the
  hottest `map_object_access` library callback
- all eight tracked non-GC cases stayed bit-for-bit identical relative to kept
  live `162914` in `instructions/helpers/slowpaths`, so this was another pure
  runtime-body/code-layout probe rather than a quickening or opcode-mix change
- relative to kept live
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`,
  the representative totals were:
  - `numeric_loops`: `255,428,731 -> 255,412,391` (`-16,340`)
  - `dispatch_loops`: `349,832,789 -> 349,832,761` (`-28`)
  - `matrix_add_2d`: `12,009,340 -> 12,000,083` (`-9,257`)
  - `map_object_access`: `21,881,343 -> 21,913,019` (`+31,676`)
- the target function did not improve:
  - `zr_container_map_get_item_readonly_inline_fast`: `871,617 -> 871,624`
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands` stayed
    `401,595`
- because the only targeted representative case regressed materially while the
  callback itself did not shrink, the probe was discarded

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`
- WSL gcc direct binaries pass after revert:
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test` (`39 Tests 0 Failures`)
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
    (`57 Tests 0 Failures`)

### Rejected map readonly-inline cached-pair set callback inline

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_map_set_cached_pair_inline_20260424-172039`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_map_set_cached_pair_inline_confirm_20260424-172300`

Production probe:

- `zr_vm_lib_container/src/zr_vm_lib_container/module.c`

Reason:

- this probe only touched the `Map` readonly-inline set no-result callback:
  - when an existing entry already had `entryObject->cachedCapacityPair`, the
    callback called `ZrCore_Object_SetExistingPairValueUnchecked(...)` directly
    and updated `memberVersion` / `cachedStringLookupPair`
  - uncached entries still used `zr_container_pair_set_second_fast(...)`
- all eight tracked non-GC cases stayed bit-for-bit identical relative to kept
  live `162914` in `instructions/helpers/slowpaths`, so this was again a pure
  runtime-body/code-layout probe
- relative to kept live
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`,
  the representative totals were:
  - first rerun:
    - `numeric_loops`: `255,428,731 -> 255,420,250` (`-8,481`)
    - `dispatch_loops`: `349,832,789 -> 349,810,917` (`-21,872`)
    - `matrix_add_2d`: `12,009,340 -> 12,000,234` (`-9,106`)
    - `map_object_access`: `21,881,343 -> 21,887,641` (`+6,298`)
  - confirm rerun:
    - `numeric_loops`: `255,428,731 -> 255,426,375` (`-2,356`)
    - `dispatch_loops`: `349,832,789 -> 349,824,089` (`-8,700`)
    - `matrix_add_2d`: `12,009,340 -> 12,005,744` (`-3,596`)
    - `map_object_access`: `21,881,343 -> 21,905,180` (`+23,837`)
- because the target `map_object_access` regression worsened on confirm and
  the slice only bought smaller broad-case wins by making the Map callback body
  larger, it was discarded rather than carried as a mixed live continuation

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`
- WSL gcc direct binaries pass after revert:
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test` (`39 Tests 0 Failures`)
  - `build-wsl-gcc/bin/zr_vm_container_temp_value_root_test`
    (`4 Tests 0 Failures`)
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
    (`57 Tests 0 Failures`)

### Rejected cached get/set stack-root receiver refresh skip

Decision:

- rejected and partially reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_stack_root_receiver_refresh_skip_20260424-012934`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_stack_root_receiver_refresh_skip_confirm_20260424-012934`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Reason:

- this probe tried to skip the unconditional entry-level forwarded refresh in
  both `execution_member_get_cached(...)` and `execution_member_set_cached(...)`
  when the receiver already lived in VM stack roots
- both reruns stayed bit-for-bit identical to live
  `performance_profile_tracked_non_gc_after_getupval_setupval_stack_get_no_profile_reapply_confirm_20260423-094444`
  across all eight tracked non-GC `instructions/helpers/slowpaths`, so the
  change stayed a pure runtime-body/code-layout probe
- representative totals versus live `094444` were:
  - first rerun:
    - `numeric_loops`: `255,466,835 -> 255,453,237` (`-13,598`)
    - `dispatch_loops`: `349,859,645 -> 349,858,215` (`-1,430`)
    - `matrix_add_2d`: `12,028,686 -> 12,032,132` (`+3,446`)
    - `map_object_access`: `21,928,034 -> 21,933,695` (`+5,661`)
  - confirm rerun:
    - `numeric_loops`: `255,466,835 -> 255,454,920` (`-11,915`)
    - `dispatch_loops`: `349,859,645 -> 349,882,750` (`+23,105`)
    - `matrix_add_2d`: `12,028,686 -> 12,030,601` (`+1,915`)
    - `map_object_access`: `21,928,034 -> 21,924,735` (`-3,299`)
- because the confirm rerun still gave back hottest `dispatch_loops` on the
  same tracked signature, the wider get+set version was discarded instead of
  being kept as another ambiguous runtime-body cut

### Kept set-only stack-root operand gate

Decision:

- kept as the next live continuation

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_cached_stack_root_operand_gate_20260424-013904`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_cached_stack_root_operand_gate_confirm_20260424-013904`
- corroborating rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_cached_stack_root_operand_gate_rerun_20260424-013904`

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- this narrower follow-up keeps `execution_member_get_cached(...)` fully at live
  `094444`, and only changes the set side:
  - `execution_member_set_cached(...)` now skips the receiver forwarded refresh
    only when the receiver already points into VM stack roots
  - `stackOperandsGuaranteed` is computed once in
    `execution_member_set_cached(...)` and threaded into
    `execution_member_try_cached_set(...)` instead of rechecking the same stack
    ranges inside the callee
- a new stack-root guard test stays in tree:
  - `test_execution_member_get_cached_descriptor_stack_receiver_hits_from_vm_stack_roots`
- first, confirm, and rerun all stayed bit-for-bit identical to live
  `094444` across all eight tracked non-GC `instructions/helpers/slowpaths`
- representative totals versus live `094444` were:
  - first rerun:
    - `numeric_loops`: `255,466,835 -> 255,437,108` (`-29,727`)
    - `dispatch_loops`: `349,859,645 -> 349,887,202` (`+27,557`)
    - `matrix_add_2d`: `12,028,686 -> 12,032,985` (`+4,299`)
    - `map_object_access`: `21,928,034 -> 21,938,998` (`+10,964`)
  - confirm rerun:
    - `numeric_loops`: `255,466,835 -> 255,454,255` (`-12,580`)
    - `dispatch_loops`: `349,859,645 -> 349,855,756` (`-3,889`)
    - `matrix_add_2d`: `12,028,686 -> 12,038,163` (`+9,477`)
    - `map_object_access`: `21,928,034 -> 21,912,838` (`-15,196`)
  - corroborating rerun:
    - `numeric_loops`: `255,466,835 -> 255,451,585` (`-15,250`)
    - `dispatch_loops`: `349,859,645 -> 349,852,445` (`-7,200`)
    - `matrix_add_2d`: `12,028,686 -> 12,033,185` (`+4,499`)
    - `map_object_access`: `21,928,034 -> 21,925,898` (`-2,136`)
- function-level Callgrind on the targeted write path also moved in the keep
  direction for the two follow-up reruns:
  - `dispatch_loops` `object_set_value_core`: `847,834 -> 847,350 -> 845,875`
  - `map_object_access` `object_set_value_core`: `475,950 -> 474,121 -> 475,949`
- because the confirm and corroborating rerun both improved hottest
  `dispatch_loops` while staying on the same tracked signature, this narrower
  set-only cut is worth keeping as the next live continuation
- because `matrix_add_2d` still regresses and the first rerun stayed mixed, this
  cut is not strong enough to replace the accepted W1 checkpoint

Validation on the kept tree:

- WSL gcc focused direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
- WSL clang focused direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
- WSL gcc tracked non-GC reruns pass:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/benchmark-gcc-release --parallel 16 && ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop cmake --build build/benchmark-gcc-release --target run_performance_suite --parallel 1"`

Current live evidence:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_cached_stack_root_operand_gate_rerun_20260424-013904`

### Rejected known-missing direct-key add lane for member set slow paths

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_set_direct_key_known_missing_add_20260424-020407`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_set_direct_key_known_missing_add_confirm_20260424-020634`
- tie-break rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_set_direct_key_known_missing_add_rerun_20260424-020819`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Guard test kept:

- `tests/core/test_execution_member_access_fast_paths.c`
  - `test_execution_member_set_cached_descriptor_missing_pair_slow_lane_inserts_storage`

Reason:

- this probe introduced a new direct-key add helper for callers that had
  already proved the receiver-side pair was missing, and initially wired it into
  three member-set slow lanes:
  - cached descriptor set
  - set-by-name unchecked
  - set-by-name fast
- all three reruns stayed bit-for-bit identical to live
  `performance_profile_tracked_non_gc_after_set_cached_stack_root_operand_gate_rerun_20260424-013904`
  across the eight tracked non-GC `instructions/helpers/slowpaths`, so this was
  again a pure runtime-body/code-layout probe
- representative totals versus live `013904` were:
  - first rerun:
    - `numeric_loops`: `255,451,585 -> 255,475,519` (`+23,934`)
    - `dispatch_loops`: `349,852,445 -> 349,847,845` (`-4,600`)
    - `matrix_add_2d`: `12,033,185 -> 12,026,799` (`-6,386`)
    - `map_object_access`: `21,925,898 -> 21,921,872` (`-4,026`)
  - confirm rerun:
    - `numeric_loops`: `255,451,585 -> 255,469,958` (`+18,373`)
    - `dispatch_loops`: `349,852,445 -> 349,843,147` (`-9,298`)
    - `matrix_add_2d`: `12,033,185 -> 12,029,545` (`-3,640`)
    - `map_object_access`: `21,925,898 -> 21,925,194` (`-704`)
  - tie-break rerun:
    - `numeric_loops`: `255,451,585 -> 255,464,870` (`+13,285`)
    - `dispatch_loops`: `349,852,445 -> 349,862,071` (`+9,626`)
    - `matrix_add_2d`: `12,033,185 -> 12,031,365` (`-1,820`)
    - `map_object_access`: `21,925,898 -> 21,944,876` (`+18,978`)
- function-level movement also failed to stabilize on the hottest target path:
  - `dispatch_loops` `object_set_value_core`: `845,875 -> 844,882 -> 844,347 -> 846,648`
  - `map_object_access` `object_set_value_core`: `475,949 -> 475,213 -> 475,190`
  - `dispatch_loops` `ZrCore_String_Equal`: `476,948 -> 475,076 -> 479,436`
- because the tie-break rerun gave back both hottest `dispatch_loops` and
  `map_object_access` on the same tracked signature, the broader three-caller
  version was discarded instead of being kept as an ambiguous live continuation

### Rejected cached-descriptor-only direct-key add retry

Decision:

- rejected and reverted

Snapshot:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_direct_key_known_missing_add_20260424-021236`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this narrower retry kept the same helper idea, but only on
  `object_set_member_cached_descriptor_unchecked_core(...)` so the probe stayed
  strictly on the user-prioritized cached descriptor set slow lane
- the first rerun still stayed bit-for-bit identical to live `013904` across
  tracked non-GC `instructions/helpers/slowpaths`
- representative totals versus live `013904` were already mixed:
  - `numeric_loops`: `255,451,585 -> 255,476,286` (`+24,701`)
  - `dispatch_loops`: `349,852,445 -> 349,849,021` (`-3,424`)
  - `matrix_add_2d`: `12,033,185 -> 12,029,067` (`-4,118`)
  - `map_object_access`: `21,925,898 -> 21,941,370` (`+15,472`)
- because the very first rerun still materially regressed
  `map_object_access` while only shaving a small amount from `dispatch_loops`,
  the narrower retry was rejected without spending more reruns on another weak
  runtime-body variant

Validation after revert:

- WSL gcc focused direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
- WSL clang focused direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
- `tests_generated/performance/` was restored to live
  `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_set_cached_stack_root_operand_gate_rerun_20260424-013904`

### Rejected `GET_BY_INDEX` readonly-inline direct-result write

Decision:

- rejected and reverted

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- this probe tried to let the dispatch-local `GET_BY_INDEX` readonly-inline
  fast hit write directly into the destination slot when the destination did
  not alias the receiver/key operands and had no ownership payload
- the focused WSL gcc/clang guard binaries passed after the probe:
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_execution_dispatch_callable_metadata_test`
  - `zr_vm_object_call_known_native_fast_path_test`
- the tracked non-GC profile run then failed correctness on the real
  `map_object_access` benchmark:
  - `MOD_SIGNED_CONST requires numeric operands`
  - generated source location:
    `build/benchmark-gcc-release/tests_generated/performance_suite/cases/map_object_access/zr/src/main.zr:35`
- because this changed benchmark semantics instead of merely shifting runtime
  body cost, the probe was immediately reverted

Validation after revert:

- WSL gcc `map_object_access` profile/callgrind single-case rerun passes:
  - `ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=map_object_access cmake --build build/benchmark-gcc-release --target run_performance_suite --parallel 1`
- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c` has no remaining
  diff after the revert

### Rejected `GET_BY_INDEX` stable-result reset removal

Decision:

- rejected and reverted

Snapshots:

- fresh live baseline:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_before_get_by_index_stable_result_reset_20260424-155848`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_stable_result_reset_cut_20260424-160302`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_stable_result_reset_cut_confirm_20260424-160659`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- this probe removed the dispatch-local `ZrCore_Value_ResetAsNull(&stableResult)`
  before `GET_BY_INDEX` calls into the readonly-inline fast path or the protected
  slow object path
- all eight tracked non-GC `instructions/helpers/slowpaths` stayed bit-for-bit
  identical to fresh live baseline, so the probe was pure runtime-body/code
  layout movement rather than quickening or helper-signature movement
- representative Callgrind totals versus fresh live baseline were:
  - first rerun:
    - `numeric_loops`: `255,421,443 -> 255,470,201` (`+48,758`)
    - `dispatch_loops`: `349,859,320 -> 349,846,258` (`-13,062`)
    - `matrix_add_2d`: `12,024,576 -> 12,026,420` (`+1,844`)
    - `map_object_access`: `21,916,776 -> 21,858,980` (`-57,796`)
  - confirm rerun:
    - `numeric_loops`: `255,421,443 -> 255,478,576` (`+57,133`)
    - `dispatch_loops`: `349,859,320 -> 349,848,388` (`-10,932`)
    - `matrix_add_2d`: `12,024,576 -> 12,027,409` (`+2,833`)
    - `map_object_access`: `21,916,776 -> 21,900,043` (`-16,733`)
- because `numeric_loops` and `matrix_add_2d` regressed in both reruns while
  the win on `map_object_access` shrank sharply on confirm, this was rejected
  instead of retained as an ambiguous runtime-body cut

Validation after revert:

- WSL gcc focused binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
- WSL gcc `map_object_access` profile/callgrind single-case rerun passes:
  - `ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=map_object_access cmake --build build/benchmark-gcc-release --target run_performance_suite --parallel 1`
- `tests_generated/performance/` was restored to the fresh live baseline
  `performance_profile_tracked_non_gc_live_before_get_by_index_stable_result_reset_20260424-155848`
- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c` has no remaining
  diff after the revert

### Rejected get-index readonly-inline callback success early-return reshaping

Decision:

- rejected and reverted

Snapshot:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_fast_callback_success_early_return_20260424-161259`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe kept the same `GET_BY_INDEX` protocol but reshaped
  `object_try_call_hot_cached_known_native_get_by_index_readonly_inline_stack_operands(...)`
  so callback failure/status handling returned early instead of keeping the
  final `state->threadStatus == FINE && success` expression
- WSL gcc/clang focused guard binaries passed:
  - `zr_vm_object_call_known_native_fast_path_test`
  - `zr_vm_execution_member_access_fast_paths_test`
- the tracked non-GC profile signature stayed bit-for-bit identical to fresh
  live baseline across all eight cases
- representative Callgrind totals versus fresh live baseline were:
  - `numeric_loops`: `255,421,443 -> 255,429,715` (`+8,272`)
  - `dispatch_loops`: `349,859,320 -> 349,855,991` (`-3,329`)
  - `matrix_add_2d`: `12,024,576 -> 12,024,439` (`-137`)
  - `map_object_access`: `21,916,776 -> 21,942,157` (`+25,381`)
- because the target `map_object_access` path regressed materially while the
  hottest improvement was very small, the probe was rejected after the first
  rerun rather than promoted to confirm

Validation after revert:

- WSL gcc focused binary passes:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
- `tests_generated/performance/` was restored to the fresh live baseline
  `performance_profile_tracked_non_gc_live_before_get_by_index_stable_result_reset_20260424-155848`
- `zr_vm_core/src/zr_vm_core/object/object.c` has no remaining diff after the
  revert

### Rejected get-index readonly-inline hot-helper branch hints

Decision:

- rejected and reverted

Snapshot:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_hot_helper_unlikely_guards_20260424-161740`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe only wrapped the cold guard branches in
  `object_try_call_hot_cached_known_native_get_by_index_readonly_inline_stack_operands(...)`
  with `ZR_UNLIKELY(...)`
- WSL gcc/clang focused guard binaries passed:
  - `zr_vm_object_call_known_native_fast_path_test`
  - `zr_vm_execution_member_access_fast_paths_test`
- the tracked non-GC profile signature stayed bit-for-bit identical to fresh
  live baseline across all eight cases
- representative Callgrind totals versus fresh live baseline were:
  - `numeric_loops`: `255,421,443 -> 255,439,884` (`+18,441`)
  - `dispatch_loops`: `349,859,320 -> 349,849,702` (`-9,618`)
  - `matrix_add_2d`: `12,024,576 -> 12,017,913` (`-6,663`)
  - `map_object_access`: `21,916,776 -> 21,952,284` (`+35,508`)
- because the target readonly-index benchmark regressed materially, this pure
  branch-layout probe was rejected after the first rerun

Validation after revert:

- WSL gcc focused binary passes:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
- `zr_vm_core/src/zr_vm_core/object/object.c` has no remaining diff after the
  revert

### Retained re-applied `GETUPVAL` / `SETUPVAL` dispatch-local `stack_get_value` cut on live `085734`

Decision:

- kept in the live tree as the new live continuation checkpoint
- not promoted to a new accepted W1 checkpoint

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_getupval_setupval_stack_get_no_profile_reapply_20260423-094321`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_getupval_setupval_stack_get_no_profile_reapply_confirm_20260423-094444`

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Test coverage:

- `tests/core/test_execution_dispatch_callable_metadata.c`
  - `test_getupval_open_capture_path_avoids_stack_get_value_helpers`
  - `test_setupval_open_capture_path_avoids_stack_get_value_helpers`
- directly relevant instruction cases still pass during broader suite runs:
  - `test_getupval`
  - `test_setupval`

Reason:

- the current live tree had drifted back to `call_chain_polymorphic
  helpers.stack_get_value = 5160`, so this change explicitly restored the
  dispatch-local no-profile closure-frame read and no-profile open/closed
  upvalue dereference only for the steady-state `GETUPVAL` / `SETUPVAL` path
- both reruns versus kept live
  `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_short_string_canonical_compare_elision_confirm_20260423-085734`
  stabilized to the same isolated tracked delta:
  - seven of the eight tracked non-GC cases stayed bit-for-bit identical in
    `instructions/helpers/slowpaths`
  - `call_chain_polymorphic` was the only tracked change, and only on the
    intended helper line:
    - `stack_get_value`: `5160 -> 1324` (`-3836`)
- representative Callgrind totals versus kept live `085734` stayed bounded but
  mixed:
  - first rerun:
    - `numeric_loops`: `255,499,613 -> 255,467,406` (`-32,207`)
    - `dispatch_loops`: `349,844,840 -> 349,849,927` (`+5,087`)
    - `matrix_add_2d`: `12,016,640 -> 12,030,575` (`+13,935`)
    - `map_object_access`: `21,916,599 -> 21,954,175` (`+37,576`)
  - confirm rerun:
    - `numeric_loops`: `255,499,613 -> 255,466,835` (`-32,778`)
    - `dispatch_loops`: `349,844,840 -> 349,859,645` (`+14,805`)
    - `matrix_add_2d`: `12,016,640 -> 12,028,686` (`+12,046`)
    - `map_object_access`: `21,916,599 -> 21,928,034` (`+11,435`)
- the confirm rerun's hottest `dispatch_loops` functions moved only slightly:
  - `object_set_value_core`: `844,449 -> 847,834`
  - `ZrCore_String_Equal`: `474,732 -> 478,524`
  - `ZrCore_Object_GetValue`: `198,887 -> 199,351`
- unlike the rejected runtime-body probes nearby, both reruns preserved the same
  exact tracked signature change instead of flipping back to a wholly identical
  baseline; this is therefore a real targeted helper-count reduction, not just
  code-layout noise
- because the confirm rerun still drifts upward on three representative
  non-target cases, this should stay a kept live continuation rather than a new
  accepted W1 checkpoint

Validation after keep:

- WSL gcc build/test commands:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_execution_dispatch_callable_metadata_test zr_vm_instructions_test --parallel 16"`
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test && build-wsl-gcc/bin/zr_vm_instructions_test"`
- WSL clang build/test commands:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_execution_dispatch_callable_metadata_test zr_vm_instructions_test --parallel 16"`
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test && build-wsl-clang/bin/zr_vm_instructions_test"`
- benchmark profile command:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop cmake -DCLI_EXE=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_cli -DPERF_RUNNER_EXE=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_perf_runner -DNATIVE_BENCHMARK_EXE=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_native_benchmark_runner -DBENCHMARKS_DIR=/mnt/e/Git/zr_vm/tests/benchmarks -DGENERATED_DIR=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated -DHOST_BINARY_DIR=/mnt/e/Git/zr_vm/build/benchmark-gcc-release -P /mnt/e/Git/zr_vm/tests/cmake/run_performance_suite.cmake"`
- new helper-profile tests pass on both gcc and clang and confirm the local
  open-capture path now records zero profiled `stack_get_value` helpers
- broader `zr_vm_instructions_test` still reports two unrelated current-tree
  failures on the remembered young receiver GC rewrite line:
  - `test_get_member_slot_instruction_minor_gc_rewrites_forwarded_young_receiver_pair_before_permanent_pic_prune`
  - `test_set_member_slot_instruction_minor_gc_rewrites_forwarded_young_receiver_pair_before_permanent_pic_prune`
- `tests_generated/performance/` now reflects kept live confirm:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_getupval_setupval_stack_get_no_profile_reapply_confirm_20260423-094444`

### Rejected dispatch profiled-stack `value_copy` use-site cut

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_value_copy_profiled_stack_lane_20260423-055747`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_value_copy_profiled_stack_lane_confirm_20260423-060021`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Guard coverage kept in tree:

- `tests/core/test_execution_dispatch_callable_metadata.c`
- `test_execute_get_constant_profiled_stack_destination_records_one_value_copy_helper`
- `test_execute_get_stack_profiled_stack_destination_records_one_value_copy_helper`

Reason:

- this probe tried to keep the exact same helper-counting semantics on the
  hottest `GET_CONSTANT` / `GET_STACK` stack-destination copies, but stop
  routing profiled stack copies through the extra
  `FAST_PREPARE_DESTINATION_FROM_OFFSET(...)` plus
  `execution_copy_value_fast(...)` wrapper
- both reruns stayed bit-for-bit identical to kept live `232733` in
  `instructions/helpers/slowpaths` across all eight tracked non-GC cases, so
  this remained a pure runtime-body/code-layout experiment
- targeted helper totals stayed frozen:
  - `dispatch_loops value_copy`: `2,889,069 -> 2,889,069`
  - `dispatch_loops get_member`: `307,431 -> 307,431`
  - `dispatch_loops precall`: `153,852 -> 153,852`
- representative totals versus kept live `232733` would not hold:
  - first rerun:
    - `numeric_loops`: `255,479,313 -> 255,483,258` (`+3,945`)
    - `dispatch_loops`: `349,867,883 -> 349,880,489` (`+12,606`)
    - `matrix_add_2d`: `12,044,572 -> 12,038,213` (`-6,359`)
    - `map_object_access`: `21,949,857 -> 21,943,572` (`-6,285`)
  - confirm rerun:
    - `numeric_loops`: `255,479,313 -> 255,513,650` (`+34,337`)
    - `dispatch_loops`: `349,867,883 -> 349,892,092` (`+24,209`)
    - `matrix_add_2d`: `12,044,572 -> 12,052,047` (`+7,475`)
    - `map_object_access`: `21,949,857 -> 21,948,448` (`-1,409`)
- because confirm again pushed the two hottest representative cases above kept
  live while changing none of the tracked signature, the slice does not qualify
  as a W1 runtime-cost keep

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`

### Hot-map lookup second-pair carry-through

Decision:

- rejected

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_hot_map_lookup_second_pair_carry_20260423-044229`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_hot_map_lookup_second_pair_carry_confirm_20260423-044229`

Reason:

- this container-map probe tried to carry the second readonly lookup pair farther
  through the hot lookup/update path
- both reruns stayed bit-for-bit identical to kept live `232733` in tracked
  `instructions/helpers/slowpaths` across all eight non-GC cases, so the change
  never moved opcode mix, quickening, or helper signature
- representative totals versus kept live `232733` were:
  - first rerun:
    - `numeric_loops`: `255,479,313 -> 255,505,952` (`+26,639`)
    - `dispatch_loops`: `349,867,883 -> 349,898,060` (`+30,177`)
    - `matrix_add_2d`: `12,044,572 -> 12,047,877` (`+3,305`)
    - `map_object_access`: `21,949,857 -> 21,979,913` (`+30,056`)
  - confirm rerun:
    - `numeric_loops`: `255,479,313 -> 255,486,287` (`+6,974`)
    - `dispatch_loops`: `349,867,883 -> 349,883,873` (`+15,990`)
    - `matrix_add_2d`: `12,044,572 -> 12,055,567` (`+10,995`)
    - `map_object_access`: `21,949,857 -> 21,957,526` (`+7,669`)
- the target helper stayed flat while adjacent hot bodies regressed:
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`: `401,595 -> 401,595`
  - `zr_container_map_get_item_readonly_inline_fast`: `871,623 -> 875,744` on first rerun
  - `zr_container_map_set_item_readonly_inline_no_result_fast`: `375,262 -> 387,621` on first rerun
  - `object_set_value_core`: `844,049 -> 847,807` on first rerun
- this exact hot lookup second-pair carry-through idea is therefore another pure
  runtime-body regression and should not be retried

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
  - `build-wsl-gcc/bin/zr_vm_container_temp_value_root_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`
  - `build-wsl-clang/bin/zr_vm_container_temp_value_root_test`

### Dispatch-level single-slot exact-receiver cached hot fast

Decision:

- rejected

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_single_slot_exact_receiver_cached_hot_fast_20260423-051724`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_single_slot_exact_receiver_cached_hot_fast_confirm_20260423-051724`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_internal.h`
- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- this probe fronted the single-slot exact-receiver cached slow lanes directly
  in dispatch for:
  - object/member-name
  - callable
  - descriptor fallback
- both reruns stayed bit-for-bit identical to kept live `232733` in tracked
  `instructions/helpers/slowpaths` across all eight non-GC cases, and the
  dispatch helper / opcode mix also stayed frozen:
  - `value_copy = 2,889,069`
  - `get_member = 307,431`
  - `precall = 153,852`
  - `GET_MEMBER_SLOT = 307,446`
  - `SET_MEMBER_SLOT = 153,604`
- representative totals versus kept live `232733` were:
  - first rerun:
    - `numeric_loops`: `255,479,313 -> 255,483,504` (`+4,191`)
    - `dispatch_loops`: `349,867,883 -> 350,507,200` (`+639,317`)
    - `matrix_add_2d`: `12,044,572 -> 12,039,298` (`-5,274`)
    - `map_object_access`: `21,949,857 -> 21,984,306` (`+34,449`)
  - confirm rerun:
    - `numeric_loops`: `255,479,313 -> 255,522,527` (`+43,214`)
    - `dispatch_loops`: `349,867,883 -> 350,512,036` (`+644,153`)
    - `matrix_add_2d`: `12,044,572 -> 12,047,639` (`+3,067`)
    - `map_object_access`: `21,949,857 -> 21,983,842` (`+33,985`)
- hottest `dispatch_loops` bodies also regressed on both reruns:
  - first rerun:
    - `object_set_value_core`: `844,049 -> 847,866`
    - `ZrCore_String_Equal`: `495,196 -> 499,604`
    - `ZrCore_Object_GetValue`: `197,157 -> 198,021`
  - confirm rerun:
    - `object_set_value_core`: `844,049 -> 850,363`
    - `ZrCore_String_Equal`: `495,196 -> 501,764`
    - `ZrCore_Object_GetValue`: `197,157 -> 198,250`
- related map-side helpers also stayed flat enough to rule out any side benefit:
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`: `401,595 -> 401,595`
  - `zr_container_map_get_item_readonly_inline_fast`: `871,623 -> 871,623`
- this exact dispatch-level hot-fast bypass is therefore another pure
  runtime-body/code-layout regression and should not be retried

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`

### Readonly-inline index hot ready-flag gate

Decision:

- rejected

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_hot_ready_flag_gate_20260423-053949`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_hot_ready_flag_gate_confirm_20260423-053949`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `tests/core/test_object_call_known_native_fast_path.c`

Reason:

- this probe tried to make the hottest stack-operands readonly-inline get/set
  helpers trust the cached `reserved1` hot-ready bits directly and stop
  re-running the shape-match insurance there
- both reruns stayed bit-for-bit identical to kept live `232733` in tracked
  `instructions/helpers/slowpaths` across all eight non-GC cases
- representative totals versus kept live `232733` were:
  - first rerun:
    - `numeric_loops`: `255,479,313 -> 255,476,010` (`-3,303`)
    - `dispatch_loops`: `349,867,883 -> 349,881,984` (`+14,101`)
    - `matrix_add_2d`: `12,044,572 -> 12,040,992` (`-3,580`)
    - `map_object_access`: `21,949,857 -> 21,950,294` (`+437`)
  - confirm rerun:
    - `numeric_loops`: `255,479,313 -> 255,495,666` (`+16,353`)
    - `dispatch_loops`: `349,867,883 -> 349,889,611` (`+21,728`)
    - `matrix_add_2d`: `12,044,572 -> 12,051,066` (`+6,494`)
    - `map_object_access`: `21,949,857 -> 21,957,848` (`+7,991`)
- target helper totals moved by only noise-level amounts:
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`: `401,595 -> 401,592`
  - `ZrCore_Object_TrySetByIndexReadonlyInlineFastStackOperands`: `176,294 -> 176,292`
- hotter adjacent bodies also failed to hold a gain on confirm:
  - `object_set_value_core`: `476,496 -> 476,931`
  - `ZrCore_String_Equal`: `110,597 -> 112,221`
  - `ZrCore_Object_GetValue`: `108,448 -> 109,930`
- this exact hot-ready-flag gate is therefore another pure runtime-body/code-layout
  regression and should not be retried

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`

### Rejected cached descriptor-hit follow-up pair backfill

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_hit_followup_pair_backfill_20260423-035810`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_hit_followup_pair_backfill_confirm_20260423-040312`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`
- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe tried to shrink the remaining cached descriptor fallback on cached
  get/set:
  - after descriptor success, backfill `cachedMemberName` from descriptor
    metadata and recover `cachedReceiverPair` only when the receiver's cached
    string lookup already matched that descriptor name
  - on the set side, steady-state field/static-member updates on
    `targetObject == object` probed
    `object_get_own_string_pair_by_name_cached_unchecked(...)` before
    materializing `memberKey`
- both reruns again kept all eight tracked non-GC
  `instructions/helpers/slowpaths` bit-for-bit identical to kept live `232733`,
  so this stayed a pure runtime-body/code-layout experiment rather than a new
  specialization win
- representative totals versus kept live `232733` were:
  - first rerun:
    - `numeric_loops`: `255,479,313 -> 255,544,381` (`+65,068`)
    - `dispatch_loops`: `349,867,883 -> 349,878,604` (`+10,721`)
    - `matrix_add_2d`: `12,044,572 -> 12,047,143` (`+2,571`)
    - `map_object_access`: `21,949,857 -> 21,929,555` (`-20,302`)
  - confirm rerun:
    - `numeric_loops`: `255,479,313 -> 255,479,994` (`+681`)
    - `dispatch_loops`: `349,867,883 -> 349,890,908` (`+23,025`)
    - `matrix_add_2d`: `12,044,572 -> 12,052,076` (`+7,504`)
    - `map_object_access`: `21,949,857 -> 21,933,978` (`-15,879`)
- hottest `dispatch_loops` bodies also failed to improve:
  - first rerun:
    - `object_set_value_core`: `844,049 -> 847,014`
    - `ZrCore_String_Equal`: `495,196 -> 497,068`
    - `ZrCore_Object_GetValue`: `197,157 -> 196,050`
  - confirm rerun:
    - `object_set_value_core`: `844,049 -> 846,988`
    - `ZrCore_String_Equal`: `495,196 -> 500,124`
    - `ZrCore_Object_GetValue`: `197,157 -> 199,184`
- under the current W1 order, giving back `dispatch_loops` and
  `matrix_add_2d` on both reruns for a bounded `map_object_access` gain is not
  acceptable, so this exact descriptor-follow-up backfill idea is not worth
  retrying

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`

### Rejected readonly-index precleared-result helper split

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_readonly_inline_precleared_result_20260423-024402`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_readonly_inline_precleared_result_confirm_20260423-024402`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe split `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands(...)`
  into a precleared-result variant so the readonly inline index path would stop
  re-zeroing the output slot before the real lookup
- both reruns stayed bit-for-bit identical to kept live `232733` in
  `instructions/helpers/slowpaths` across all eight tracked non-GC cases, so
  there was still no opcode-mix, helper-signature, or quickening change
- representative Callgrind totals versus kept live `232733` were:
  - first rerun:
    - `numeric_loops`: `255,479,313 -> 255,498,488` (`+19,175`)
    - `dispatch_loops`: `349,867,883 -> 349,890,977` (`+23,094`)
    - `matrix_add_2d`: `12,044,572 -> 12,037,447` (`-7,125`)
    - `map_object_access`: `21,949,857 -> 21,948,254` (`-1,603`)
  - confirm rerun:
    - `numeric_loops`: `255,479,313 -> 255,494,279` (`+14,966`)
    - `dispatch_loops`: `349,867,883 -> 349,898,714` (`+30,831`)
    - `matrix_add_2d`: `12,044,572 -> 12,050,311` (`+5,739`)
    - `map_object_access`: `21,949,857 -> 21,943,376` (`-6,481`)
- the target helper itself stayed flat on `map_object_access`:
  - `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`
    / `...PreclearedResult`: `401,595 Ir`
- meanwhile the hotter `dispatch_loops` bodies moved the wrong way on the
  confirm rerun:
  - `object_set_value_core`: `844,049 -> 849,207`
  - `ZrCore_String_Equal`: `495,196 -> 501,916`
  - `ZrCore_Object_GetValue`: `197,157 -> 198,354`
- because the helper split did not reduce the target helper total and still
  regressed the highest-priority representative case on an otherwise identical
  tracked signature, this exact readonly-index idea is not worth retrying

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
  - `build-wsl-gcc/bin/zr_vm_container_temp_value_root_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`
  - `build-wsl-clang/bin/zr_vm_container_temp_value_root_test`

### Rejected cached descriptor-set assigned-value copy-elision

Decision:

- rejected and reverted

Snapshots:

- rejected probe:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_descriptor_set_assigned_value_copy_elision_20260423-075852`
- fresh live rebaseline after revert:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_cached_descriptor_set_assigned_value_copy_elision_revert_20260423-080558`
- nearby already-live generic-call confirm snapshot used for attribution check:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_generic_call_call_hook_mask_narrowing_confirm_20260423-072947`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_member_access.c`

Reason:

- this probe removed the local `stableAssignedValue = *assignedValue` materialization from the four non-stack cached descriptor-set paths in `execution_member_set_cached(...)`
- the fresh tracked non-GC rerun on the probe and the fresh rerun after restoring the local copy both stayed bit-for-bit identical across all eight tracked non-GC `instructions/helpers/slowpaths`
- the rejected probe was also bit-for-bit identical to the nearby generic-call confirm snapshot, including the already-live `call_chain_polymorphic` helper residue:
  - `stack_get_value = 5160`
- that proves the current live tree still carries the pre-existing `call_chain` helper drift and that reverting the descriptor-set copy-elision probe did not restore the earlier `1324` level
- representative totals versus kept live `232733` after the revert were:
  - `numeric_loops`: `255,479,313 -> 255,473,064` (`-6,249`)
  - `dispatch_loops`: `349,867,883 -> 349,877,922` (`+10,039`)
  - `matrix_add_2d`: `12,044,572 -> 12,045,086` (`+514`)
  - `map_object_access`: `21,949,857 -> 21,964,676` (`+14,819`)
- the probe therefore did not establish a clean new runtime-body win and has been removed from the live tree

Validation after revert:

- focused benchmark gcc direct binaries pass:
  - `build/benchmark-gcc-release/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build/benchmark-gcc-release/bin/zr_vm_precall_frame_slot_reset_test`
  - `build/benchmark-gcc-release/bin/zr_vm_value_copy_fast_paths_test`
  - `build/benchmark-gcc-release/bin/zr_vm_execution_member_access_fast_paths_test`
- `tests_generated/performance/` now reflects the fresh live rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_cached_descriptor_set_assigned_value_copy_elision_revert_20260423-080558`

### Container hot-lookup stats contract in release builds

Decision:

- fixed in test coverage only; no runtime production change

Affected test:

- `tests/container/test_container_runtime.c`
- scenario:
  - `test_container_map_runtime_stable_concat_keys_avoid_entry_slot_validation_on_hot_lookup`

Reason:

- the apparent lower-layer regression was not in map lookup, string identity, or member-version invalidation
- root cause was build-mode contract mismatch:
  - `ZrVmLibContainer_Debug_GetHotMapLookupStats(...)` returns live counters only under `ZR_DEBUG`
  - the underlying `hotHitCount` / `memberVersionHitCount` increments in `zr_vm_lib_container/src/zr_vm_lib_container/module.c` are also compiled only under `#if defined(ZR_DEBUG)`
- `build/benchmark-gcc-release/bin/zr_vm_container_runtime_test` therefore correctly observed zeroed debug stats while `build-wsl-gcc/bin/zr_vm_container_runtime_test` observed positive counters
- the test now asserts:
  - positive hot-hit counters in debug builds
  - zeroed counters in release builds
  - `entryValidationReadCount == 0` in both builds

Validation:

- `wsl bash -lc "cd /mnt/e/Git/zr_vm/build/benchmark-gcc-release && ./bin/zr_vm_container_runtime_test"` passes
- `wsl bash -lc "cd /mnt/e/Git/zr_vm/build-wsl-gcc && ./bin/zr_vm_container_runtime_test"` passes

### Retained short-string canonical compare elision

Decision:

- retained as the new live-tree continuation checkpoint
- not promoted to a new accepted W1 baseline

Snapshots:

- pre-slice live baseline:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_live_rebaseline_after_cached_descriptor_set_assigned_value_copy_elision_revert_20260423-080558`
- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_short_string_canonical_compare_elision_20260423-085335`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_short_string_canonical_compare_elision_confirm_20260423-085734`

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_lib_container/src/zr_vm_lib_container/module.c`

Reason:

- this cut removes redundant `ZrCore_String_Equal(...)` calls after pointer misses on already-canonical short-string lanes:
  - hidden-items known-field checks in `object_key_matches_known_field(...)`
  - map string-key equality fallback in `zr_container_map_find_entry*_fast(...)`
- content comparison remains enabled for long strings, which are still allowed to exist as distinct objects with equal text
- both reruns stayed bit-for-bit identical to live `080558` across all eight tracked non-GC `instructions/helpers/slowpaths`, so the slice remains a pure runtime-body reduction instead of an opcode-mix or quickening shift
- representative totals versus live `080558`:
  - first rerun:
    - `numeric_loops`: `255,473,064 -> 255,468,396` (`-4,668`)
    - `dispatch_loops`: `349,877,922 -> 349,856,430` (`-21,492`)
    - `matrix_add_2d`: `12,045,086 -> 12,026,788` (`-18,298`)
    - `map_object_access`: `21,964,676 -> 21,923,732` (`-40,944`)
  - confirm rerun:
    - `numeric_loops`: `255,473,064 -> 255,499,613` (`+26,549`)
    - `dispatch_loops`: `349,877,922 -> 349,844,840` (`-33,082`)
    - `matrix_add_2d`: `12,045,086 -> 12,016,640` (`-28,446`)
    - `map_object_access`: `21,964,676 -> 21,916,599` (`-48,077`)
- confirm function movement versus live `080558` shows the intended hot body shifted down:
  - `dispatch_loops`:
    - `object_set_value_core`: `845,995 -> 844,449`
    - `ZrCore_String_Equal`: `497,292 -> 474,732`
    - `ZrCore_Object_GetValue`: `197,664 -> 198,887`
  - `map_object_access`:
    - `object_set_value_core`: `475,697 -> 474,644`
    - `ZrCore_String_Equal`: `109,989 -> 94,701`
    - `ZrCore_Object_GetValue`: `108,666 -> 107,080`
  - `numeric_loops`:
    - `object_set_value_core`: `831,057 -> 838,409`
    - `ZrCore_String_Equal`: `112,932 -> 103,852`
- compared to accepted `055330`, the confirm rerun is lower on three of the four representative cases:
  - `numeric_loops`: `255,474,138 -> 255,499,613` (`+25,475`)
  - `dispatch_loops`: `350,330,462 -> 349,844,840` (`-485,622`)
  - `matrix_add_2d`: `12,053,512 -> 12,016,640` (`-36,872`)
  - `map_object_access`: `21,931,485 -> 21,916,599` (`-14,886`)
- because the confirm rerun still leaves `numeric_loops` above accepted `055330`, this cut is worth keeping only as the current live continuation rather than as a new accepted checkpoint

Validation on the kept tree:

- benchmark gcc focused direct binaries pass:
  - `build/benchmark-gcc-release/bin/zr_vm_container_runtime_test`
  - `build/benchmark-gcc-release/bin/zr_vm_container_temp_value_root_test`
  - `build/benchmark-gcc-release/bin/zr_vm_value_copy_fast_paths_test`
  - `build/benchmark-gcc-release/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build/benchmark-gcc-release/bin/zr_vm_execution_dispatch_callable_metadata_test`
- WSL gcc focused direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
  - `build-wsl-gcc/bin/zr_vm_container_temp_value_root_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
- `tests_generated/performance/` now reflects kept live confirm:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_short_string_canonical_compare_elision_confirm_20260423-085734`

### Rejected hidden-items cache pair targeting

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_hidden_items_cache_pair_targeting_reject_20260423-032404`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

Reason:

- this probe tried to narrow the hidden-items cache guard so non-hidden
  existing-pair writes would still take the fast plain-value update path when
  `cachedHiddenItemsObject` was populated but the target pair was not the actual
  hidden-items pair
- the fresh tracked rerun again stayed bit-for-bit identical to kept live
  `232733` in `instructions/helpers/slowpaths` across all eight tracked non-GC
  cases, so the change was still only a runtime-body/code-layout experiment
- representative Callgrind totals versus kept live `232733` were:
  - `numeric_loops`: `255,479,313 -> 255,480,981` (`+1,668`)
  - `dispatch_loops`: `349,867,883 -> 349,877,822` (`+9,939`)
  - `matrix_add_2d`: `12,044,572 -> 12,050,080` (`+5,508`)
  - `map_object_access`: `21,949,857 -> 21,921,957` (`-27,900`)
- function-level movement on hottest `dispatch_loops` also stayed negative:
  - `object_set_value_core`: `844,049 -> 845,365`
  - `ZrCore_String_Equal`: `495,196 -> 497,340`
  - `ZrCore_Object_GetValue`: `197,157 -> 198,200`
- the same profile-tier rerun also produced large single-iteration wall-time
  drift (`dispatch_loops +38.75%`, `numeric_loops +44.28%`) while Callgrind
  stayed nearly flat; that mismatch was treated as host-noise evidence rather
  than as a reason to keep a runtime-body cut with worse representative Ir
- under the current W1 order, giving back hottest `dispatch_loops` for a small
  `map_object_access` win is not acceptable, so the slice was removed and the
  old hidden-items semantics restored

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`
  - `build-wsl-gcc/bin/zr_vm_container_temp_value_root_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`
  - `build-wsl-clang/bin/zr_vm_container_temp_value_root_test`

### Accepted get-index stack readonly-inline helper split live continuation

Decision:

- accepted as the current live continuation
- not promoted to a new accepted W1 checkpoint by itself

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_20260424-162649`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this cut keeps the non-stack readonly-inline get path on the existing shared
  mode helper, but expands the stack-operands wrapper into its own fast body
  so the hottest `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`
  path no longer carries the non-stack residency branch shape through the
  shared mode helper
- behavior stays unchanged:
  - debug hook, callback readiness, shape fallback, null-result-on-fine-miss,
    and thread-status handling are identical to the previous stack-operands
    mode
  - non-stack `object_try_call_cached_known_native_get_by_index_readonly_inline`
    still uses the existing `object_value_resides_on_vm_stack(...)` guard
- both reruns kept all eight tracked non-GC `instructions/helpers/slowpaths`
  profile signatures bit-for-bit identical to the fresh live baseline
  `performance_profile_tracked_non_gc_live_before_get_by_index_stable_result_reset_20260424-155848`
  after excluding the two GC fragment cases, so this is still a bounded
  runtime-body/code-layout cut rather than opcode-mix or quickening drift
- representative totals versus fresh live baseline `155848`:
  - first rerun:
    - `numeric_loops`: `255,421,443 -> 255,423,155` (`+1,712`)
    - `dispatch_loops`: `349,859,320 -> 349,818,305` (`-41,015`)
    - `matrix_add_2d`: `12,024,576 -> 12,015,169` (`-9,407`)
    - `map_object_access`: `21,916,776 -> 21,906,366` (`-10,410`)
  - confirm rerun:
    - `numeric_loops`: `255,421,443 -> 255,428,731` (`+7,288`)
    - `dispatch_loops`: `349,859,320 -> 349,832,789` (`-26,531`)
    - `matrix_add_2d`: `12,024,576 -> 12,009,340` (`-15,236`)
    - `map_object_access`: `21,916,776 -> 21,881,343` (`-35,433`)
- the target helper remains the top profiled helper for `map_object_access`,
  but the total case body moved down on both reruns:
  - first: `21,916,776 -> 21,906,366`
  - confirm: `21,916,776 -> 21,881,343`
- relative to accepted `055330`, the confirm rerun stays lower on all four
  representative cases:
  - `numeric_loops`: `255,474,138 -> 255,428,731` (`-45,407`)
  - `dispatch_loops`: `350,330,462 -> 349,832,789` (`-497,673`)
  - `matrix_add_2d`: `12,053,512 -> 12,009,340` (`-44,172`)
  - `map_object_access`: `21,931,485 -> 21,881,343` (`-50,142`)

Validation on the kept tree:

- benchmark gcc release direct binaries pass:
  - `build/benchmark-gcc-release/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build/benchmark-gcc-release/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
- Windows MSVC CLI smoke passes:
  - configure/build `build/codex-msvc-cli-debug` with `BUILD_TESTS=OFF`
  - `build/codex-msvc-cli-debug/bin/Debug/zr_vm_cli.exe tests/fixtures/projects/hello_world/hello_world.zrp`
    prints `hello world`
- `tests_generated/performance/` now reflects kept live confirm:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`

### Rejected set-index stack readonly-inline helper split follow-up

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_set_index_stack_readonly_inline_split_20260424-163541`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe tried the symmetric set-side version of the kept get-side split:
  `object_try_call_cached_known_native_set_by_index_readonly_inline_stack_operands(...)`
  was expanded into a stack-operands-only body instead of calling the shared
  mode helper with `ZR_TRUE`
- the focused correctness guard accepted the probe, but the first tracked
  non-GC Callgrind rerun regressed both the hottest dispatch case and the
  target map case relative to the kept get-side live continuation
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`
- representative totals versus kept get-side live:
  - `numeric_loops`: `255,428,731 -> 255,420,645` (`-8,086`)
  - `dispatch_loops`: `349,832,789 -> 349,836,098` (`+3,309`)
  - `matrix_add_2d`: `12,009,340 -> 11,989,753` (`-19,587`)
  - `map_object_access`: `21,881,343 -> 21,901,174` (`+19,831`)
- because W1 is still prioritizing hottest `dispatch_loops` and the readonly
  index target `map_object_access`, this was not worth confirming; it was
  removed after the first rerun

Validation around the rejected probe:

- WSL gcc direct binaries passed before the perf rerun:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries passed before the perf rerun:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
- after revert, `tests_generated/performance/` was restored to kept get-side
  live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`

### Rejected get-index stack raw object cast follow-up

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_raw_object_cast_20260424-164038`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe tried to narrow the kept get-side stack readonly-inline entry:
  `object_try_get_by_index_readonly_inline_fast_stack_operands(...)` used
  `ZR_CAST(SZrObject *, receiver->value.object)` after the local receiver type
  and null guards, instead of the checked `ZR_CAST_OBJECT(...)`
- focused WSL gcc/clang readonly-index/native-fast-path guards passed, but the
  first tracked non-GC Callgrind rerun regressed both hottest dispatch and the
  target map case relative to the kept get-side live continuation
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`
- representative totals versus kept get-side live:
  - `numeric_loops`: `255,428,731 -> 255,430,562` (`+1,831`)
  - `dispatch_loops`: `349,832,789 -> 349,838,800` (`+6,011`)
  - `matrix_add_2d`: `12,009,340 -> 11,999,555` (`-9,785`)
  - `map_object_access`: `21,881,343 -> 21,903,393` (`+22,050`)
- because this was another two-regression / one-target-regression layout probe,
  it was removed without a confirm run

Validation around the rejected probe:

- WSL gcc direct binary passed before the perf rerun:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
- WSL clang direct binary passed before the perf rerun:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`
- after revert, `tests_generated/performance/` was restored to kept get-side
  live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`

### Rejected get-index direct-miss state-based reset follow-up

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_direct_miss_state_reset_20260424-164549`

Production probe:

- `zr_vm_core/src/zr_vm_core/object/object.c`

Reason:

- this probe tried to keep the `value_reset_null` helper count stable while
  avoiding the exported `ZrCore_Value_ResetAsNull(...)` call at the
  `object_get_by_index_unchecked_core(...)` direct-index miss site:
  a file-local helper recorded `ZR_PROFILE_HELPER_VALUE_RESET_NULL` from
  `state` and then called `ZrCore_Value_ResetAsNullNoProfile(...)`
- focused WSL gcc/clang object/member fast-path guards passed, but the first
  tracked non-GC Callgrind rerun regressed the target map case and slightly
  regressed matrix while only improving `dispatch_loops`
- representative totals versus kept get-side live
  `performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`:
  - `numeric_loops`: `255,428,731 -> 255,441,570` (`+12,839`)
  - `dispatch_loops`: `349,832,789 -> 349,800,215` (`-32,574`)
  - `matrix_add_2d`: `12,009,340 -> 12,010,141` (`+801`)
  - `map_object_access`: `21,881,343 -> 21,896,605` (`+15,262`)
- the target `map_object_access` regression makes this unsuitable for the
  current W1 order, so it was removed without a confirm run

Validation around the rejected probe:

- WSL gcc direct binaries passed before the perf rerun:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
- WSL clang direct binaries passed before the perf rerun:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
- after revert, `tests_generated/performance/` was restored to kept get-side
  live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_index_stack_readonly_inline_split_confirm_20260424-162914`

### Rejected prepared-resolved direct-VM-function `precall` steady-state dedup

Decision:

- rejected and reverted

Snapshots:

- first rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_precall_prepared_resolved_vm_steady_state_miss_dedup_reject_20260423-010009`
- confirm rerun:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_precall_prepared_resolved_vm_steady_state_miss_dedup_confirm_reject_20260423-021106`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- this probe tried to trim the hottest prepared resolved VM member-call `precall`
  body only:
  - `execution_store_resolved_vm_function_value_fast(...)` returned early when
    the callable slot already held the same normalized direct VM function value
- the target was the steady-state `KNOWN_VM_MEMBER_CALL` path where the runtime
  still rematerializes the same direct callable value before entering the VM
  frame
- both reruns again kept all eight tracked non-GC cases bit-for-bit identical
  to kept live `232733` in `instructions/helpers/slowpaths`, so this remained a
  pure runtime-body/code-layout experiment
- relative to kept live `232733`, the representative totals were:
  - first rerun:
    - `numeric_loops`: `255,479,313 -> 255,482,680` (`+3,367`)
    - `dispatch_loops`: `349,867,883 -> 349,880,206` (`+12,323`)
    - `matrix_add_2d`: `12,044,572 -> 12,046,179` (`+1,607`)
    - `map_object_access`: `21,949,857 -> 21,954,030` (`+4,173`)
  - confirm rerun:
    - `numeric_loops`: `255,479,313 -> 255,480,927` (`+1,614`)
    - `dispatch_loops`: `349,867,883 -> 350,336,294` (`+468,411`)
    - `matrix_add_2d`: `12,044,572 -> 12,051,418` (`+6,846`)
    - `map_object_access`: `21,949,857 -> 21,940,431` (`-9,426`)
- relative to accepted `055330`, the confirm rerun also lost the W1 checkpoint
  on three of the four representative cases:
  - `numeric_loops`: `255,474,138 -> 255,480,927` (`+6,789`)
  - `dispatch_loops`: `350,330,462 -> 350,336,294` (`+5,832`)
  - `matrix_add_2d`: `12,053,512 -> 12,051,418` (`-2,094`)
  - `map_object_access`: `21,931,485 -> 21,940,431` (`+8,946`)
- because even the first rerun already regressed hottest `dispatch_loops`, and
  the confirm rerun turned into a clear `dispatch_loops` blow-up on the same
  tracked signature, the slice was discarded instead of being kept as another
  ambiguous `precall` cleanup

Validation after revert:

- restored `tests_generated/performance/` to kept live evidence:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_get_null_key_descriptor_name_cache_confirm_20260422-232733`
- WSL gcc direct binaries pass:
  - `build-wsl-gcc/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-gcc/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-gcc/bin/zr_vm_precall_frame_slot_reset_test`
- WSL clang direct binaries pass:
  - `build-wsl-clang/bin/zr_vm_execution_numeric_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`
  - `build-wsl-clang/bin/zr_vm_value_copy_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_execution_member_access_fast_paths_test`
  - `build-wsl-clang/bin/zr_vm_precall_frame_slot_reset_test`

### Kept GET_BY_INDEX fast result dispatch-copy writeback

Decision:

- kept as bounded live continuation

Snapshots:

- first:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_20260424-continue`
- confirm:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_confirm_20260424-continue`
- final live restore point:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_final_20260424-continue`

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- this probe changes only the `GET_BY_INDEX` opcode tail after a resolved
  `stableResult`
- the final writeback uses the dispatch-local profile state:
  `execution_copy_value_fast(state, destination, &stableResult, profileRuntime,
  recordHelpers)`
- this replaces `ZrCore_Value_Copy(...)`, avoiding another TLS current-profile
  helper record while preserving the `value_copy` helper count through
  `execution_copy_value_fast(...)`
- `execution_dispatch.c` remains oversized, but this probe only replaces an
  existing statement inside one opcode arm and does not add a new helper,
  protocol, or responsibility

Representative totals versus accepted checked-object confirm
`performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`:

| run | `numeric_loops` delta | `dispatch_loops` delta | `matrix_add_2d` delta | `map_object_access` delta |
| --- | ---: | ---: | ---: | ---: |
| first | +6,788 | -5,106 | -4,671 | -8,231 |
| confirm | -13,892 | -16,130 | -17,473 | -113 |
| tie-break | -2,916 | -7,278 | +8,340 | +18,715 |
| final | -1,328 | +505 | -2,263 | -5,922 |

All four checked tracked non-GC runs kept `instructions/helpers/slowpaths`
signatures identical to the accepted checked-object confirm baseline. The
effect is deliberately classified as small runtime-body/layout movement: most
representative cells improve, but the tie-break and final runs still show
bounded noise. It is therefore kept as live continuation, not promoted as a new
headline W1 checkpoint.

Validation before performance reruns:

- WSL gcc direct binaries passed:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`: 57 tests, 0 failures
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`: 39 tests, 0 failures
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures
- WSL clang direct binaries passed:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`: 57 tests, 0 failures
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`: 39 tests, 0 failures
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures

### Rejected SET_BY_INDEX fast-hit destination reload skip

Decision:

- rejected and reverted

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_set_by_index_fast_hit_destination_reload_skip_20260424-continue`

Production probe:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`

Reason:

- this probe removed the `destination = E(instruction) == ...` reload from the
  `SET_BY_INDEX` readonly-inline fast-hit branch
- it kept `UPDATE_BASE(callInfo)`, because the callback can still require base
  refresh before the next dispatch
- the opcode success path does not read `destination` again before `DONE(1)`,
  so this looked like a narrow dead-tail cleanup
- `execution_dispatch.c` remains oversized, but this was a one-line probe in an
  existing opcode arm and did not add a responsibility

Representative totals versus current live
`performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_final_20260424-continue`:

| run | `numeric_loops` delta | `dispatch_loops` delta | `matrix_add_2d` delta | `map_object_access` delta |
| --- | ---: | ---: | ---: | ---: |
| first | +19,815 | -22,895 | +5,217 | -15,121 |
| confirm | +49,713 | +2,346 | +10,804 | -34,801 |

Both checked tracked non-GC runs kept `instructions/helpers/slowpaths`
signatures identical to current live. The map target improved in both runs,
but the hottest dispatch case failed to confirm and the numeric/matrix
regressions were too large. The probe was reverted, and
`tests_generated/performance/` was restored to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_final_20260424-continue`

Validation around the rejected probe:

- WSL gcc direct binaries passed before perf:
  - `build-wsl-gcc/bin/zr_vm_object_call_known_native_fast_path_test`: 57 tests, 0 failures
  - `build-wsl-gcc/bin/zr_vm_container_runtime_test`: 39 tests, 0 failures
  - `build-wsl-gcc/bin/zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures
- WSL clang direct binaries passed before perf:
  - `build-wsl-clang/bin/zr_vm_object_call_known_native_fast_path_test`: 57 tests, 0 failures
  - `build-wsl-clang/bin/zr_vm_container_runtime_test`: 39 tests, 0 failures
  - `build-wsl-clang/bin/zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures
