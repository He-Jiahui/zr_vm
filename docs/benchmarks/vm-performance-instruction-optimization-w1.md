---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - tests/acceptance/2026-04-21-w1-t7-t8-existing-pair-slow-lane.md
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/object/object.c
plan_sources:
  - .codex/plans/虚拟机性能与指令优化二阶段计划.md
tests:
  - build/codex-wsl-gcc-debug/bin/zr_vm_object_call_known_native_fast_path_test
  - build/codex-wsl-gcc-debug/bin/zr_vm_container_runtime_test
  - build/codex-wsl-gcc-debug/bin/zr_vm_execution_dispatch_callable_metadata_test
  - build/codex-wsl-clang-debug/bin/zr_vm_object_call_known_native_fast_path_test
  - build/codex-wsl-clang-debug/bin/zr_vm_container_runtime_test
  - build/codex-wsl-clang-debug/bin/zr_vm_execution_dispatch_callable_metadata_test
  - build/codex-msvc-cli-debug/bin/Debug/zr_vm_cli.exe tests/fixtures/projects/hello_world/hello_world.zrp
doc_type: milestone-detail
---

# VM Performance Instruction Optimization W1

## Scope

This document tracks the W1 runtime-body optimization checkpoints from the
second-stage VM performance plan. The current retained line keeps
`instructions/helpers/slowpaths` signatures stable for the tracked non-GC
profile cases while reducing hot-path runtime body cost.

## Current Live Continuation

The live continuation is rooted in the readonly-inline stack-operands index
path:

- `GET_BY_INDEX` / `SET_BY_INDEX` dispatch branches now count the matching
  profile helper through the dispatch loop's cached `profileRuntime` and
  `recordHelpers` state instead of calling `ZrCore_Profile_RecordHelperFromState`.
- The internal stack-operands readonly-inline get/set helpers in `object.c`
  no longer repeat the receiver object/array type gate that dispatch already
  checked before entering the helper.
- The get-side readonly-inline stack helper is split from the generic mode
  helper so the stack-only success body stays local to the hot call.
- The set-only member-access stack-root operand gate remains retained.

## Latest Accepted Evidence

Baseline:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_index_profile_runtime_local_rerun_20260424-continue`

Receiver type guard skip:

- first:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_20260424-continue`
- confirm:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`

Confirm totals versus the live baseline:

| case | baseline Ir | confirm Ir | delta |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 255,453,211 | 255,466,030 | +12,819 |
| `dispatch_loops` | 349,834,504 | 349,798,765 | -35,739 |
| `matrix_add_2d` | 12,000,740 | 12,008,069 | +7,329 |
| `map_object_access` | 21,766,383 | 21,699,329 | -67,054 |

The first and confirm runs kept the checked tracked non-GC profile signatures
identical to the live baseline for `instructions`, `helpers`, and `slowpaths`.
The direct target also improved in Callgrind annotate:

- `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperands`:
  - baseline: 401,595 Ir
  - guard-skip runs: 368,811 Ir

The non-target numeric/matrix movements are treated as bounded run-to-run
movement; the target `map_object_access` and dispatch loop movement justify
keeping the guard skip as the current live continuation.

## Rejected Adjacent Probe

The adjacent `GET_BY_INDEX` stable-result reset-local counting probe was
rejected even though total Ir moved down in the first rerun. It changed tracked
helper signatures:

- `map_object_access/value_reset_null`: 4,601 -> 12,797
- `string_build/value_reset_null`: 6,734 -> 6,897

Current W1 profile discipline keeps these signatures stable unless the plan
explicitly changes acceptance policy.

The adjacent stack-operands null-guard assertion probe was also rejected. It
changed the remaining null argument checks in the readonly-inline stack-operands
get/set helpers into debug assertions while keeping the real payload and int-key
guards. All three runs kept tracked profile signatures identical and reduced
the target helper body, but `dispatch_loops` regressed in every run:

| run | `dispatch_loops` delta | `map_object_access` delta |
| --- | ---: | ---: |
| first | +42,685 | -62,154 |
| confirm | +22,967 | -48,578 |
| tie-break | +11,620 | -38,056 |

The probe was reverted because the current W1 order treats repeated
`dispatch_loops`回吐 as a rejection signal even when the narrower map/index
target improves.

The Map readonly-inline callback receiver-type guard skip was rejected for the
same reason. Removing the duplicate object/array type check from
`zr_container_map_get_item_readonly_inline_fast(...)` and
`zr_container_map_set_item_readonly_inline_no_result_fast(...)` consistently
reduced the direct callback body, but the representative dispatch case was not
stable enough to keep:

| run | `dispatch_loops` delta | `map_object_access` delta | get callback Ir |
| --- | ---: | ---: | ---: |
| first | -3,019 | -41,393 | 822,455 |
| confirm | +71,369 | -41,942 | 822,455 |
| tie-break | +31,158 | -45,269 | 822,455 |

The live tree therefore stays at the stack-operands receiver type guard skip;
the callback-local type guard remains in place.

The dispatch-side KnownObject fast-entry probe was also rejected and reverted.
It tried to cast the receiver object in `GET_BY_INDEX` / `SET_BY_INDEX`
dispatch before calling a new `*KnownObject` stack-operands helper, so the
object helper would not repeat `receiver->value.object` extraction. The probe
kept checked tracked non-GC signatures identical, but it made the direct target
body and representative totals worse:

| case | current live Ir | probe Ir | delta |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 255,466,030 | 255,433,283 | -32,747 |
| `dispatch_loops` | 349,798,765 | 349,829,906 | +31,141 |
| `matrix_add_2d` | 12,008,069 | 12,006,235 | -1,834 |
| `map_object_access` | 21,699,329 | 21,802,724 | +103,395 |

Annotate made the rejection clear:

- `ZrCore_Object_TryGetByIndexReadonlyInlineFastStackOperandsKnownObject`:
  385,197 Ir versus the current live helper's 368,811 Ir
- `ZrCore_Object_TrySetByIndexReadonlyInlineFastStackOperandsKnownObject`:
  188,591 Ir versus the current live helper's 159,894 Ir
- `ZrCore_Execute`: 9,026,536 Ir versus current live 8,981,450 Ir

The live `tests_generated/performance/` directory was restored to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`

