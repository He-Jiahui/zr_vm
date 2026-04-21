# 2026-04-20 W3 GC Backend Hotpath Follow-up

## Scope

- Continue W3 GC backend hotpath reduction after the accepted ignore-registry gate.
- Keep GC behavior and benchmark checksums stable while cutting shared runtime cost in:
  - `zr_vm_core/src/zr_vm_core/gc/gc_internal.h`
  - `zr_vm_core/src/zr_vm_core/gc/gc_mark.c`
  - `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c`
  - `zr_vm_core/src/zr_vm_core/gc/gc_object.c`
  - `zr_vm_core/src/zr_vm_core/gc/gc_sweep.c`
  - `zr_vm_core/src/zr_vm_core/gc/gc.c`
- Re-profile `gc_fragment_stress` on fresh Callgrind snapshots after each accepted slice.

## Baseline

- Prior accepted focused-profile gate:
  - `gc_fragment_baseline: 464.255 ms -> 453.037 ms` (`-2.42%`)
  - `gc_fragment_stress: 1266.652 ms -> 1198.003 ms` (`-5.42%`)
- Callgrind baseline for the current manual W3 attribution line:
  - `gc_fragment_stress__manual_ignore_registry_fastpath = 2,506,665,731 Ir`
- Existing repository/toolchain baseline outside this slice:
  - WSL `gcc` GC test suite was green before this follow-up.
  - Windows MSVC CLI smoke had passed earlier and was not re-run in this note.
  - WSL `clang` acceptance remains blocked by the pre-existing unrelated string-table startup crash; this note does not claim that blocker is fixed.

## Test Inventory

- Focused subsystem tests:
  - `build-wsl-gcc/bin/zr_vm_gc_test`
- Focused runtime benchmark:
  - `build/benchmark-gcc-release/.../benchmark_gc_fragment_stress.zrp`
- Tool-assisted profiling:
  - manual Callgrind snapshots for each accepted W3 backend slice
- Boundary/behavior guards covered by the existing GC suite:
  - gray queue drain correctness
  - string-table forwarding rewrite
  - concat-pair cache forwarding rewrite
  - region-cache descriptor/index stability
  - old-to-young forwarding/root rewrite invariants

## Tooling Evidence

- WSL `gcc` rebuild:

```bash
cd /mnt/e/Git/zr_vm/build-wsl-gcc
cmake --build . --target zr_vm_gc_test -j 8
```

- Benchmark release rebuild:

```bash
cd /mnt/e/Git/zr_vm/build/benchmark-gcc-release
cmake --build . --target zr_vm_cli_executable -j 8
```

- Focused GC regression suite:

```bash
cd /mnt/e/Git/zr_vm/build-wsl-gcc
./bin/zr_vm_gc_test
```

- Focused benchmark correctness smoke:

```bash
cd /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr
/mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_cli ./benchmark_gc_fragment_stress.zrp
```

- Manual Callgrind profiling:

```bash
cd /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_suite/cases/gc_fragment_stress/zr
valgrind --tool=callgrind --cache-sim=no --branch-sim=no \
  --callgrind-out-file=/mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_profile_callgrind/<snapshot>.callgrind.out \
  /mnt/e/Git/zr_vm/build/benchmark-gcc-release/bin/zr_vm_cli ./benchmark_gc_fragment_stress.zrp

callgrind_annotate --auto=yes --threshold=99 \
  /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_profile_callgrind/<snapshot>.callgrind.out \
  > /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/performance_profile_callgrind/<snapshot>.annotate.txt
```

## Results

### Accepted Follow-on Cuts

- `is_unreferenced_fast`
  - add internal fast predicate and switch hot GC callers to it
- `string_table_fastpaths`
  - use known-string mark fastpath during root marking
  - rewrite only string-table keys during forwarding
- `object_size_fastpath`
  - add internal object base-size fastpath and switch hot GC callers
- `region_cache_fastreturn`
  - add current-region descriptor/index/used-byte fast return
- `root_rewrite_dedup`
  - stop pre-rewriting `state/mainThreadState` graphs inside `rewrite_forwarded_roots` because the caller immediately rewrites the full `gcObjectList`
- `clone_uninitialized_alloc`
  - evacuating clones now allocate raw object memory without the normal zero+construct path before the full memcpy
- `known_old_string_minor_skip`
  - minor root marking no longer stamps `minorScanEpoch` on known old/pinned/permanent string roots that have no outgoing GC edges
- `string_table_bucket_sync_fastpath`
  - remove unconditional minor young-bucket sync on hot mark/rewrite paths and rebuild the bucket flags when string-table growth/rehash changes hash capacity
