# 2026-04-20 W1-T7/W1-T8 Tracked Runtime Follow-Up

## Scope

This note closes the current W1-T7/W1-T8 rerun loop after the accepted
`object_set_cached_string_pair_long_only_gate` slice.

The goal of this follow-up was not another W2 opcode/quickening pass. It was to:

- rerun fresh tracked non-GC evidence on the current runtime branch
- try a few bounded runtime-body cuts under the already-specialized opcode mix
- keep only changes that improve tracked runtime cost without introducing unexplained fallback or helper drift

## Accepted Baseline

The accepted production baseline for the current object-member runtime line remains:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_object_set_cached_string_pair_long_only_gate_20260420-002421`

That state already includes the accepted long-string cached-pair fast reuse in:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`

## Fresh Evidence

Focused WSL validation rerun on both toolchains passed:

- gcc:
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_instructions_test`
  - `zr_vm_value_copy_fast_paths_test`
- clang:
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_instructions_test`
  - `zr_vm_value_copy_fast_paths_test`

New regression coverage kept in tree:

- `test_object_set_value_non_hidden_same_length_string_does_not_refresh_hidden_items_cache`
- `test_object_set_value_distinct_long_string_same_length_does_not_reuse_cached_pair`

These tests live in:

- `tests/core/test_execution_member_access_fast_paths.c`

They lock two important semantics:

1. non-hidden equal-length names must not disturb hidden-items cache state
2. distinct same-length long-string keys must not be mistaken for the cached pair key

## Accepted Follow-Up Slice

### 5. Dispatch frame/base bookkeeping `stack_get_value` de-profile

Snapshots:

- pre: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_pre_dispatch_stack_get_bookkeeping_20260420-052842`
- after: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_stack_get_bookkeeping_20260420-052940`

Decision:

- accepted

Production change:

- `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c`
- `tests/core/test_execution_dispatch_callable_metadata.c`

Reason:

- the cut is tightly bounded: only the frame/base refresh bookkeeping reads at `LZrReturning` and the shared fetch loop now use `ZrCore_Stack_GetValueNoProfile(...)`
- tracked representative callgrind totals improved without opcode/helper mix drift:
  - `numeric_loops`: `257,061,114 -> 257,048,442` (`-12,672`)
  - `dispatch_loops`: `355,862,346 -> 354,180,617` (`-1,681,729`)
  - `matrix_add_2d`: `12,945,515 -> 12,948,326` (`+2,811`)
  - `map_object_access`: `23,180,936 -> 23,165,205` (`-15,731`)
- the intended helper collapse is explicit and isolated:
  - `dispatch_loops` `stack_get_value`: `615,436 -> 52`
  - `map_object_access` `stack_get_value`: `16,424 -> 32`
  - `numeric_loops` `stack_get_value`: `48 -> 40`
  - `matrix_add_2d` `stack_get_value`: `57 -> 49`
- `value_copy`, `precall`, `get_member`, `GET_MEMBER_SLOT`, and `KNOWN_VM_CALL` counts stayed unchanged in the representative cases, so this is a real bookkeeping cut instead of another code-layout-only trade

Validation:

- WSL gcc targeted tests pass:
  - `zr_vm_execution_dispatch_callable_metadata_test`
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_instructions_test`
- WSL gcc tracked profile rerun passes:
  - `performance_report`
- WSL clang still rebuilds but the targeted test binary exits immediately before test output, matching the already-known baseline startup blocker on the clang line rather than a new dispatch-specific regression

### 6. Cached descriptor/callable own-name lookup wrapper cut

Snapshots:

- pre: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_stack_get_bookkeeping_20260420-052940`
- after: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_name_cached_lane_20260420-055700`

Decision:

- accepted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object.c`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- the cut is still strictly on the W1 runtime-body line:
  - `object_get_own_string_pair_by_name_cached_unchecked(...)` now preserves the old cached lookup pair on miss instead of clearing it
  - that same wrapper now reuses equal long-string cached pairs before falling back to `ZrCore_HashSet_Find(...)`
  - `ZrCore_Object_TryGetMemberWithKeyFastUnchecked(...)`
  - `ZrCore_Object_GetMemberCachedCallableTargetUnchecked(...)`
  - `ZrCore_Object_GetMemberCachedCallableUnchecked(...)`
  - `ZrCore_Object_GetMemberCachedDescriptorUnchecked(...)`
    now route the own string-name lookup through the wrapper once instead of repeating:
    pointer-cache probe -> key materialization -> `object_get_own_value_unchecked(...)`
- representative callgrind totals improved across the full non-GC hotspot set:
  - `numeric_loops`: `257,048,442 -> 257,034,527` (`-13,915`)
  - `dispatch_loops`: `354,180,617 -> 354,146,057` (`-34,560`)
  - `matrix_add_2d`: `12,948,326 -> 12,919,863` (`-28,463`)
  - `map_object_access`: `23,165,205 -> 23,119,181` (`-46,024`)
- all tracked non-GC profile counts stayed identical, so this is another shared runtime-body cut rather than opcode drift:
  - representative helper counts are unchanged in `numeric_loops`, `dispatch_loops`, `matrix_add_2d`, and `map_object_access`
  - all `instructions/helpers/slowpaths` arrays are bit-for-bit identical across:
    - `numeric_loops`
    - `dispatch_loops`
    - `container_pipeline`
    - `matrix_add_2d`
    - `map_object_access`
    - `string_build`
    - `call_chain_polymorphic`
    - `mixed_service_loop`
  - bounded deferred W2 residue stays bounded:
    - `call_chain_polymorphic` `FUNCTION_CALL`: `322 -> 322`
    - `call_chain_polymorphic` `KNOWN_NATIVE_CALL`: `1 -> 1`
    - `call_chain_polymorphic` `KNOWN_VM_CALL`: `960 -> 960`
