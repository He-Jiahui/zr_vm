# PreCall Cached Closure Fast Path

## Scope
- Continued the `PreCall/callInfo` hot-path work in `zr_vm_core/src/zr_vm_core/function.c`.
- Kept the earlier `existing VM closure` direct path and added a second direct path for:
  - zero-capture `ZR_VALUE_TYPE_FUNCTION`
  - `function->cachedStatelessClosure != NULL`
- Applied the same cached-closure fast-path idea to:
  - `ZrCore_Function_PreCallKnownValue(...)`
  - `ZrCore_Function_TryReuseTailVmCall(...)`
- Extracted a shared VM-closure stack write helper so all closure materialization paths use the same slot write contract.
- Tightened the tail-reuse regression test to assert that function-value tail reuse lands on the cached stateless closure object.

## Touched Files
- `zr_vm_core/src/zr_vm_core/function.c`
- `tests/core/test_tail_reuse_callinfo_reset.c`

## Why This Slice
- The immediately previous `profile` snapshot after the first `existing closure` fast path still showed `ZrCore_Function_PreCallKnownValue` as a first-order hotspot:
  - snapshot timestamp: `2026-04-09T04:51:04Z`
  - `dispatch_loops`
    - total Ir: `2,280,901,703`
    - `ZrCore_Function_PreCallKnownValue`: `87,737,919 Ir` (`3.85%`)
  - `map_object_access`
    - total Ir: `394,856,684`
    - `ZrCore_Function_PreCallKnownValue`: `12,613,372 Ir` (`3.19%`)
- The mixed benchmark signal from that snapshot strongly suggested the remaining waste was not only `ZR_VALUE_TYPE_CLOSURE`, but also the common case where the call site still holds a zero-capture function value and the cached closure already exists.

## Test Coverage
- Focused WSL gcc checks:
  - `zr_vm_vm_closure_precall_test`
  - `zr_vm_tail_reuse_callinfo_reset_test`
  - `zr_vm_precall_frame_slot_reset_test`
  - `zr_vm_value_copy_fast_paths_test`
  - `zr_vm_stateless_function_closure_cache_test`
  - `benchmark_registry`
  - `zr_vm_tail_call_pipeline_test`
  - CLI `hello_world`
- Focused WSL clang checks:
  - same target set as gcc
- Windows compatibility:
  - MSVC Debug CLI build
  - CLI `hello_world`
- Formal benchmark/report run:
  - WSL gcc `ZR_VM_TEST_TIER=profile ctest -R '^performance_report$'`

## Commands
- WSL gcc build:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-current-gcc-debug-make --target zr_vm_vm_closure_precall_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_precall_frame_slot_reset_test zr_vm_value_copy_fast_paths_test zr_vm_stateless_function_closure_cache_test zr_vm_benchmark_registry_test zr_vm_tail_call_pipeline_test zr_vm_cli_executable zr_vm_perf_runner zr_vm_native_benchmark_runner -j 8"`
- WSL gcc checks:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-wsl-current-gcc-debug-make/bin/zr_vm_vm_closure_precall_test && ./build/codex-wsl-current-gcc-debug-make/bin/zr_vm_tail_reuse_callinfo_reset_test && ./build/codex-wsl-current-gcc-debug-make/bin/zr_vm_precall_frame_slot_reset_test && ./build/codex-wsl-current-gcc-debug-make/bin/zr_vm_value_copy_fast_paths_test && ./build/codex-wsl-current-gcc-debug-make/bin/zr_vm_stateless_function_closure_cache_test && ctest --test-dir build/codex-wsl-current-gcc-debug-make -R '^benchmark_registry$' --output-on-failure && ./build/codex-wsl-current-gcc-debug-make/bin/zr_vm_tail_call_pipeline_test && ./build/codex-wsl-current-gcc-debug-make/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp"`
- WSL gcc formal report:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ZR_VM_TEST_TIER=profile ctest --test-dir build/codex-wsl-current-gcc-debug-make -R '^performance_report$' --output-on-failure"`
- WSL clang build:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-current-clang-debug-make && cmake --build build/codex-wsl-current-clang-debug-make --target zr_vm_vm_closure_precall_test zr_vm_tail_reuse_callinfo_reset_test zr_vm_precall_frame_slot_reset_test zr_vm_value_copy_fast_paths_test zr_vm_stateless_function_closure_cache_test zr_vm_benchmark_registry_test zr_vm_tail_call_pipeline_test zr_vm_cli_executable -j 8"`
- WSL clang checks:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-wsl-current-clang-debug-make/bin/zr_vm_vm_closure_precall_test && ./build/codex-wsl-current-clang-debug-make/bin/zr_vm_tail_reuse_callinfo_reset_test && ./build/codex-wsl-current-clang-debug-make/bin/zr_vm_precall_frame_slot_reset_test && ./build/codex-wsl-current-clang-debug-make/bin/zr_vm_value_copy_fast_paths_test && ./build/codex-wsl-current-clang-debug-make/bin/zr_vm_stateless_function_closure_cache_test && ctest --test-dir build/codex-wsl-current-clang-debug-make -R '^benchmark_registry$' --output-on-failure && ./build/codex-wsl-current-clang-debug-make/bin/zr_vm_tail_call_pipeline_test && ./build/codex-wsl-current-clang-debug-make/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp"`
- Windows MSVC smoke:
  - `cmake --build build\codex-msvc-cli-debug-current --config Debug --target zr_vm_cli_executable --parallel 8`
  - `.\build\codex-msvc-cli-debug-current\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp`