After reverting, WSL gcc and clang direct binaries passed:

- `zr_vm_object_call_known_native_fast_path_test`: 57 tests, 0 failures
- `zr_vm_container_runtime_test`: 39 tests, 0 failures
- `zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures

The Map readonly-inline get callback plain-value direct-copy probe was also
rejected and reverted. It tried to bypass `ZrCore_Value_CopyNoProfile(...)` in
`zr_container_map_get_item_readonly_inline_fast(...)` when the mapped value and
destination both used none ownership and the mapped value was not a GC object.
The local callback body became smaller, but the representative total did not
hold:

| run | `numeric_loops` delta | `dispatch_loops` delta | `matrix_add_2d` delta | `map_object_access` delta | get callback Ir |
| --- | ---: | ---: | ---: | ---: | ---: |
| first | -53,459 | +7,808 | -9,785 | +5,902 | 863,437 |
| confirm | -31,643 | +37,440 | -8,766 | +3,338 | 863,430 |

The current live callback body is 871,617 Ir, so the direct-copy branch really
removed local work. It still regressed `map_object_access` total in both runs
and moved `dispatch_loops` the wrong way, while checked tracked non-GC
`instructions/helpers/slowpaths` signatures stayed identical. The probe is
therefore classified as another pure runtime-body/code-layout regression, and
the live `tests_generated/performance/` directory was restored again to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`

The object-local `value_reset_null` profile-counting probe was rejected after
one rerun and reverted. It replaced `object.c` calls to
`ZrCore_Value_ResetAsNull(...)` with an object-local
`object_record_helper(..., ZR_PROFILE_HELPER_VALUE_RESET_NULL)` plus
`ZrCore_Value_ResetAsNullNoProfile(...)`, matching the dispatch-local counting
style without changing helper signatures. The checked tracked non-GC
`instructions/helpers/slowpaths` signatures stayed identical, but the target
did not move:

| case | current live Ir | probe Ir | delta |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 255,466,030 | 255,416,386 | -49,644 |
| `dispatch_loops` | 349,798,765 | 349,816,294 | +17,529 |
| `matrix_add_2d` | 12,008,069 | 11,991,207 | -16,862 |
| `map_object_access` | 21,699,329 | 21,697,762 | -1,567 |

