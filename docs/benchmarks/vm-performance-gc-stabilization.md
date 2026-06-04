---
related_code:
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - tests/CMakeLists.txt
  - tests/core/test_execution_member_access_fast_paths.c
  - tests/gc/gc_tests.c
  - tests/container/test_container_runtime.c
  - tests/cmake/run_performance_suite.cmake
  - scripts/benchmark/run_gc_overhead_stress.sh
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - tests/CMakeLists.txt
  - tests/container/test_container_runtime.c
plan_sources:
  - user: 2026-05-02 GC Stress 稳定与 W3 热路径续压计划
  - tests/acceptance/2026-04-20-w3-is-unreferenced-ignore-registry-gate.md
  - tests/acceptance/2026-04-21-w1-t7-t8-existing-pair-slow-lane.md
tests:
  - tests/core/test_execution_member_access_fast_paths.c
  - tests/gc/gc_tests.c
  - tests/container/test_container_runtime.c
  - build/benchmark-gcc-release/tests_generated_gc_nopin_baseline_20260502_profile/performance/
  - build/benchmark-gcc-release/tests_generated_gc_reverted_final_20260502_profile/performance_profile_callgrind/
doc_type: workflow-detail
---

# GC Stress Stabilization Evidence

本页记录 2026-05-02 到 2026-05-03 针对 `gc_fragment_stress` 的 GC/object-write 稳定工作。第一组 public `SetValue` no-pin existing-pair probe 未达性能门槛，已撤回；随后接受了更窄的 `ZrLib_Array_PushValue` dense pair-pool append probe，并继续接受 `container.Array.add` 的 dense pair-pool append probe。

## Probe Scope

目标 probe 是在 `ZrCore_Object_SetValue` 的 direct string-key existing-pair 更新上跳过临时 `IgnoreObject/UnignoreObject` pin-unpin 循环，同时保留 existing-pair plain fast helper 的 value barrier、memberVersion bump 和 string lookup cache refresh。

本轮尝试过三种逐步收窄的实现：

| probe | fast-lane match | result |
| --- | --- | --- |
| direct hash find | direct storage key 后先查 `ZrCore_HashSet_Find` | rejected |
| cached matcher | 只查 existing string lookup cache | rejected |
| pointer cache hit | 只接受 cache pair key 指针命中 | rejected |

三者都已从源码撤回，对应 RED tests 也未保留，避免让主线携带未接受行为。

## Accepted Follow-up Probe

第二刀没有继续扩大 public `SetValue`。`ZrLib_Array_PushValue` 原本已经在 native binding 外层 pin 住数组和值，但内部仍回到 public `ZrCore_Object_SetValue`，导致数组追加再走一次通用 int-key object write。

保留的实现只在外层 pin 成功后尝试数组 dense sequential append：

- 要求目标是 array、raw-int side storage 未启用、nodeMap 已初始化、追加桶位为空。
- 使用 dense int key + hash pair-pool 直接写入新 pair。
- 仍复制 value、触发 value barrier、递增 `memberVersion`。
- 任何形状不匹配、容量/ pair-pool 准备失败、或桶位非空时都回退旧 `ZrCore_Object_SetValue`。

新增回归覆盖：

- `tests/container/test_container_runtime.c`
  - `test_container_array_push_value_uses_dense_pair_pool_for_gc_object_values`
  - RED: old path left `targetArray->nodeMap.pairPoolUsed == 0`; accepted path stores the appended GC object through pair-pool.
- `tests/gc/gc_tests.c`
  - `test_gc_array_push_value_dense_pair_pool_records_old_to_young_barrier`
  - RED: old path did not use pair-pool; accepted path also proves old array -> young object append still records remembered-set/barrier state.
- `tests/CMakeLists.txt`
  - `zr_vm_gc_test` now links `zr_vm_library` explicitly because this GC test calls public `ZrLib_Array_*` helpers.

## Rejected Follow-up: Public Plain Append No-Pin