- `region_usedbytes_fastpath`
  - cached region allocators no longer re-compare the mirrored `current*RegionUsedBytes` against the descriptor on the fast path before advancing the allocation cursor
- `clone_raw_alloc`
  - evacuation clone allocation now uses a GC-internal raw object allocation path so moving a live object no longer charges `gcDebtSize` or pays `GcMalloc` fallback overhead
- `check_sizes_region_sum`
  - `garbage_collector_check_sizes` now sums per-region `liveBytes` instead of rescanning the full `gcObjectList` and recomputing every object base size
- `gc_reference_helper_inline`
  - move `garbage_collector_object_can_hold_gc_references` into `gc_internal.h` so hot GC walkers stop paying an out-of-line helper call
- `old_compact_filter_order`
  - old-compaction gating now rejects non-`OLD_MOVABLE + OLD` and `RELEASED` objects before the expensive liveness checks, and compactable-region dedup uses per-region scan epochs instead of repeated prefix rescans
- `allocator_inline_fastpath`
  - `clone/free/new raw object` callers now inline the current-region allocation/release fast path and fall back to the out-of-line cached helpers only on cache miss or stale metadata
- `free_object_noop_guards`
  - `garbage_collector_free_object` now skips registry-forget and ownership-release calls when the object was never registered or never had ownership metadata
- `free_object_sized_lane`
  - `garbage_collector_sweep_list` now reuses its already computed `objectSize` through `garbage_collector_free_object_sized(...)` instead of forcing `garbage_collector_free_object(...)` to run the same size-switch twice
  - `garbage_collector_free_object(...)` tail cleanup no longer pays a no-op type switch for array/object/function cases; only `NATIVE_DATA` keeps the release callback lane
- `clone_size_reuse`
  - `garbage_collector_clone_for_minor_evacuation(...)` now takes caller-computed `objectSize` so the minor-evacuation lane and old-compaction lane stop recomputing the same base-size switch before every clone
- `permanent_root_skip`
  - `garbage_collector_mark_string_roots(...)` now skips permanently rooted known-string singletons (`memoryErrorMessage`, `metaFunctionName`) and sentinel API-cache entries, while the major known-string fast path early-returns for permanent strings
  - `garbage_collector_rewrite_forwarded_roots(...)` no longer rewrites permanent singleton/prototype roots, and `garbage_collector_rewrite_string_slot(...)` now early-returns for permanent strings so fixed metadata names stop paying forwarding checks

### Focused Validation

- WSL `gcc` GC suite after the latest slice:
  - `62 Tests 0 Failures 0 Ignored`
- Focused benchmark smoke after the latest slice:
  - `BENCH_GC_FRAGMENT_STRESS_PASS`
  - checksum `829044624`

### Callgrind Trend

- `gc_fragment_stress__manual_ignore_registry_fastpath`
  - `2,506,665,731 Ir`
- `gc_fragment_stress__manual_is_unreferenced_fast`
  - `2,433,119,374 Ir`
  - delta vs previous: `-73,546,357` (`-2.93%`)
- `gc_fragment_stress__manual_string_table_fastpaths`
  - `2,336,661,505 Ir`
  - delta vs previous: `-96,457,869` (`-3.96%`)
- `gc_fragment_stress__manual_object_size_fastpath`
  - `2,300,587,065 Ir`
  - delta vs previous: `-36,074,440` (`-1.54%`)
- `gc_fragment_stress__manual_region_cache_fastreturn`
  - `2,291,777,217 Ir`
  - delta vs previous: `-8,809,848` (`-0.38%`)
- `gc_fragment_stress__manual_root_rewrite_dedup`
  - `2,290,522,921 Ir`
  - delta vs previous: `-1,254,296` (`-0.05%`)
- `gc_fragment_stress__manual_clone_uninitialized_alloc`
  - `2,225,094,232 Ir`
  - delta vs previous: `-65,428,689` (`-2.86%`)
- `gc_fragment_stress__manual_known_old_string_minor_skip`
  - `2,214,669,299 Ir`
  - delta vs previous: `-10,424,933` (`-0.47%`)

### Current-Tree Follow-on Checkpoint

- `gc_fragment_stress__manual_current_tree_20260420`
  - `2,127,808,576 Ir`
- `gc_fragment_stress__manual_string_table_bucket_sync_fastpath_20260420`
  - `2,123,873,183 Ir`
  - delta vs previous: `-3,935,393` (`-0.18%`)
- `gc_fragment_stress__manual_region_usedbytes_fastpath_20260421`
  - `2,122,026,031 Ir`
  - delta vs previous: `-1,847,152` (`-0.09%`)