- profile-tier wall time drift was broad and obviously noisy across the whole suite, so it was not used as the acceptance signal for this slice; the callgrind and profile-count evidence are aligned and stable

New regression coverage kept in tree:

- `test_object_get_own_string_pair_by_name_cached_preserves_existing_cache_on_miss`
- `test_object_get_own_string_pair_by_name_cached_reuses_equal_long_string_cached_pair`

Validation:

- WSL gcc targeted tests pass:
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_execution_dispatch_callable_metadata_test`
  - `zr_vm_instructions_test`
- WSL gcc benchmark release rerun passes:
  - `performance_report` with `ZR_VM_TEST_TIER=profile`
  - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
- Windows MSVC CLI smoke passes:
  - `build/codex-msvc-cli-debug/bin/Debug/zr_vm_cli.exe`
  - `tests/fixtures/projects/hello_world/hello_world.zrp`
- WSL clang still rebuilds the targeted runtime tests, but runtime startup remains blocked before first test execution:
  - `gdb` shows the current crash in `ZrCore_StringTable_Init -> ZrCore_String_Create -> string_create_short -> ZrCore_HashSet_GetBucket`
  - this happens before the member-access test bodies run, so the clang line is still not usable as acceptance evidence for this W1 slice

### 7. Unchecked cached string-pair raw-cast cleanup

Snapshots:

- pre: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_pre_cached_string_unchecked_raw_cast_20260420-063547`
- after: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_string_unchecked_raw_cast_20260420-063952`

Decision:

- accepted

Production change:

- `zr_vm_core/src/zr_vm_core/object/object_internal.h`
- `tests/core/test_execution_member_access_fast_paths.c`

Reason:

- this is a very narrow runtime-body cleanup in the already-accepted cached string lookup lane:
  - `object_try_get_cached_string_pair_by_name_unchecked(...)`
  - `object_try_get_cached_string_pair_unchecked(...)`
  now raw-cast cached string objects after their surrounding guards have already proved the values are string keys, instead of paying an extra checked cast in the unchecked hot path
- representative callgrind totals remain stable-to-better with no profile drift:
  - `numeric_loops`: `257,034,527 -> 257,035,951` (`+1,424`)
  - `dispatch_loops`: `354,146,057 -> 354,141,124` (`-4,933`)
  - `matrix_add_2d`: `12,919,863 -> 12,914,825` (`-5,038`)
  - `map_object_access`: `23,119,181 -> 23,118,639` (`-542`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed identical across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- the improvement is small, but unlike the rejected broader runtime-body experiments it does not introduce a representative regression; the only upward movement is the tiny `numeric_loops` `+1,424 Ir`, which stays well within the surrounding noise floor while the other tracked representatives all improve

New regression coverage kept in tree:

- `test_object_get_value_reuses_equal_long_string_cached_pair`

Validation:

- WSL gcc targeted tests pass:
  - `zr_vm_execution_member_access_fast_paths_test`
  - `zr_vm_execution_dispatch_callable_metadata_test`
  - `zr_vm_instructions_test`
- WSL gcc benchmark release rerun passes:
  - `performance_report` with `ZR_VM_TEST_TIER=profile`
  - `ZR_VM_PERF_CALLGRIND_COUNTING=1`
- WSL clang targeted rebuild still ends in the existing startup failure before first test output:
  - `zr_vm_execution_member_access_fast_paths_test`
  - no new touched-path signal was observed before the existing crash
- Windows MSVC CLI smoke passes:
  - `build/codex-msvc-cli-debug/bin/Debug/zr_vm_cli.exe`
  - `tests/fixtures/projects/hello_world/hello_world.zrp`

## Rejected Experiments

### 1. Hidden-items literal shortcut

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_hidden_items_literal_20260420-005247`

Decision:

- rejected

Reason:

- `dispatch_loops`: `356,019,085 -> 356,031,490` (`+12,405`)
- `numeric_loops`: `257,116,931 -> 257,122,967` (`+6,036`)
- only `map_object_access` improved a little: `23,207,162 -> 23,202,115` (`-5,047`)

This was not a good exchange for the hotter tracked line.

### 2. Full `value_to_*` header-inline attempt

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_inline_value_conversion_20260420-010217`

Decision:

- rejected

Reason:

- `dispatch_loops`: `356,019,085 -> 355,994,181` (`-24,904`)
- but `numeric_loops`: `257,116,931 -> 257,158,793` (`+41,862`)
- and `map_object_access`: `23,207,162 -> 23,226,456` (`+19,294`)
- helper/opcode/slowpath counts stayed identical, so this was just runtime-body/codegen churn, not a real tracked-line improvement

### 3. Local long-string raw-compare inside cached pair lookup

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_object_cached_long_string_raw_compare_20260420-011314`

Decision:

- rejected

Reason:

- `numeric_loops` improved a little: `257,116,931 -> 257,094,767` (`-22,164`)
- but `dispatch_loops`: `356,019,085 -> 356,029,087` (`+10,002`)
- `map_object_access`: `23,207,162 -> 23,223,854` (`+16,692`)
- `matrix_add_2d`: `12,993,300 -> 13,003,931` (`+10,631`)

Again, helper/opcode/slowpath totals stayed identical. The evidence is too flat-to-worse to accept.