2026-05-03 continuation tried one additional bounded probe around the remaining outer pin scope in `ZrLib_Array_PushValue`.

Scope attempted:

- Before the public array/value pinning, accept only array appends that already had dense bucket capacity and pair-pool capacity available.
- Accept only plain non-GC values, so the write could avoid value-barrier allocation and avoid rooting assumptions.
- Keep GC object/string values on the existing pinned path.

RED test added during the probe:

- `tests/container/test_container_runtime.c`
  - `test_container_array_push_value_plain_reserved_append_avoids_ignore_registry_growth`
  - Old path failed when the ignore registry was full and the next growth allocation was denied.
  - Probe implementation made the test pass.

Decision: rejected and withdrawn.

Reason:

- The real `ZrLib_Array_PushValue` call surface is dominated by metadata/task string/object appends, so this public plain-value lane would mostly add a miss check to the GC-stress shape instead of reducing `IgnoreObject/UnignoreObject`.
- WSL entered `HCS_E_CONNECTION_TIMEOUT` / `0x800705aa` while trying to continue gcc validation and Callgrind, so there is no same-build performance proof for keeping this branch.
- Per the acceptance rule, a probe without benchmark evidence is not retained. The RED test and implementation were removed from the final source state.

## Accepted Follow-up: Container Array.add Dense Append

2026-05-03 continuation found that `gc_fragment_stress` hot path uses `container.Array.add` for `survivors`, `scratch`, and `oldArchive`, while public `ZrLib_Array_PushValue` only accounts for a tiny residual slice in this case.

The accepted implementation keeps the generic `zr_container_storage_push` fallback unchanged and adds a narrow `Array.add` fast lane before that fallback:

- Accept only GC values, array hidden-items storage, initialized dense int-key nodeMap, no raw-int side storage, and empty append bucket.
- Prepare dense bucket and pair-pool capacity through the same storage primitives used by the existing raw-int fast path.
- Write the new dense int-key pair directly, copy the value, run `ZrCore_Value_Barrier`, and bump `memberVersion`.
- Map and Set storage pushes remain on the existing generic path.

New RED coverage:

- `tests/container/test_container_runtime.c`
  - `test_container_array_add_uses_dense_pair_pool_for_gc_object_values`
  - Old path returned a correct array but left hidden items on the generic object-write path (`pairPoolUsed == 0` for the append); accepted path stores the GC object in dense pair-pool storage.

## Callgrind A/B

口径：WSL gcc Release, `build/benchmark-gcc-release`, profile tier generated `gc_fragment_stress`, manual Callgrind, checksum `829044624`。

| snapshot | Callgrind Ir | delta vs fresh baseline | decision |
| --- | ---: | ---: | --- |
| fresh no-pin baseline | 892,447,828 | 0 | baseline |
| direct hash find probe | 897,170,077 | +4,722,249 (+0.529%) | rejected |
| cached matcher probe | 895,751,167 | +3,303,339 (+0.370%) | rejected |
| pointer cache hit probe | 897,179,222 | +4,731,394 (+0.530%) | rejected |
| final reverted code | 892,362,513 | -85,315 (-0.010%) | accepted as rollback |
| array push dense pair-pool | 892,243,652 | -204,176 (-0.023%) vs fresh baseline; -118,861 (-0.013%) vs final reverted | accepted |
| array push dense final retime | 892,164,677 | -283,151 (-0.032%) vs fresh baseline | accepted context |
| container Array.add dense pair-pool | 873,623,765 | -18,824,063 (-2.109%) vs fresh baseline; -18,540,912 (-2.078%) vs array push dense final retime | accepted for Callgrind |

Historical W3 totals are not directly comparable to this fresh profile run because the current workspace/runtime line now measures around 892M Ir instead of the April 2026 1.45B Ir line. They remain context only:

- W3 best: `gc_fragment_stress__manual_object_short_storage_key_fast_20260421 = 1,453,661,197 Ir`
- Later runtime line: `gc_fragment_stress__manual_existing_pair_slow_lane_20260421 = 1,455,307,029 Ir`

### Container Array.add Hot-path Confirmation

The accepted `container.Array.add` cut removes the exact number of generic object writes expected from this case:

| metric | array push dense final | container Array.add dense | delta |
| --- | ---: | ---: | ---: |
| `ZrCore_Object_SetValue` primary calls | 269,885 | 243,857 | -26,028 |
| `object_set_value_core` primary calls | 266,764 | 240,736 | -26,028 |
| `ZrCore_GarbageCollector_IgnoreObject` self Ir | 22,799,271 | 20,664,975 | -2,134,296 |
| `ZrCore_GarbageCollector_UnignoreObject` self Ir | 17,874,022 | 16,208,230 | -1,665,792 |
| `ZrCore_Value_Barrier` self Ir | 1,580,937 | 1,476,825 | -104,112 |

`ZrCore_Value_Barrier` remains in the path; the reduction is from avoiding the generic object-write lane for `Array.add`, not from dropping the barrier.

## Stress Wall Gate

The default `scripts/benchmark/run_gc_overhead_stress.sh build/benchmark-gcc-release` gate was not accepted in this workspace.

Earlier reduced stress runs timed out during `gc_fragment_stress__zr_interp`. After the `container.Array.add` cut, a direct full-scale stress CLI run completed:

- Command shape: `timeout 240s zr_vm_cli tests_generated/performance_suite/cases/gc_fragment_stress/zr/benchmark_gc_fragment_stress.zrp`
- Result: `BENCH_GC_FRAGMENT_STRESS_PASS`, checksum `47994849`, elapsed `231.91s`

However, a default full script rerun with a 40 minute outer guard still terminated while running `gc_fragment_stress__zr_interp`. It refreshed only the baseline and C-side stress JSON before termination:

| case | implementation | mean wall ms | note |
| --- | --- | ---: | --- |
| gc_fragment_baseline | C | 431.070 | refreshed 2026-05-03 |
| gc_fragment_baseline | ZR interp | 29,222.789 | refreshed 2026-05-03 |
| gc_fragment_baseline | ZR binary | 59,373.541 | refreshed 2026-05-03, noisy |
| gc_fragment_stress | C | 371.480 | refreshed 2026-05-03 |
| gc_fragment_stress | ZR interp | not refreshed | full gate terminated |
| gc_fragment_stress | ZR binary | not refreshed | full gate terminated |

Because the full stress wall run still did not complete, final acceptance remains Callgrind plus focused correctness, not a full wall-gate pass. The manual Callgrind profile/cache run keeps the pass banner/checksum stable: `BENCH_GC_FRAGMENT_STRESS_PASS`, checksum `829044624`.

A direct `gc_fragment_baseline` CLI smoke also kept the expected banner/checksum: `BENCH_GC_FRAGMENT_BASELINE_PASS`, checksum `829044624`.

## Accepted Final Hotspots

Accepted array push pair-pool manual Callgrind leaders:

| function | Ir | share |
| --- | ---: | ---: |
| `garbage_collector_run_generational_step` | 105,372,086 | 11.81% |
| `ZrCore_Execute` | 98,657,210 | 11.06% |
| `garbage_collector_run_generational_major_collection` | 59,500,980 | 6.67% |
| `object_set_value_core` | 46,969,744 | 5.26% |
| `zr_container_map_set_item_readonly_inline_no_result_fast` | 38,327,284 | 4.30% |
| `_int_malloc` | 38,157,466 | 4.28% |
| `ZrCore_String_ConcatPair` | 24,184,848 | 2.71% |
| `ZrCore_GarbageCollector_IgnoreObject` | 22,799,307 | 2.56% |
| `ZrCore_Object_SetExistingPairValueAfterFastMissUnchecked` | 21,130,382 | 2.37% |
| `ZrCore_GarbageCollector_Barrier` | 20,511,447 | 2.30% |