- `gc_fragment_stress__manual_clone_raw_alloc_20260421`
  - `2,101,631,958 Ir`
  - delta vs previous: `-20,394,073` (`-0.96%`)
- `gc_fragment_stress__manual_check_sizes_region_sum_20260421`
  - `2,074,158,522 Ir`
  - delta vs previous: `-27,473,436` (`-1.31%`)
- `gc_fragment_stress__manual_gc_reference_helper_inline_20260421`
  - `2,055,248,880 Ir`
  - delta vs previous: `-18,909,642` (`-0.91%`)
- `gc_fragment_stress__manual_old_compact_filter_order_20260421`
  - `2,047,851,866 Ir`
  - delta vs previous: `-7,397,014` (`-0.36%`)
- `gc_fragment_stress__manual_allocator_inline_fastpath_20260421`
  - `2,016,854,937 Ir`
  - delta vs previous: `-30,996,929` (`-1.51%`)
- `gc_fragment_stress__manual_free_object_noop_guards_20260421`
  - `1,988,142,127 Ir`
  - delta vs previous: `-28,712,810` (`-1.42%`)
- `gc_fragment_stress__manual_free_object_sized_20260421`
  - `1,986,657,831 Ir`
  - delta vs previous: `-1,484,296` (`-0.07%`)
- `gc_fragment_stress__manual_clone_size_reuse_20260421`
  - `1,983,460,920 Ir`
  - delta vs previous: `-3,196,911` (`-0.16%`)
- `gc_fragment_stress__manual_permanent_root_skip_20260421`
  - `1,983,263,339 Ir`
  - delta vs previous: `-197,581` (`-0.01%`)
- Net from the current-tree checkpoint:
  - `2,127,808,576 -> 1,983,263,339 Ir`
  - `-144,545,237 Ir` (`-6.79%`)
- Net from the current W3 manual-profile baseline:
  - `2,506,665,731 -> 1,983,263,339 Ir`
  - `-523,402,392 Ir` (`-20.88%`)

### Fresh Residual Hotspots

From `gc_fragment_stress__manual_permanent_root_skip_20260421.annotate.txt`:

- `garbage_collector_clone_for_minor_evacuation = 81,934,564`
- `garbage_collector_free_object = 54,295,098`
- `garbage_collector_mark_string_roots = 40,130,373`
- `garbage_collector_rewrite_forwarded_roots = 35,043,524`
- `garbage_collector_mark_value = 18,107,739`
- `garbage_collector_rewrite_object_graph.part.0 = 15,760,584`
- `garbage_collector_rewrite_hash_set = 14,110,087`
- `ZrCore_RawObject_New = 10,082,263`

Interpretation:

- the string-table bucket-flag rebuild move was real; the old unconditional sync helper dropped out of the hot list and the remaining rebuild work is now a bounded rehash-only cost around `6.09M`
- the cached allocator used-bytes fast path was also real; `garbage_collector_allocate_old_region_id_cached` dropped by about `1.65M Ir` and `garbage_collector_allocate_region_id_cached` dropped by about `0.53M Ir` without checksum drift
- the clone raw-allocation cut was materially real; `garbage_collector_clone_for_minor_evacuation` dropped by about `3.79M Ir` and `ZrCore_Memory_GcMalloc` fell from about `22.76M` to `6.61M`, confirming that evacuation should not be treated like net-new managed allocation debt
- the region-sum check-size cut was also materially real; `garbage_collector_check_sizes` disappeared from the residual hotspot list after switching that accounting pass to the existing per-region `liveBytes` counters
- caller-side allocator inline fast paths were a large real win; direct `garbage_collector_allocate_old_region_id_cached / allocate_region_id_cached / release_region_allocation_cached` calls collapsed to rare fallback-only events while total Ir still dropped by about `31.00M`
- the free-object noop guards were also real; `garbage_collector_forget_object_from_registries` fell to about `42.7K Ir` and `ZrCore_Ownership_NotifyObjectReleased` dropped out of the tracked hot list because almost all frees never carried either metadata class
- the sized free lane was also real; `garbage_collector_free_object` dropped from `56.17M` to `54.30M Ir` after removing the duplicate size-switch from the sweep path and shrinking the post-release tail to the only live `NATIVE_DATA` callback case
- the clone size-reuse lane was materially real; `garbage_collector_clone_for_minor_evacuation` dropped from `89.90M` to `81.93M Ir` after the minor-evacuation and old-compaction callers stopped recalculating object base size before every clone
- the permanent-root skip lane was a bounded but real cleanup; total Ir still dropped by about `197.6K`, `mark_string_roots` fell by about `349.0K`, and the remaining `rewrite_forwarded_roots` cost slightly ticked up, so the next cut should keep targeting the rewrite lane instead of treating this as “done”
- the next W3 backend targets should stay on GC runtime cost, now led by `clone_for_minor_evacuation`, `free_object`, `mark_string_roots`, and `rewrite_forwarded_roots`, not jump back to W2 quickening

