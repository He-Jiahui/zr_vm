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