## Results
- All focused WSL gcc checks passed.
- All focused WSL clang checks passed.
- Windows MSVC CLI smoke passed and printed `hello world`.
- Formal WSL gcc `profile` report passed and refreshed the canonical performance artifacts.

## Benchmark Delta
- Immediate before snapshot:
  - `2026-04-09T04:51:04Z`
- Current snapshot:
  - `2026-04-09T04:59:50Z`

### `ZR interp` Ratio Changes (`vs C`)
- Improved:
  - `dispatch_loops: 61.499x -> 56.159x` (`-8.68%`)
  - `fib_recursive: 31.889x -> 25.789x` (`-19.13%`)
  - `call_chain_polymorphic: 26.542x -> 24.767x` (`-6.69%`)
  - `matrix_add_2d: 26.026x -> 22.816x` (`-12.34%`)
  - `string_build: 24.565x -> 23.022x` (`-6.28%`)
  - `prime_trial_division: 27.768x -> 24.856x` (`-10.49%`)
  - `branch_jump_dense: 27.546x -> 24.928x` (`-9.50%`)
  - `mixed_service_loop: 28.601x -> 25.312x` (`-11.50%`)
  - `object_field_hot: 31.415x -> 30.711x` (`-2.24%`)
- Regressed:
  - `numeric_loops: 42.257x -> 43.207x`
  - `sort_array: 20.982x -> 25.274x`
  - `map_object_access: 35.069x -> 36.299x`
  - `array_index_dense: 23.752x -> 24.771x`

## Hotspot Delta
- `dispatch_loops`
  - total Ir: `2,280,901,703 -> 2,273,067,043` (`-0.34%`)
  - `ZrCore_Function_PreCallKnownValue`: `87,737,919 -> 80,047,439 Ir` (`-8.77%`)
  - hotspot share: `3.85% -> 3.52%`
- `map_object_access`
  - total Ir: `394,856,684 -> 395,976,824` (`+0.28%`)
  - `ZrCore_Function_PreCallKnownValue`: `12,613,372 -> 12,252,643 Ir` (`-2.86%`)
  - hotspot share: `3.19% -> 3.09%`

## Interpretation
- This slice finally reduced `PreCallKnownValue` structurally, not only in wall-time noise:
  - `dispatch_loops` shows a real instruction-count drop in the targeted function.
  - `map_object_access` still spends most of its cost in native binding / stack-anchor traffic, but `PreCallKnownValue` itself also moved down.
- The new cached-stateless-function fast path matches the workload family we expected:
  - `fib_recursive`
  - `call_chain_polymorphic`
  - `mixed_service_loop`
  - `dispatch_loops`
- `map_object_access` remains mixed because its dominant cost is no longer just precall dispatch:
  - `ZrCore_Function_StackAnchorRestore`
  - `native_binding_dispatcher`
  - object/native binding helpers

## Acceptance Decision
- Accepted for this slice.
- Reason:
  - the fast path is covered by focused closure/tail-reuse/cache regressions
  - gcc + clang runtime validation stayed stable
  - Windows CLI smoke stayed compatible
  - `profile` tier completed successfully
  - call-heavy workloads improved and `PreCallKnownValue` callgrind cost dropped materially in `dispatch_loops`

## Next Slice
- Do not keep stacking more branches into `function_prepare_vm_callable_value`.
- The next highest-value work is now likely:
  - native-call precall trimming in `function_pre_call_native`
  - `StackAnchorInit/Restore` reduction on native/object call paths
  - further `stack/value` traffic reduction only where new `profile + hotspot` evidence points