`ZrCore_Value_ResetAsNull` remained the `dispatch_loops` top helper at 139,740
Ir, while `object_set_value_core` moved from 843,728 to 844,463 Ir. The probe
therefore missed the actual reset hotspot and was reverted without a confirm
rerun.

The `GET_STACK` / `GET_CONSTANT` profile-mode offset-copy probe was rejected
and reverted after one rerun. It kept helper counting inline in the opcode body
and wrote stack/ret destinations directly instead of routing the profile path
through `execution_copy_value_fast(...)`. The checked tracked non-GC
`instructions/helpers/slowpaths` signatures stayed identical, but the runtime
body moved the wrong way on the representative cases:

| case | current live Ir | probe Ir | delta |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 255,466,030 | 255,409,667 | -56,363 |
| `dispatch_loops` | 349,798,765 | 349,819,223 | +20,458 |
| `matrix_add_2d` | 12,008,069 | 11,999,457 | -8,612 |
| `map_object_access` | 21,699,329 | 21,708,824 | +9,495 |

`ZrCore_Execute` stayed flat at 332,568,516 Ir in `dispatch_loops`, while
`object_set_value_core` moved from 843,728 to 846,456 Ir. The probe therefore
looked like layout churn rather than a real value-copy cut, so it was not taken
to a confirm rerun.

The dispatch member exact-pair checked-object entry was accepted after a
first/confirm pair. It adds dispatch-only `GET_MEMBER_SLOT` / `SET_MEMBER_SLOT`
helpers for the opcode arm where receiver type has already been checked as
object/array, so the hottest exact receiver-pair member hit skips the duplicate
object/array guard inside the generic dispatch helper. String receivers still
fall through to the existing cached member path.

Archived evidence:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_20260424-continue`
- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`

Relative to current live
`performance_profile_tracked_non_gc_after_index_stack_receiver_type_guard_skip_confirm_20260424-continue`:

| run | `numeric_loops` delta | `dispatch_loops` delta | `matrix_add_2d` delta | `map_object_access` delta |
| --- | ---: | ---: | ---: | ---: |
| first | -16,431 | -2,703,364 | +44,500 | +44,044 |
| confirm | -15,691 | -2,706,567 | +18,712 | +46,640 |

The four checked tracked non-GC cases kept identical
`instructions/helpers/slowpaths` signatures versus current live. The small
`matrix_add_2d` / `map_object_access` layout cost is accepted because the
target dispatch workload repeatedly drops about 2.7M Ir without semantic
signature movement.

Validation after adding direct coverage for the new checked-object helpers:

- WSL gcc:
  - `zr_vm_execution_member_access_fast_paths_test`: 102 tests, 0 failures
  - `zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures
- WSL clang:
  - `zr_vm_execution_member_access_fast_paths_test`: 102 tests, 0 failures
  - `zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures

The adjacent `GET_MEMBER_SLOT` non-string checked-object gate probe was
rejected and reverted. It tried to replace the accepted `(object || array)`
dispatch gate with `opA->type != ZR_VALUE_TYPE_STRING`, relying on the outer
opcode receiver check to have already rejected every other type. The semantic
profile signatures stayed identical, but the representative dispatch workload
regressed sharply versus the accepted checked-object confirm baseline:

| case | accepted checked-object confirm Ir | probe Ir | delta |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 255,450,339 | 255,438,995 | -11,344 |
| `dispatch_loops` | 347,092,198 | 348,660,382 | +1,568,184 |
| `matrix_add_2d` | 12,026,781 | 12,018,735 | -8,046 |
| `map_object_access` | 21,745,969 | 21,742,780 | -3,189 |

Because `dispatch_loops` moved the wrong way by about 1.57M Ir with unchanged
`instructions/helpers/slowpaths`, this is classified as a branch-shape/layout
regression. The negative snapshot is archived at:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_get_member_slot_non_string_checked_gate_20260424-continue`

The live `tests_generated/performance/` directory was restored to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`

The late right-const arithmetic fold pass-order probe was rejected and
reverted. It added a second `fold_right_const_arithmetic_late` pass followed by
`compact_nops_12` immediately after `array_int_index_accesses_late` in child
function quickening. Fresh `.zri` inspection only showed a local child-function
change in `map_object_access`'s `labelFor`; the tracked top-level profile
instruction mix did not change.

