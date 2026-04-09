# PreCall / CallInfo Hotpath Optimization

## Status
- Accepted for this slice.
- Focus area: `ZrCore_Function_PreCall`, `callInfo` reuse/init, and stack-check setup on the call hot path.
- Primary validation chain: WSL/Linux first. Windows kept at CLI smoke.

## Baseline Before This Slice
- Baseline artifacts:
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/comparison_report.md`
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/hotspot_report.md`
- Baseline profile snapshot timestamp: `2026-04-08T18:40:04Z`
- Key baseline ratios (`ZR interp vs C`):
  - `dispatch_loops = 76.498x`
  - `map_object_access = 34.021x`
  - `call_chain_polymorphic = 30.075x`
  - `mixed_service_loop = 27.858x`
  - `matrix_add_2d = 27.154x`
  - `fib_recursive = 27.214x`
- Key baseline hotspots:
  - `dispatch_loops`
    - `Callgrind total Ir = 2,938,899,363`
    - `ZrCore_Function_PreCall = 100,954,046 Ir (3.44%)`
    - helpers: `stack_get_value = 639461`, `value_copy = 621121`, `get_member = 461287`
  - `map_object_access`
    - `Callgrind total Ir = 402,917,085`
    - `ZrCore_Function_PreCall = 9,194,691 Ir (2.28%)`
    - helpers: `stack_get_value = 284869`, `value_copy = 28335`, `precall = 20496`

## Implemented Changes
- Replaced hot-path `memset(callInfo, 0, sizeof(*callInfo))` with explicit reuse/init helpers in `zr_vm_core/src/zr_vm_core/function.c`:
  - `function_acquire_call_info`
  - `function_init_call_info_common`
  - `function_init_native_call_info`
  - `function_init_vm_call_info`
- Removed redundant default writes followed by immediate overwrites in VM/native precall setup.
- Cached `debugHookSignal` in native precall instead of rereading thread state inside the hot branch.
- Explicitly zeroed `TZrCallInfoContext` and `TZrCallInfoYieldContext` unions on reuse instead of whole-struct clearing.
  - This kept the optimization but restored stable reused-frame semantics.
- Added a no-grow fast path in `function_restore_call_pointers_after_stack_check`.
  - The common case now returns immediately when `checkPointer + size` still fits in the current stack.
  - Anchor creation/restoration only happens on real stack growth.

## Debugging Snapshot
- First attempt at explicit field-only init caused `core` benchmark failures in:
  - `container_pipeline`
  - `string_build`
  - `map_object_access`
- Failure shape:
  - `ZrCore_Execute` assertion: `base == callInfo->functionBase.valuePointer + 1`
- Root-cause investigation used direct benchmark repro and WSL `gdb`.
- The reproducible evidence showed:
  - nested native call stack growth relocated the active frame
  - `callInfo->functionBase` had the new stack address
  - the executing `base` local still held the old stack address
- The optimization was stabilized by:
  - explicitly zeroing reused `context/yield` unions
  - then adding the no-grow fast path so the common call path no longer paid anchor setup cost

## Final Results
- Final profile snapshot timestamp: `2026-04-08T20:03:26Z`
- Final report artifacts:
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/benchmark_report.md`
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/comparison_report.md`
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/instruction_report.md`
  - `build/codex-wsl-current-gcc-debug-make/tests_generated/performance/hotspot_report.md`

### Wall-Time Comparison (`ZR interp vs C`)
- Improved:
  - `dispatch_loops: 76.498x -> 74.702x` (`-2.35%`)
  - `container_pipeline: 38.628x -> 36.782x` (`-4.78%`)
  - `matrix_add_2d: 27.154x -> 22.484x` (`-17.20%`)
  - `fib_recursive: 27.214x -> 18.901x` (`-30.54%`)
  - `call_chain_polymorphic: 30.075x -> 28.206x` (`-6.21%`)
  - `object_field_hot: 38.127x -> 34.375x` (`-9.84%`)
  - `mixed_service_loop: 27.858x -> 19.470x` (`-30.11%`)
  - `string_build: 24.246x -> 23.992x` (`-1.05%`)
- Regressed:
  - `numeric_loops: 34.321x -> 42.903x`
  - `sort_array: 21.904x -> 24.280x`
  - `prime_trial_division: 26.059x -> 27.169x`
  - `map_object_access: 34.021x -> 34.737x`
  - `array_index_dense: 23.564x -> 33.406x`
  - `branch_jump_dense: 23.894x -> 28.672x`

### Hotspot Delta
- `dispatch_loops`
  - `Callgrind total Ir: 2,938,899,363 -> 2,596,053,707` (`-11.67%`)
  - `ZrCore_Function_PreCall: 100,954,046 -> 66,207,561 Ir` (`-34.41%`)
  - `stack_get_value: 639461 -> 331060` (`-48.23%`)
  - `protect_e: 77040 -> 3`
- `map_object_access`
  - `Callgrind total Ir: 402,917,085 -> 418,366,088` (`+3.83%`)
  - `ZrCore_Function_PreCall: 9,194,691 -> 9,214,976 Ir` (`+0.22%`)
  - `stack_get_value: 284869 -> 272577` (`-4.31%`)
  - `protect_e: none -> 16394`
- `matrix_add_2d`
  - `Callgrind total Ir: 45,854,286 -> 44,826,404` (`-2.24%`)

## Validation
- WSL gcc Debug build:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-current-gcc-debug-make -j 8"`
- WSL clang Debug build:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-current-clang-debug-make -j 8"`
- WSL `benchmark_registry`:
  - gcc: passed
  - clang: passed
- WSL CLI smoke:
  - gcc: `hello world`
  - clang: `hello world`
- WSL performance report:
  - `ZR_VM_TEST_TIER=smoke`: passed
  - `ZR_VM_TEST_TIER=core`: passed
  - `ZR_VM_TEST_TIER=profile`: passed
- Windows MSVC CLI smoke:
  - build: `cmake --build build\codex-msvc-cli-debug-current --config Debug --target zr_vm_cli_executable --parallel 8`
  - run: `.\build\codex-msvc-cli-debug-current\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp`
  - output: `hello world`

## Acceptance Decision
- Accepted for this slice.
- Reason:
  - `callInfo` reuse/init no longer depends on whole-struct `memset`.
  - The hot call path now skips stack-anchor setup entirely when no growth is required.
  - The `profile` tier again produces complete instruction artifacts for every successful `ZR interp` case.
  - `dispatch_loops` and multiple call-heavy workloads improved structurally and in wall-time.

## Remaining Hotspots / Next Slice
- The next high-value work is no longer `callInfo` init itself.
- Current dominant remaining hotspots are:
  - `ZrCore_Ownership_AssignValue`
  - `value_copy`
  - `stack_get_value`
  - object/index slowpaths around `protect_e`
- The strongest next slice is to continue on `stack/value/call` mainline:
  - trim `ownership/value_copy` traffic
  - reduce `protect_e` frequency in object/index paths
  - only revisit `PreCall` again if new data shows another concrete win there