### 4. Single-slot exact-receiver-object get hot helper

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_exact_receiver_object_get_hot_fast_rerun_20260420-015858`

Decision:

- rejected

Reason:

- first fresh rerun on the live `tests_generated/performance` tree looked promising on the target case but not on the tracked set:
  - `numeric_loops`: `257,116,931 -> 257,108,506` (`-8,425`)
  - `dispatch_loops`: `356,019,085 -> 355,995,126` (`-23,959`)
  - `matrix_add_2d`: `12,993,300 -> 12,995,922` (`+2,622`)
  - `map_object_access`: `23,207,162 -> 23,233,503` (`+26,341`)
- archived rerun kept the same `dispatch_loops` win but stayed net-flat to slightly worse outside the target case:
  - `numeric_loops`: `257,116,931 -> 257,113,736` (`-3,195`)
  - `dispatch_loops`: `356,019,085 -> 355,994,740` (`-24,345`)
  - `matrix_add_2d`: `12,993,300 -> 13,012,501` (`+19,201`)
  - `map_object_access`: `23,207,162 -> 23,215,980` (`+8,818`)
- the current helper/opcode profile remained identical, so this cut only shifted runtime-body/code-layout cost
- the target gain is real, but it is too small and too unstable across the rest of tracked non-GC cases to lock in as an acceptance-quality W1 cut

### 5. `ZrCore_Object_SetValue` cached-pair pre-normalization helper

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_setvalue_cached_pair_lane_20260420-060929`

Decision:

- rejected

Reason:

- representative callgrind totals regressed across the full tracked hotspot set:
  - `numeric_loops`: `257,034,527 -> 257,127,970` (`+93,443`)
  - `dispatch_loops`: `354,146,057 -> 354,253,963` (`+107,906`)
  - `matrix_add_2d`: `12,919,863 -> 12,958,879` (`+39,016`)
  - `map_object_access`: `23,119,181 -> 23,162,794` (`+43,613`)
- the target function got hotter in the two most relevant representatives:
  - `dispatch_loops` `ZrCore_Object_SetValue`: `773,432 -> 868,180`
  - `map_object_access` `ZrCore_Object_SetValue`: `435,146 -> 487,630`
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed identical, so this was not specialization drift; it was a pure runtime-body regression
- the experiment was removed from production code after verification

### 6. Global plain-object `value_copy` fast lane

Snapshots:

- pre: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_pre_value_copy_plain_object_fast_rerun_20260420-062043`
- after: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_value_copy_plain_object_fast_rerun_20260420-062755`

Decision:

- rejected

Reason:

- the broader `value.h` fast-lane expansion did reduce some local object-body costs, but the full tracked totals regressed in the hottest representatives:
  - `numeric_loops`: `257,034,527 -> 257,493,079` (`+458,552`)
  - `dispatch_loops`: `354,146,057 -> 355,166,077` (`+1,020,020`)
  - `matrix_add_2d`: `12,919,863 -> 12,897,337` (`-22,526`)
  - `map_object_access`: `23,119,181 -> 23,133,527` (`+14,346`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed identical, so this was another pure runtime-body/codegen regression rather than an opcode-mix change
- the experiment was fully removed from production code after verification; the kept value-copy baseline remains the previously accepted state

### 7. `SetValue` string-key raw-cast cleanup

Snapshots:

- pre: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_pre_object_string_key_raw_cast_20260420-064543`
- after: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_object_string_key_raw_cast_20260420-064806`

Decision:

- rejected

Reason:

- the candidate did hit the target case:
  - `dispatch_loops`: `354,141,124 -> 354,103,057` (`-38,067`)
- but the broader representative line stayed mixed in the same way as earlier rejected code-layout trades:
  - `numeric_loops`: `257,035,951 -> 257,038,753` (`+2,802`)
  - `matrix_add_2d`: `12,914,825 -> 12,925,370` (`+10,545`)
  - `map_object_access`: `23,118,639 -> 23,118,870` (`+231`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed identical, so this was again runtime-body/code-layout movement rather than specialization drift
- the function-level shape also stayed mixed instead of clearly tightening the shared hot line:
  - `dispatch_loops` improved:
    - `ZrCore_Object_SetValue`: `772,442 -> 768,979`
    - `object_get_own_value.part.0`: `166,657 -> 163,314`
    - `ZrCore_String_Equal`: `530,717 -> 524,752`
  - but `matrix_add_2d` and `map_object_access` pushed those same hot functions back up:
    - `matrix_add_2d` `ZrCore_Object_SetValue`: `434,047 -> 435,541`
    - `matrix_add_2d` `object_get_own_value.part.0`: `91,271 -> 93,390`
    - `matrix_add_2d` `ZrCore_String_Equal`: `173,433 -> 176,829`
    - `map_object_access` `ZrCore_Object_SetValue`: `433,843 -> 436,348`
    - `map_object_access` `object_get_own_value.part.0`: `88,224 -> 90,863`
    - `map_object_access` `ZrCore_String_Equal`: `113,364 -> 118,172`
- the experiment was removed from production code after verification

### 8. Cached member receiver raw-cast cleanup

Snapshots:

- pre: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_pre_execution_member_receiver_raw_cast_20260420-065059`
- after: `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_execution_member_receiver_raw_cast_20260420-065408`

Decision:

- rejected

Reason:

- this one was targeted directly at `execution_member_get_cached` / `execution_member_set_cached`, but the tracked representatives still split in both directions:
  - `numeric_loops`: `257,035,951 -> 257,033,011` (`-2,940`)
  - `dispatch_loops`: `354,141,124 -> 354,162,997` (`+21,873`)
  - `matrix_add_2d`: `12,914,825 -> 12,926,184` (`+11,359`)
  - `map_object_access`: `23,118,639 -> 23,105,184` (`-13,455`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed identical, so this was again a code-layout/runtime-body trade rather than a specialization win
- because the user-directed priority case `dispatch_loops` regressed and the overall shape stayed mixed, the experiment was removed from production code after verification

### 9. Exact member-name pointer gate in cached own-hit lanes

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-080602`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_exact_name_pointer_cached_lane_20260420-080759`

Decision:

- rejected

Reason:

- the narrower exact-pointer gate was still a full tracked regression:
  - `numeric_loops`: `257,035,951 -> 257,094,399` (`+58,448`)
  - `dispatch_loops`: `354,141,124 -> 354,319,264` (`+178,140`)
  - `matrix_add_2d`: `12,914,825 -> 12,995,976` (`+81,151`)
  - `map_object_access`: `23,118,639 -> 23,179,042` (`+60,403`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed bit-for-bit identical across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- the main movement was another shared runtime-body/code-layout loss in string-name comparison work rather than a real hot-lane collapse:
  - `numeric_loops` `ZrCore_String_Equal`: `122,692 -> 127,460`
  - `dispatch_loops` `ZrCore_String_Equal`: `530,717 -> 556,332`
  - `matrix_add_2d` `ZrCore_String_Equal`: `173,433 -> 182,436`
  - `map_object_access` `ZrCore_String_Equal`: `113,364 -> 121,930`
- the intended own-hit bodies did not cool enough to justify the exchange:
  - `dispatch_loops` `ZrCore_Object_SetValue`: `772,442 -> 772,877`
  - `dispatch_loops` `object_get_own_value.part.0`: `166,657 -> 166,693`
  - `map_object_access` `ZrCore_Object_SetValue`: `433,843 -> 436,101`
  - `map_object_access` `object_get_own_value.part.0`: `88,224 -> 90,055`
- the experiment was fully removed from production code after verification; the active W1 baseline remains the previously accepted wrapper-only cached lane plus the unchecked raw-cast cleanup

### 10. String-equality hash-gate rerun

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_string_equal_hash_gate_rerun_20260420-071638`

Decision:

- rejected

Reason:

- the representative callgrind line stayed net-regressive even before the later narrower follow-ups:
  - `numeric_loops`: `257,035,951 -> 257,061,001` (`+25,050`)
  - `dispatch_loops`: `354,141,124 -> 354,150,350` (`+9,226`)
  - `matrix_add_2d`: `12,914,825 -> 12,933,960` (`+19,135`)
  - `map_object_access`: `23,118,639 -> 23,140,162` (`+21,523`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed bit-for-bit identical across all eight tracked cases, so this was another pure runtime-body/code-layout loss
- `ZrCore_String_Equal` got hotter in all four representative cases:
  - `numeric_loops`: `122,692 -> 126,777`
  - `dispatch_loops`: `530,717 -> 531,939`
  - `matrix_add_2d`: `173,433 -> 178,267`
  - `map_object_access`: `113,364 -> 118,662`

### 11. Cached member object-pair refresh

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_cached_member_object_pair_refresh_20260420-073159`

Decision:

- rejected

Reason:

- the representative callgrind line regressed broadly:
  - `numeric_loops`: `257,035,951 -> 257,056,609` (`+20,658`)
  - `dispatch_loops`: `354,141,124 -> 354,285,735` (`+144,611`)
  - `matrix_add_2d`: `12,914,825 -> 12,987,794` (`+72,969`)
  - `map_object_access`: `23,118,639 -> 23,146,958` (`+28,319`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed identical across all eight tracked cases
- the heaviest visible shared loss was again string-name comparison work rather than a real cached-lane collapse:
  - `dispatch_loops` `ZrCore_String_Equal`: `530,717 -> 557,616`
  - `matrix_add_2d` `ZrCore_String_Equal`: `173,433 -> 185,692`
  - `map_object_access` `ZrCore_String_Equal`: `113,364 -> 122,134`

### 12. Object profiled-copy plain-fast variant

Snapshot:

- `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_object_profiled_copy_plain_fast_20260420-075144`

Decision:

- rejected

Reason:

- the representative callgrind line regressed in every tracked hotspot case:
  - `numeric_loops`: `257,035,951 -> 257,099,878` (`+63,927`)
  - `dispatch_loops`: `354,141,124 -> 354,315,595` (`+174,471`)
  - `matrix_add_2d`: `12,914,825 -> 12,988,989` (`+74,164`)
  - `map_object_access`: `23,118,639 -> 23,193,174` (`+74,535`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays stayed identical across all eight tracked cases
- the shared hot line still moved the wrong way:
  - `dispatch_loops` `ZrCore_String_Equal`: `530,717 -> 557,972`
  - `dispatch_loops` `ZrCore_Object_SetValue`: `772,442 -> 775,732`

### 13. Anchored result-buffer de-profile

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-083329`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_anchored_result_deprofile_20260420-083431`

Decision:

- rejected

Reason:

- the representative callgrind line regressed sharply:
  - `numeric_loops`: `257,035,951 -> 257,088,431` (`+52,480`)
  - `dispatch_loops`: `354,141,124 -> 354,316,957` (`+175,833`)
  - `matrix_add_2d`: `12,914,825 -> 13,001,018` (`+86,193`)
  - `map_object_access`: `23,118,639 -> 23,191,103` (`+72,464`)
- unlike the earlier pure code-layout rejections, the tracked helper arrays did move, but only by a tiny bounded amount:
  - `numeric_loops` `value_copy`: `1,799,657 -> 1,799,654`
  - `numeric_loops` `value_reset_null`: `7,990 -> 7,987`
  - `dispatch_loops` `value_copy`: `2,889,069 -> 2,889,066`
  - `dispatch_loops` `value_reset_null`: `8,220 -> 8,217`
  - the same 1-5 count reductions held across the other tracked cases too
- that helper reduction was far too small to justify the shared runtime loss:
  - `dispatch_loops` `ZrCore_Object_SetValue`: `772,442 -> 775,713`
  - `dispatch_loops` `ZrCore_String_Equal`: `530,717 -> 557,384`
  - `dispatch_loops` `object_get_own_value.part.0`: `166,657 -> 167,590`
  - `map_object_access` `ZrCore_String_Equal`: `113,364 -> 120,052`
- the experiment was fully removed from production code after verification

### 14. Existing-pair setter reuse in cached setters

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-090152`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_existing_pair_setter_reuse_20260420-090613`
  - `build/benchmark-gcc-release/tests_generated/performance_suite_after_existing_pair_setter_reuse_20260420-090613`

Decision:

- rejected

Reason:

- the representative callgrind line regressed again across the tracked runtime set:
  - `numeric_loops`: `257,035,951 -> 257,076,210` (`+40,259`)
  - `dispatch_loops`: `354,141,124 -> 354,278,780` (`+137,656`)
  - `matrix_add_2d`: `12,914,825 -> 12,987,471` (`+72,646`)
  - `map_object_access`: `23,118,639 -> 23,152,155` (`+33,516`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays again stayed bit-for-bit identical across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- the visible function movement says the candidate was another bad shared runtime-body trade centered on extra string comparison work:
  - `dispatch_loops`:
    - `ZrCore_Object_SetValue`: `772,442 -> 772,376`
    - `ZrCore_String_Equal`: `530,717 -> 551,932`
    - `object_get_own_value.part.0`: `166,657 -> 165,011`
  - `numeric_loops`:
    - `ZrCore_Object_SetValue`: `760,406 -> 761,714`
    - `ZrCore_String_Equal`: `122,692 -> 130,456`
    - `object_get_own_value.part.0`: `162,686 -> 162,726`
  - `matrix_add_2d`:
    - `ZrCore_Object_SetValue`: `434,047 -> 435,710`
    - `ZrCore_String_Equal`: `173,433 -> 186,780`
    - `object_get_own_value.part.0`: `91,271 -> 93,386`
  - `map_object_access`:
    - `ZrCore_Object_SetValue`: `433,843 -> 434,792`
    - `ZrCore_String_Equal`: `113,364 -> 118,796`
    - `object_get_own_value.part.0`: `88,224 -> 89,320`
- the experiment was fully removed from production code after verification; the active baseline remains `...after_cached_string_unchecked_raw_cast_20260420-063952`

### 15. Cached descriptor helper extraction in member runtime

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-093247`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_cached_descriptor_helpers_20260420-093433`
  - `build/benchmark-gcc-release/tests_generated/performance_suite_after_member_cached_descriptor_helpers_20260420-093433`

Decision:

- rejected

Reason:

- the representative callgrind line regressed across the tracked runtime set:
  - `numeric_loops`: `257,035,951 -> 257,093,118` (`+57,167`)
  - `dispatch_loops`: `354,141,124 -> 354,287,672` (`+146,548`)
  - `matrix_add_2d`: `12,914,825 -> 12,970,097` (`+55,272`)
  - `map_object_access`: `23,118,639 -> 23,134,529` (`+15,890`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays again stayed bit-for-bit identical across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- the intended body shrink only turned into another shared code-layout loss centered on hotter string comparison work:
  - `numeric_loops`:
    - `ZrCore_Object_SetValue`: `760,406 -> 762,859`
    - `ZrCore_String_Equal`: `122,692 -> 132,872`
    - `object_get_own_value.part.0`: `162,686 -> 162,993`
  - `dispatch_loops`:
    - `ZrCore_Object_SetValue`: `772,442 -> 774,476`
    - `ZrCore_String_Equal`: `530,717 -> 556,472`
    - `object_get_own_value.part.0`: `166,657 -> 167,024`
  - `matrix_add_2d`:
    - `ZrCore_Object_SetValue`: `434,047 -> 434,289`
    - `ZrCore_String_Equal`: `173,433 -> 182,004`
    - `object_get_own_value.part.0`: `91,271 -> 91,376`
  - `map_object_access`:
    - `ZrCore_Object_SetValue`: `433,843 -> 433,742`
    - `ZrCore_String_Equal`: `113,364 -> 119,684`
    - `object_get_own_value.part.0`: `88,224 -> 88,935`
- the experiment was fully removed from production code after verification; no part of this helper extraction remains in tree

### 16. Receiver object/prototype context merge in member runtime

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-093848`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_receiver_context_20260420-094026`
  - `build/benchmark-gcc-release/tests_generated/performance_suite_after_member_receiver_context_20260420-094026`

Decision:

- rejected

Reason:

- the smaller receiver-context-only retry still regressed the tracked representatives:
  - `numeric_loops`: `257,035,951 -> 257,066,093` (`+30,142`)
  - `dispatch_loops`: `354,141,124 -> 354,288,244` (`+147,120`)
  - `matrix_add_2d`: `12,914,825 -> 12,974,954` (`+60,129`)
  - `map_object_access`: `23,118,639 -> 23,158,698` (`+40,059`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays again stayed bit-for-bit identical across the same eight tracked cases, so this was still not a specialization win
- even this narrower cut continued the same bad movement:
  - `numeric_loops`:
    - `ZrCore_Object_SetValue`: `760,406 -> 758,834`
    - `ZrCore_String_Equal`: `122,692 -> 128,060`
    - `object_get_own_value.part.0`: `162,686 -> 162,399`
  - `dispatch_loops`:
    - `ZrCore_Object_SetValue`: `772,442 -> 773,790`
    - `ZrCore_String_Equal`: `530,717 -> 556,412`
    - `object_get_own_value.part.0`: `166,657 -> 167,747`
  - `matrix_add_2d`:
    - `ZrCore_Object_SetValue`: `434,047 -> 434,944`
    - `ZrCore_String_Equal`: `173,433 -> 183,428`
    - `object_get_own_value.part.0`: `91,271 -> 91,719`
  - `map_object_access`:
    - `ZrCore_Object_SetValue`: `433,843 -> 435,519`
    - `ZrCore_String_Equal`: `113,364 -> 121,084`
    - `object_get_own_value.part.0`: `88,224 -> 89,669`
- the experiment was fully removed from production code after verification; the active baseline remains unchanged

### 17. Empty own-lookup zero-count guard

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-095322`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_empty_own_lookup_zero_count_guard_20260420-095655`
  - `build/benchmark-gcc-release/tests_generated/performance_suite_after_empty_own_lookup_zero_count_guard_20260420-095655`

Decision:

- rejected

Reason:

- the narrower empty-node-map early-return still regressed the tracked representatives:
  - `numeric_loops`: `257,035,951 -> 257,091,775` (`+55,824`)
  - `dispatch_loops`: `354,141,124 -> 354,276,006` (`+134,882`)
  - `matrix_add_2d`: `12,914,825 -> 12,990,202` (`+75,377`)
  - `map_object_access`: `23,118,639 -> 23,142,571` (`+23,932`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays again stayed bit-for-bit identical across the same eight tracked cases, so this was still not a specialization or helper-mix win
- even the intended own-lookup shrink turned back into the same shared string/body loss pattern:
  - `numeric_loops`:
    - `ZrCore_String_Equal`: `122,692 -> 129,920`
    - `object_get_own_value.part.0`: `162,686 -> 162,765`
    - `ZrCore_Object_SetValue`: `760,406 -> 761,573`
  - `dispatch_loops`:
    - `ZrCore_String_Equal`: `530,717 -> 550,800`
    - `object_get_own_value.part.0`: `166,657 -> 163,973`
    - `ZrCore_Object_SetValue`: `772,442 -> 771,136`
  - `matrix_add_2d`:
    - `ZrCore_String_Equal`: `173,433 -> 185,992`
    - `object_get_own_value.part.0`: `91,271 -> 92,744`
    - `ZrCore_Object_SetValue`: `434,047 -> 434,998`
  - `map_object_access`:
    - `ZrCore_String_Equal`: `113,364 -> 119,596`
    - `object_get_own_value.part.0`: `88,224 -> 89,119`
    - `ZrCore_Object_SetValue`: `433,843 -> 435,040`
- the experiment was fully removed from production code after verification; the active baseline remains unchanged

### 18. Single-slot prototype-empty cached callable get

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-100906`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_single_slot_prototype_empty_callable_get_20260420-101029`
  - `build/benchmark-gcc-release/tests_generated/performance_suite_after_single_slot_prototype_empty_callable_get_20260420-101029`

Decision:

- rejected

Reason:

- the representative callgrind line regressed across the tracked runtime set:
  - `numeric_loops`: `257,035,951 -> 257,082,113` (`+46,162`)
  - `dispatch_loops`: `354,141,124 -> 354,300,565` (`+159,441`)
  - `matrix_add_2d`: `12,914,825 -> 12,964,680` (`+49,855`)
  - `map_object_access`: `23,118,639 -> 23,151,138` (`+32,499`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays again stayed bit-for-bit identical across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- the intended cached callable shortcut again translated into hotter shared object/string work instead of a specialization or helper-mix win:
  - `numeric_loops`:
    - `ZrCore_String_Equal`: `122,692 -> 130,720`
    - `ZrCore_Object_SetValue`: `760,406 -> 761,167`
    - `object_get_own_value.part.0`: `162,686 -> 165,129`
  - `dispatch_loops`:
    - `ZrCore_String_Equal`: `530,717 -> 558,376`
    - `ZrCore_Object_SetValue`: `772,442 -> 773,920`
    - `object_get_own_value.part.0`: `166,657 -> 167,436`
  - `matrix_add_2d`:
    - `ZrCore_String_Equal`: `173,433 -> 181,376`
    - `ZrCore_Object_SetValue`: `434,047 -> 433,199`
    - `object_get_own_value.part.0`: `91,271 -> 90,700`
  - `map_object_access`:
    - `ZrCore_String_Equal`: `113,364 -> 123,772`
    - `ZrCore_Object_SetValue`: `433,843 -> 436,679`
    - `object_get_own_value.part.0`: `88,224 -> 90,075`
- `call_chain_polymorphic` also stayed unchanged on the deferred W2 residue counters:
  - `FUNCTION_CALL`: `322 -> 322`
  - `KNOWN_NATIVE_CALL`: `1 -> 1`
  - `KNOWN_VM_CALL`: `960 -> 960`
  - `get_member`: `423 -> 423`
  - `precall`: `1,288 -> 1,288`
- the experiment was fully removed from production code after verification; no part of this callable shortcut remains in tree

### 19. Profiled dispatch getter destination skip

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-103912`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_dispatch_profiled_getter_dest_skip_20260420-104032`
  - `build/benchmark-gcc-release/tests_generated/performance_suite_after_dispatch_profiled_getter_dest_skip_20260420-104032`

Decision:

- rejected

Reason:

- the representative callgrind line regressed across the tracked runtime set:
  - `numeric_loops`: `257,035,951 -> 257,093,770` (`+57,819`)
  - `dispatch_loops`: `354,141,124 -> 354,289,799` (`+148,675`)
  - `matrix_add_2d`: `12,914,825 -> 12,998,586` (`+83,761`)
  - `map_object_access`: `23,118,639 -> 23,159,018` (`+40,379`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays again stayed bit-for-bit identical across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- the targeted helper counters also stayed unchanged, so this did not remove real runtime work:
  - `dispatch_loops` `value_copy`: `2,889,069 -> 2,889,069`
  - `dispatch_loops` `stack_get_value`: `52 -> 52`
  - `dispatch_loops` `precall`: `153,852 -> 153,852`
  - `dispatch_loops` `get_member`: `307,431 -> 307,431`
- this was another pure runtime-body/codegen regression rather than a real getter cost reduction, so the experiment was fully reverted after verification

### 20. Multi-slot lazy receiver-prototype resolution

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-105157`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_member_multislot_lazy_receiver_prototype_20260420-105157`
  - `build/benchmark-gcc-release/tests_generated/performance_suite_after_member_multislot_lazy_receiver_prototype_20260420-105157`

Decision:

- rejected

Reason:

- the representative callgrind line regressed across the tracked runtime set:
  - `numeric_loops`: `257,035,951 -> 257,124,063` (`+88,112`)
  - `dispatch_loops`: `354,141,124 -> 354,271,876` (`+130,752`)
  - `matrix_add_2d`: `12,914,825 -> 13,026,703` (`+111,878`)
  - `map_object_access`: `23,118,639 -> 23,173,155` (`+54,516`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays again stayed bit-for-bit identical across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- the target helper and instruction counters also stayed unchanged in `dispatch_loops`, so the intended member slow-lane work did not actually disappear:
  - helpers:
    - `value_copy`: `2,889,069 -> 2,889,069`
    - `stack_get_value`: `52 -> 52`
    - `precall`: `153,852 -> 153,852`
    - `get_member`: `307,431 -> 307,431`
    - `set_member`: `153,596 -> 153,596`
  - instructions:
    - `GET_MEMBER_SLOT`: `307,446 -> 307,446`
    - `FUNCTION_CALL`: `1 -> 1`
    - `KNOWN_NATIVE_CALL`: `2 -> 2`
    - `KNOWN_VM_CALL`: `4 -> 4`
- the visible shared function movement again followed the same bad pattern instead of exposing a real member-access runtime cut:
  - `dispatch_loops`:
    - `ZrCore_String_Equal`: `530,717 -> 546,984`
    - `object_get_own_value.part.0`: `166,657 -> 162,945`
    - `ZrCore_Object_SetValue`: `772,442 -> 769,521`
  - `map_object_access`:
    - `ZrCore_String_Equal`: `113,364 -> 120,276`
    - `object_get_own_value.part.0`: `88,224 -> 90,576`
    - `ZrCore_Object_SetValue`: `433,843 -> 434,933`
- this was another pure runtime-body/codegen regression rather than a real steady-state member cached win, so the experiment was fully reverted after verification

### 21. Mixed numeric add exact-fast expansion

Snapshots:

- archived live mutable benchmark dirs before rerun:
  - `build/benchmark-gcc-release/tests_generated/archived_pre_profile_20260420-105913`
- after:
  - `build/benchmark-gcc-release/tests_generated/performance_profile_tracked_non_gc_after_add_mixed_numeric_exact_fast_20260420-105913`
  - `build/benchmark-gcc-release/tests_generated/performance_suite_after_add_mixed_numeric_exact_fast_20260420-105913`

Decision:

- rejected

Reason:

- the representative callgrind line regressed across the tracked runtime set:
  - `numeric_loops`: `257,035,951 -> 257,119,310` (`+83,359`)
  - `dispatch_loops`: `354,141,124 -> 354,265,432` (`+124,308`)
  - `matrix_add_2d`: `12,914,825 -> 13,018,122` (`+103,297`)
  - `map_object_access`: `23,118,639 -> 23,166,331` (`+47,692`)
- all tracked non-GC `instructions/helpers/slowpaths` arrays again stayed bit-for-bit identical across:
  - `numeric_loops`
  - `dispatch_loops`
  - `container_pipeline`
  - `matrix_add_2d`
  - `map_object_access`
  - `string_build`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
- the target counters in `dispatch_loops` also stayed unchanged, so the intended `value_to_int64` work reduction did not show up as a tracked execution-mix change:
  - helpers:
    - `value_copy`: `2,889,069 -> 2,889,069`
    - `stack_get_value`: `52 -> 52`
    - `precall`: `153,852 -> 153,852`
    - `get_member`: `307,431 -> 307,431`
    - `set_member`: `153,596 -> 153,596`
  - instructions:
    - `ADD`: `307,200 -> 307,200`
    - `GET_MEMBER_SLOT`: `307,446 -> 307,446`
    - `FUNCTION_CALL`: `1 -> 1`
    - `KNOWN_NATIVE_CALL`: `2 -> 2`
    - `KNOWN_VM_CALL`: `4 -> 4`
- the visible shared movement again went the wrong way despite the narrower add-only hypothesis:
  - `numeric_loops`:
    - `ZrCore_String_Equal`: `122,692 -> 133,836`
    - `ZrCore_Object_SetValue`: `760,406 -> 763,090`
    - `object_get_own_value.part.0`: `162,686 -> 164,550`
  - `dispatch_loops`:
    - `ZrCore_String_Equal`: `530,717 -> 545,020`
    - `ZrCore_Object_SetValue`: `772,442 -> 767,988`
    - `object_get_own_value.part.0`: `166,657 -> 162,940`
- this was another pure runtime-body/codegen regression rather than an acceptance-quality numeric fast-path cut, so the production change was fully reverted after verification

New regression coverage kept in tree:

- `test_try_builtin_add_signed_bool_returns_int64_sum`
- `test_try_builtin_add_unsigned_bool_returns_int64_sum`
- `test_try_builtin_add_signed_unsigned_returns_int64_sum`
- `test_try_builtin_add_bool_pair_returns_int64_sum`

These tests live in:

- `tests/core/test_execution_add_stack_relocation.c`

## Current Acceptance Boundary

W1-T7/W1-T8 can now claim:

- a fresh tracked non-GC rerun exists on top of the accepted long-string cached-pair baseline
- a newly accepted shared dispatch bookkeeping cut removes profiler-visible `stack_get_value` noise from frame/base refresh paths
- a second accepted shared object/member runtime-body cut removes duplicate own-name cached lookup work from the cached descriptor/callable lane without changing specialization mix
- a third accepted shared cached-string cleanup removes checked-cast overhead from the unchecked cached string-pair lookup helpers without changing specialization mix
- specialization is still already largely landed; the tracked mix did not move during these experiments
- the recent runtime-body experiments above are mixed: the narrow cached-string raw-cast cleanup is acceptable, while the broader `SetValue` / global `value_copy` attempts were removed from production code
- the follow-up string-equality hash-gate rerun, cached member object-pair refresh, object profiled-copy variant, exact member-name pointer gate, and anchored result-buffer de-profile all missed acceptance and were removed after verification
- the follow-up existing-pair setter reuse experiment also missed acceptance, made `ZrCore_String_Equal` hotter across the tracked representatives, and was fully removed after verification
- the follow-up cached descriptor helper extraction and receiver-context retry also missed acceptance; both left tracked arrays identical while pushing `ZrCore_String_Equal` sharply upward, and both were fully removed after verification
- the follow-up empty own-lookup zero-count guard also missed acceptance; it again left tracked arrays identical while making `ZrCore_String_Equal` hotter across the representative set, and it was fully removed after verification
- the follow-up single-slot prototype-empty cached callable get also missed acceptance; it left all tracked arrays unchanged, did not improve the deferred `call_chain_polymorphic` generic-call residue, and again made `ZrCore_String_Equal` hotter across the representative set before being fully removed
- the follow-up profiled dispatch getter destination skip also missed acceptance; it left all tracked arrays and representative helper counters unchanged while regressing every tracked representative, so it was fully removed after verification
- the follow-up multi-slot lazy receiver-prototype resolution also missed acceptance; it left all tracked arrays and target member helper counts unchanged while again making `ZrCore_String_Equal` hotter, so it was fully removed after verification
- the follow-up mixed numeric add exact-fast expansion also missed acceptance; it left all tracked arrays and target counters unchanged while again making `ZrCore_String_Equal` hotter, so the production change was fully removed after verification
- the working tree keeps only:
  - the previously accepted long-string cached-pair runtime cut
  - the accepted dispatch bookkeeping `stack_get_value` de-profile cut
  - the accepted cached descriptor/callable own-name lookup wrapper cut
  - the accepted unchecked cached string-pair raw-cast cleanup
  - new regression tests that guard the relevant cache semantics
  - new dispatch metadata regression tests that lock the helper-count boundary for `KNOWN_VM_CALL` / exact-cache `KNOWN_VM_MEMBER_CALL`
  - new mixed numeric add regression tests that lock current `signed/unsigned/bool` result semantics before any future `value_to_int64` work

W1-T7/W1-T8 does not yet claim:

- that the remaining `ZrCore_Object_SetValue` / `value_copy` / `object_get_own_value.part.0` heat has already been closed

## Next Priority

The remaining measured runtime priority stays:

1. `ZrCore_Object_SetValue`
2. `ZrCore_String_Equal`
3. `object_get_own_value.part.0`
4. `execution_member_get_cached` / `execution_member_set_cached` slow-lane bodies only when the cut materially cools those shared object/string functions instead of merely shaving helper counters
5. then `value_copy`
6. bounded deferred W2 residue:
   - `call_chain_polymorphic` generic `FUNCTION_CALL`
   - `callsite_cache_lookup`

The important conclusion is unchanged: the next branch should still be driven by shared runtime-body evidence, not by restarting broad W2 quickening hit-rate chasing. After this accepted cut, `dispatch_loops` no longer has `stack_get_value` in its top helper trio; the remaining shared runtime focus is back on object/member bodies and `value_copy`.

An additional conclusion from the latest rejected retries is now explicit: pure `execution_member_access.c` callable/descriptor body reshaping with identical tracked arrays is still not enough. The next viable cut should remove real callable/descriptor/member-name work from the cached slow lane instead of only reorganizing receiver resolution, empty-receiver shortcuts, or descriptor invocation code.

## Deferred W2 Follow-Up

The previously deferred bounded `call_chain_polymorphic` W2 residue was closed
later on the same date in:

- `tests/acceptance/2026-04-20-w2-call-chain-dispatch-callable-parameter-provenance.md`

That follow-up does not change the W1 prioritization conclusion in this note.
It only narrows the old deferred pocket:

- `call_chain_polymorphic` `FUNCTION_CALL`: `322 -> 319`
- `call_chain_polymorphic` `KNOWN_VM_CALL`: `960 -> 963`
- `call_chain_polymorphic` `SUPER_DYN_TAIL_CALL_CACHED`: `320 -> 0`
- `call_chain_polymorphic` `KNOWN_VM_TAIL_CALL`: `0 -> 320`
- `call_chain_polymorphic` `callsite_cache_lookup`: `640 -> 0`

The main tracked non-GC story remains the same: after specialization closure,
the dominant remaining interpreter cost line is still shared runtime execution
work, not a broad quickening hit-rate miss.