The first accepted cut mostly removed work from `object_set_value_core` (`47,106,890 -> 46,969,744`, `-137,146 Ir`) and did not reduce `IgnoreObject/UnignoreObject`. The second accepted `container.Array.add` cut lowers total Ir to `873,623,765` and reduces `IgnoreObject/UnignoreObject` by bypassing 26,028 generic object writes in the benchmark hot path.

Focused non-GC profile check:

- `map_object_access` manual Callgrind: `21,171,073 -> 20,952,053 Ir` versus the same 2026-05-02 fresh baseline profile (`-219,020`, `-1.03%`), checksum `317868026`.
- One-shot wall smoke for `numeric_loops`, `dispatch_loops`, `matrix_add_2d`, and `map_object_access` passed with stable checksums after the `container.Array.add` cut:
  - `BENCH_NUMERIC_LOOPS_PASS`, checksum `200014259`
  - `BENCH_DISPATCH_LOOPS_PASS`, checksum `64790779`
  - `BENCH_MATRIX_ADD_2D_PASS`, checksum `139381755`
  - `BENCH_MAP_OBJECT_ACCESS_PASS`, checksum `317868026`
  - This smoke is not used as a regression percentage gate because it was not paired against an immediate before snapshot.

## Verification Notes

Focused correctness after accepted follow-ups:

- WSL gcc `zr_vm_execution_member_access_fast_paths_test`: 102 tests, 0 failures.
- WSL gcc `zr_vm_gc_test`: 64 tests, 0 failures.
- WSL gcc `zr_vm_container_runtime_test`: 49 tests, 0 failures.
- WSL clang `zr_vm_execution_member_access_fast_paths_test`: 102 tests, 0 failures.
- WSL clang `zr_vm_gc_test`: 64 tests, 0 failures.
- WSL clang `zr_vm_container_runtime_test`: 49 tests, 0 failures.

2026-05-03 continuation notes:

- The public plain append no-pin probe was RED-confirmed and made green on WSL clang before being rejected for lack of benchmark evidence.
- WSL clang `zr_vm_execution_member_access_fast_paths_test`, `zr_vm_gc_test`, and `zr_vm_container_runtime_test` all passed while the probe was present.
- WSL gcc `zr_vm_gc_test` passed after the probe build.
- Further WSL gcc member/container reruns and Callgrind were initially blocked by WSL VM startup failures (`HCS_E_CONNECTION_TIMEOUT`, `0x800705aa`); the rejected probe was removed instead of left unmeasured.
- After WSL recovered, gcc and clang focused tests were rerun and passed with the accepted `container.Array.add` cut.

## Next Useful Cut

The public `SetValue` no-pin lane is not a good next accepted cut in the current runtime shape. The retained dense append lane is deliberately small; remaining gaps are:

- `IgnoreObject/UnignoreObject` is reduced by the `container.Array.add` cut but remains a visible cost center.
- Full `scripts/benchmark/run_gc_overhead_stress.sh` wall gate still needs a clean completed run; the latest attempt terminated during `gc_fragment_stress__zr_interp`.
- Same-build paired A/B for `numeric_loops`, `dispatch_loops`, `matrix_add_2d`, and `map_object_access` was not repeated after this probe; only checksum smoke was recorded.
- The default stress wall gate needs isolation from concurrent debug/fixture suites before its baseline wall numbers are trustworthy.
- Release builds spend significant time in CMake glob rechecking because of the current generated-file surface; performance runs should continue to use targeted rebuilds/output directories until that is cleaned up.
- Continue reducing `object_set_value_core` cost without adding miss-path work.
- Attack the remaining `IgnoreObject/UnignoreObject` cost from callers that already have stack-root evidence instead of guessing inside public `SetValue`.
- Revisit `SetExistingPairValueAfterFastMissUnchecked` and ownership/plain-value split only with a same-build Callgrind gate.