### 2026-04-21 Continuation

#### Accepted Continuation Cuts

- `short_string_root_list_rewrite_skip`
  - keep the accepted short-string root-list mark optimization
  - remove the unnecessary short-string root-list forwarding rewrite walk from `rewrite_forwarded_roots`
  - this fixes the regressed experimental lane `gc_fragment_stress__manual_short_string_root_list_20260421 = 1,557,185,970 Ir`
- `mark_inline_fast`
  - fold `mark_object/value` minor-state + embedded-child + string fast-return checks into a GC-internal inline helper
  - switch hot GC-local string/function mark helpers to that inline lane
- `minor_reassign_inline`
  - stop routing minor-promotion region reassignment through the generic cached wrapper
  - mirror the already accepted old-compaction caller-side inline release/allocate lane directly inside `garbage_collector_reassign_minor_target(...)`

#### Focused Validation After Latest Slice

- WSL `gcc` GC suite:
  - `63 Tests 0 Failures 0 Ignored`
- focused benchmark smoke:
  - `BENCH_GC_FRAGMENT_STRESS_PASS`
  - checksum `829044624`
- Windows MSVC CLI smoke:
  - not re-run in this continuation
- WSL `clang`:
  - still blocked by the pre-existing string-table startup crash; not claimed fixed here

#### Continuation Callgrind Trend

- prior accepted best before this continuation:
  - `gc_fragment_stress__manual_old_compaction_oldlane_20260421 = 1,550,844,843 Ir`
- regressed experimental checkpoint:
  - `gc_fragment_stress__manual_short_string_root_list_20260421 = 1,557,185,970 Ir`
  - delta vs previous accepted best: `+6,341,127` (`+0.41%`)
- accepted fix:
  - `gc_fragment_stress__manual_short_string_root_list_rewrite_skip_20260421 = 1,540,488,221 Ir`
  - delta vs previous accepted best: `-10,356,622` (`-0.67%`)
- accepted mark inline follow-on:
  - `gc_fragment_stress__manual_mark_inline_fast_20260421 = 1,537,341,909 Ir`
  - delta vs previous: `-3,146,312` (`-0.20%`)
- accepted minor reassign inline follow-on:
  - `gc_fragment_stress__manual_minor_reassign_inline_20260421 = 1,529,759,802 Ir`
  - delta vs previous: `-7,582,107` (`-0.49%`)
- net for this continuation line:
  - `1,550,844,843 -> 1,529,759,802 Ir`
  - `-21,085,041` (`-1.36%`)
- net from the W3 manual-profile baseline:
  - `2,506,665,731 -> 1,529,759,802 Ir`
  - `-976,905,929` (`-38.97%`)

#### Latest Residual Hotspots

From `gc_fragment_stress__manual_minor_reassign_inline_20260421.annotate.txt`:

- `garbage_collector_run_generational_step = 124,214,460`
- `garbage_collector_run_generational_major_collection = 59,867,487`
- `garbage_collector_mark_string_roots = 28,325,905`
- `garbage_collector_mark_value = 17,340,919`
- `garbage_collector_mark_ignored_root_if_needed = 14,847,550`
- `ZrGarbageCollectorPropagateMark = 13,530,671`
- `garbage_collector_rewrite_hash_set = 8,821,121`
- `garbage_collector_sweep_list = 8,769,976`

Interpretation:

- the short-string root-list regression was real, and the culprit was specifically the extra rewrite-list walk; once removed, the short-string root-list mark cut became a net win
- the mark inline lane was also real:
  - `garbage_collector_mark_value: 18,107,244 -> 17,340,709` (`-766,535`)
  - `garbage_collector_mark_object: 4,438,405 -> 3,210,362` (`-1,228,043`)
  - `ZrGarbageCollectorReallyMarkObject: 1,041,216 -> 313,236` (`-727,980`)
- the minor reassign inline lane was materially real even though `garbage_collector_run_generational_step` rose as an inclusive caller aggregate:
  - `garbage_collector_reassign_region_id_cached` dropped out of the residual hot table entirely from `15,880,401`
  - total Ir still fell by `-7,582,107`, so this is cost relocation out of the generic wrapper, not a hidden regression