Focused validation before the profile run:

- WSL gcc parser/compiler/container/dispatch command returned exit code 0.
- WSL clang focused subset passed:
  - `zr_vm_compiler_call_lowering_focus_test`: 7 tests, 0 failures
  - `zr_vm_container_runtime_test`: 39 tests, 0 failures
  - `zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures

The full WSL clang parser suite was started first, but timed out after 364s
because of suite breadth/output rather than a reported test failure.

Relative to current live
`performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_final_20260424-continue`,
the first checked tracked non-GC profile regressed all four representative
cases while `GET_CONSTANT`, `MOD_SIGNED`, `MOD_SIGNED_CONST`, helpers, and
slowpaths stayed unchanged:

| case | current live Ir | probe Ir | delta |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 255,449,011 | 255,481,009 | +31,998 |
| `dispatch_loops` | 347,092,703 | 347,110,763 | +18,060 |
| `matrix_add_2d` | 12,024,518 | 12,056,413 | +31,895 |
| `map_object_access` | 21,740,047 | 21,767,732 | +27,685 |

No confirm run was taken because every tracked case regressed and the profile
signatures showed no opcode-mix benefit. Snapshots:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_late_right_const_arithmetic_fold_20260424-continue`
- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_late_right_const_arithmetic_fold_20260424-continue`

The live `tests_generated/performance/` directory was restored to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_final_20260424-continue`

The adjacent hot Map lookup last-key direct-hit probe was rejected and
reverted. It tried to avoid constructing a temporary
`ZrContainerHotMapLookupCacheSlot` for the `cache->lastKeyObject` hit path in
`zr_container_try_hot_map_lookup_cache(...)`, writing `lastIndex` and
`lastEntryObject` directly instead. Focused gcc/clang object/container/dispatch
tests passed, and the checked tracked non-GC profile signatures stayed
identical, but both tracked runs regressed the current live restore point:

| run | `numeric_loops` delta | `dispatch_loops` delta | `matrix_add_2d` delta | `map_object_access` delta |
| --- | ---: | ---: | ---: | ---: |
| first | -1,570 | +12,942 | -3,390 | +12,216 |
| confirm | +6,225 | +3,544 | +4,789 | +5,516 |

The target Map workload did not confirm an improvement, so this remains a
code-layout/branch-shape regression rather than a useful hot-cache cleanup.
The negative snapshots are archived at:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_hot_map_last_hit_direct_20260424-continue`
- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_hot_map_last_hit_direct_confirm_20260424-continue`
- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_hot_map_last_hit_direct_20260424-continue`

The live `tests_generated/performance/` directory was restored to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_final_20260424-continue`

The `GET_BY_INDEX` fast result dispatch-copy probe is kept as a bounded live
continuation. It changes only the opcode tail writeback after a resolved
`stableResult`: the final `stableResult -> destination` copy now uses
`execution_copy_value_fast(state, destination, &stableResult, profileRuntime,
recordHelpers)` instead of `ZrCore_Value_Copy(...)`. This keeps the existing
`value_copy` helper signature stable while avoiding another TLS-based helper
record at that opcode tail. The touched file is already large, but this probe
only replaces an existing statement inside one opcode arm and does not add a
new helper or responsibility.

Archived evidence:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_20260424-continue`
- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_confirm_20260424-continue`
- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_final_20260424-continue`

Relative to accepted checked-object confirm
`performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`:

| run | `numeric_loops` delta | `dispatch_loops` delta | `matrix_add_2d` delta | `map_object_access` delta |
| --- | ---: | ---: | ---: | ---: |
| first | +6,788 | -5,106 | -4,671 | -8,231 |
| confirm | -13,892 | -16,130 | -17,473 | -113 |
| tie-break | -2,916 | -7,278 | +8,340 | +18,715 |
| final | -1,328 | +505 | -2,263 | -5,922 |

All four checked tracked non-GC runs kept identical
`instructions/helpers/slowpaths` signatures versus the accepted checked-object
confirm baseline. Because the totals are small and one tie-break run moved
`matrix_add_2d` / `map_object_access` upward, this is recorded as a bounded
live continuation rather than a new headline checkpoint. Future live restores
should use the final snapshot above unless a later probe is accepted.

Focused validation before the performance runs:

- WSL gcc:
  - `zr_vm_object_call_known_native_fast_path_test`: 57 tests, 0 failures
  - `zr_vm_container_runtime_test`: 39 tests, 0 failures
  - `zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures
- WSL clang:
  - `zr_vm_object_call_known_native_fast_path_test`: 57 tests, 0 failures
  - `zr_vm_container_runtime_test`: 39 tests, 0 failures
  - `zr_vm_execution_dispatch_callable_metadata_test`: 17 tests, 0 failures

The adjacent `SET_BY_INDEX` fast-hit destination reload skip was rejected and
reverted. It removed the `destination = E(...) ...` reload after a successful
readonly-inline `SET_BY_INDEX` fast hit while leaving `UPDATE_BASE(callInfo)`
in place. Focused gcc/clang object/container/dispatch tests passed, and the
checked tracked non-GC profile signatures stayed identical, but the runtime
totals did not support keeping it:

| run | `numeric_loops` delta | `dispatch_loops` delta | `matrix_add_2d` delta | `map_object_access` delta |
| --- | ---: | ---: | ---: | ---: |
| first | +19,815 | -22,895 | +5,217 | -15,121 |
| confirm | +49,713 | +2,346 | +10,804 | -34,801 |

The map workload improved in both runs, but the hottest dispatch case did not
confirm and the numeric/matrix regressions were too large for the current W1
acceptance rule. The negative snapshot is archived at:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_set_by_index_fast_hit_destination_reload_skip_20260424-continue`

The live `tests_generated/performance/` directory was restored to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_get_by_index_fast_result_dispatch_copy_final_20260424-continue`

The dispatch exact-pair set `AfterFastMiss` fallback probe was also rejected
and reverted. It tried to call
`ZrCore_Object_SetExistingPairValueAfterFastMissUnchecked(...)` after the
dispatch helper's own plain-value fast set attempts had already missed, instead
of calling `ZrCore_Object_SetExistingPairValueUnchecked(...)` and repeating a
similar fast-miss check inside that wrapper. A checked-object slow-lane test was
added during the probe and passed in both gcc and clang, but the runtime totals
did not support keeping the change:

| case | accepted checked-object confirm Ir | probe Ir | delta |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 255,450,339 | 255,452,070 | +1,731 |
| `dispatch_loops` | 347,092,198 | 347,098,416 | +6,218 |
| `matrix_add_2d` | 12,026,781 | 12,035,300 | +8,519 |
| `map_object_access` | 21,745,969 | 21,756,786 | +10,817 |

The checked tracked non-GC `instructions/helpers/slowpaths` signatures stayed
identical, so this is another pure runtime-body/code-layout regression. The
negative snapshot is archived at:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_dispatch_exact_pair_set_after_fast_miss_20260424-continue`

After revert, the live `tests_generated/performance/` directory was restored
again to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`

The adjacent `KNOWN_VM_MEMBER_CALL` cached receiver object direct-use probe was
rejected and reverted. It tried to use `slot->cachedReceiverObject` directly in
`execution_try_resolve_known_vm_member_exact_single_slot_fast(...)` after the
receiver raw pointer had already been proven equal to the cached receiver,
skipping `ZR_CAST_OBJECT(...)` and the following null check. Focused gcc/clang
dispatch/member tests passed, but the representative totals moved the wrong
way:

| case | accepted checked-object confirm Ir | probe Ir | delta |
| --- | ---: | ---: | ---: |
| `numeric_loops` | 255,450,339 | 255,487,713 | +37,374 |
| `dispatch_loops` | 347,092,198 | 347,102,397 | +10,199 |
| `matrix_add_2d` | 12,026,781 | 12,029,056 | +2,275 |
| `map_object_access` | 21,745,969 | 21,744,804 | -1,165 |

The checked tracked non-GC `instructions/helpers/slowpaths` signatures stayed
identical, so the small source simplification is classified as a layout/branch
regression for the current W1 benchmark order. The negative snapshot is
archived at:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_rejected_known_vm_member_cached_receiver_object_direct_20260424-continue`

The live `tests_generated/performance/` directory was restored to:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_member_exact_pair_checked_object_confirm_20260424-continue`