- after these two continuation cuts, the next W3 work should stay on true residual runtime cost inside the step lane:
  - `mark_string_roots`
  - `mark_ignored_root_if_needed`
  - `PropagateMark / mark_value`
  - `rewrite_hash_set / sweep_list`

### 2026-04-21 Continuation: `object_short_storage_key_fast`

#### Accepted Continuation Cut

- `object_short_storage_key_fast`
  - `ZrCore_Object_SetValue(...)` no longer re-canonicalizes already-live short interned string keys through `ZrCore_String_Create(...)`
  - the fast lane only reuses the incoming key when it is a short string and not `RELEASED`
  - released short strings still fall back to the old canonicalization lane so the string-table resurrection behavior stays intact

#### Focused Validation After Latest Slice

- WSL `gcc` core object/member regression:
  - `zr_vm_execution_member_access_fast_paths_test`
  - `70 Tests 0 Failures 0 Ignored`
- WSL `gcc` GC suite:
  - `63 Tests 0 Failures 0 Ignored`
- focused benchmark smoke:
  - `BENCH_GC_FRAGMENT_STRESS_PASS`
  - checksum `829044624`
- Windows MSVC CLI smoke:
  - not re-run in this continuation
- WSL `clang`:
  - still blocked by the pre-existing string-table startup crash; not claimed fixed here

#### Continuation Callgrind Trend

- prior accepted best before this continuation:
  - `gc_fragment_stress__manual_string_table_young_bucket_index_list_20260421 = 1,460,984,077 Ir`
- accepted short-string storage-key fast path:
  - `gc_fragment_stress__manual_object_short_storage_key_fast_20260421 = 1,453,661,197 Ir`
  - delta vs previous accepted best: `-7,322,880` (`-0.50%`)
- net from the current short-string/object-write continuation line:
  - `1,495,099,123 -> 1,453,661,197 Ir`
  - `-41,437,926` (`-2.77%`)
- net from the W3 manual-profile baseline:
  - `2,506,665,731 -> 1,453,661,197 Ir`
  - `-1,053,004,534` (`-42.01%`)

#### Latest Residual Hotspots

From `gc_fragment_stress__manual_object_short_storage_key_fast_20260421.annotate.txt`:

- `garbage_collector_run_generational_step = 107,640,114`
- `garbage_collector_run_generational_major_collection = 59,867,487`
- `ZrCore_Object_SetValue = 45,527,943`
- `ZrCore_Object_SetExistingPairValueUnchecked = 24,381,592`
- `ZrCore_GarbageCollector_IgnoreObject = 24,186,009`
- `ZrCore_GarbageCollector_UnignoreObject = 22,850,188`
- `ZrCore_GarbageCollector_Barrier = 20,384,804`
- `string_create_short = 16,837,840`
- `garbage_collector_mark_value = 17,339,893`
- `ZrGarbageCollectorPropagateMark = 13,538,321`

Interpretation:

- the short-string direct-storage-key lane is real:
  - `string_create_short: 19,105,914 -> 16,837,840` (`-2,268,074`)
  - `ZrCore_String_Create: 1,523,697 -> 775,617` (`-748,080`)
  - `ZrCore_Object_SetValue: 45,677,883 -> 45,527,943` (`-149,940`)
- `IgnoreObject / UnignoreObject` did not move, which confirms the next W3/runtime work should stay on object-write pin/unpin and existing-pair update cost instead of spending more time on key canonicalization alone
- after this slice the next profitable follow-up should stay inside:
  - `ZrCore_Object_SetValue / SetExistingPairValueUnchecked`
  - `IgnoreObject / UnignoreObject`
  - `garbage_collector_mark_value / PropagateMark`
  - `rewrite_hash_set / sweep_list`

## Acceptance Decision

Accepted as a continued W3 GC backend hotpath reduction slice.

This note claims:

- GC correctness stayed stable under focused regression and benchmark checksum validation
- the current W3 manual Callgrind line improved from `2.506B Ir` to `1.454B Ir`
- the recent accepted cuts materially reduced shared backend work without benchmark-specific special casing

This note does not claim:

- that W3 is complete
- that the full WSL `clang` matrix is green
- that Windows MSVC acceptance was freshly re-run in this exact slice

The next W3 step should continue from the latest after-state leaders:

1. `ZrCore_Object_SetValue / SetExistingPairValueUnchecked`
2. `IgnoreObject / UnignoreObject`
3. `PropagateMark / mark_value`
4. `rewrite_hash_set`
5. `sweep_list`
