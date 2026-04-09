# Matrix Add 2D Performance Optimization

## Scope
- Continued the `matrix_add_2d` runtime optimization milestone after the call/stack-anchor/pin split around `object_call_value` and `object_call_function_with_receiver`.
- Expanded `ZrCore_Value_Copy` with a second-stage fast path for zero-ownership GC non-`OBJECT` references while preserving the original primitive hot path.
- Added a direct cached hidden-items object pointer for super-array receivers so repeated `Array<int>` index/add/set paths do not keep re-resolving the backing `__zr_items` field.
- Fixed header inline linkage warnings by giving internal-only helpers internal linkage in `hash_set.h`, `stack.h`, and `closure.h`.
- Affected layers: core runtime execution, object/super-array helpers, shared headers, Windows/MSVC build verification, WSL performance verification.

## Baseline
- Historical user-visible baseline before the earlier phases of this milestone: Windows/MSVC `zr_vm` was roughly `20s+` to `32.9s` for `matrix_add_2d`.
- Immediate pre-slice perf baseline for this continuation:
  - WSL gcc `RelWithDebInfo` `callgrind`: `205,849,393 Ir`
    - file: `build/callgrind.matrix_add_2d.gcc.superarray-cache.out`
  - Windows MSVC `Release`: about `0.381s`
  - Windows MSVC `RelWithDebInfo`: about `0.505s`
- Known repository baseline failures before this slice and still present after it:
  - `zr_vm_execbc_aot_pipeline_test`
    - `test_aot_backends_lower_benchmark_style_generic_sub_paths`
    - `test_aot_backends_lower_benchmark_style_generic_div_paths`
    - `test_benchmark_string_build_binary_roundtrip_loads_runtime_entry`
  - Shared parser diagnostic still present:
    - `execbc_aot_signed_numeric_paths_test.zr:3:1 expected '>' but got ';'`

## Test Inventory
- Focused runtime correctness:
  - `zr_vm_container_runtime_test` on WSL gcc Debug
  - `zr_vm_container_runtime_test` on WSL clang Debug
- Known-baseline regression guard:
  - `zr_vm_execbc_aot_pipeline_test` on WSL gcc Debug
  - `zr_vm_execbc_aot_pipeline_test` on WSL clang Debug
- Performance and hotspot evidence:
  - WSL gcc `RelWithDebInfo` `matrix_add_2d` benchmark
  - WSL clang `RelWithDebInfo` `matrix_add_2d` benchmark
  - WSL gcc `RelWithDebInfo` `callgrind` on `matrix_add_2d`
  - Node Windows baseline re-run
  - Windows MSVC `Release` benchmark re-run
  - Windows MSVC `RelWithDebInfo` benchmark re-run
- Build/linkage verification:
  - WSL clang Debug build log checked for `-Wstatic-in-inline`
  - WSL clang `RelWithDebInfo` build log checked for `-Wstatic-in-inline`

## Tooling Evidence
- Tool: WSL gcc/clang builds via `cmake --build`
  - Why: validate shared-header/runtime changes on both Linux toolchains
  - Commands:
    - `cmake --build /mnt/e/Git/zr_vm/build/codex-wsl-gcc-relwithdebinfo-perf-make -j 8`
    - `cmake --build /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-relwithdebinfo-make -j 8`
    - `cmake --build /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make -j 8`
    - `cmake --build /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-debug-make -j 8`
- Tool: WSL `valgrind --tool=callgrind`
  - Why: structural hotspot comparison for the `matrix_add_2d` runtime slice
  - Command:
    - `cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && valgrind --tool=callgrind --callgrind-out-file=/mnt/e/Git/zr_vm/build/callgrind.matrix_add_2d.gcc.hidden-items-cache-v2.out /mnt/e/Git/zr_vm/build/codex-wsl-gcc-relwithdebinfo-perf-make/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary`
- Tool: WSL benchmark runs with `/usr/bin/time`
  - Why: absolute benchmark timing on both Linux toolchains
  - Commands:
    - `cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-gcc-relwithdebinfo-perf-make/lib /usr/bin/time -f "ELAPSED %e" /mnt/e/Git/zr_vm/build/codex-wsl-gcc-relwithdebinfo-perf-make/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary`
    - `cd /mnt/e/Git/zr_vm/tests/benchmarks/cases/matrix_add_2d/zr && LD_LIBRARY_PATH=/mnt/e/Git/zr_vm/build/codex-wsl-current-clang-relwithdebinfo-make/lib /usr/bin/time -f "ELAPSED %e" /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-relwithdebinfo-make/bin/zr_vm_cli benchmark_matrix_add_2d.zrp --execution-mode binary`
- Tool: Direct test binaries
  - Why: the aggregated `ctest` groups exist, but the focused acceptance cases are the binary-level targeted test executables
  - Commands:
    - `cd /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/bin && ./zr_vm_container_runtime_test`
    - `cd /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-debug-make/bin && ./zr_vm_container_runtime_test`
    - `cd /mnt/e/Git/zr_vm/build/codex-wsl-current-gcc-debug-make/bin && ./zr_vm_execbc_aot_pipeline_test`
    - `cd /mnt/e/Git/zr_vm/build/codex-wsl-current-clang-debug-make/bin && ./zr_vm_execbc_aot_pipeline_test`
- Tool: Windows MSVC builds and PowerShell timing
  - Why: verify user-facing absolute performance on Windows
  - Commands:
    - `cmake --build E:\Git\zr_vm\build\codex-msvc-release-repro --config Release --target zr_vm_cli_executable --parallel 8`
    - `Measure-Command { & 'E:\Git\zr_vm\build\codex-msvc-release-repro\bin\Release\zr_vm_cli.exe' benchmark_matrix_add_2d.zrp --execution-mode binary }`
    - `cmake -S E:\Git\zr_vm -B E:\Git\zr_vm\build\codex-msvc-cli-relwithdebinfo-current -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF`
    - `cmake --build E:\Git\zr_vm\build\codex-msvc-cli-relwithdebinfo-current --config RelWithDebInfo --target zr_vm_cli_executable --parallel 8`
    - `Measure-Command { & 'E:\Git\zr_vm\build\codex-msvc-cli-relwithdebinfo-current\bin\RelWithDebInfo\zr_vm_cli.exe' benchmark_matrix_add_2d.zrp --execution-mode binary }`
- Tool: Windows Node baseline
  - Why: keep the user-visible comparison baseline current
  - Command:
    - `cd E:\Git\zr_vm\tests\benchmarks\cases\matrix_add_2d\node; Measure-Command { node .\main.js }`

## Results
- Perf hotspot result:
  - New WSL gcc `callgrind`: `205,180,241 Ir`
    - file: `build/callgrind.matrix_add_2d.gcc.hidden-items-cache-v2.out`
  - Delta vs previous local baseline `205,849,393 Ir`: `-669,152 Ir` (`-0.325%`)
  - Delta vs call-split baseline `234,549,314 Ir`: `-29,369,073 Ir` (`-12.52%`)
- WSL benchmarks:
  - gcc `RelWithDebInfo`: `0.10s`
  - clang `RelWithDebInfo`: `0.12s`
- Windows and Node benchmarks:
  - Node Windows baseline: `0.098s`
  - MSVC `Release`: `0.266s`
  - MSVC `RelWithDebInfo`: `0.366s`
- Correctness:
  - `matrix_add_2d` checksum stayed `76802768`
  - gcc Debug `zr_vm_container_runtime_test`: `29 Tests 0 Failures`
  - clang Debug `zr_vm_container_runtime_test`: `29 Tests 0 Failures`
  - gcc Debug `zr_vm_execbc_aot_pipeline_test`: `76 Tests 3 Failures`
  - clang Debug `zr_vm_execbc_aot_pipeline_test`: `76 Tests 3 Failures`
  - The three `execbc_aot_pipeline` failures match the pre-existing baseline listed above
- Linkage warning cleanup:
  - WSL clang Debug and `RelWithDebInfo` builds no longer reported the previously observed `-Wstatic-in-inline` warnings from `value.h`, `hash_set.h`, `stack.h`, or `closure.h`
- Windows build note:
  - The existing `build/codex-msvc-release-repro` tree could not be reused for `RelWithDebInfo` because a dirty-worktree-triggered reconfigure hit an unrelated missing benchmark source under `tests/benchmarks/cases/fib_recursive/c/benchmark_case.c`
  - The workaround used a new CLI-only build with `BUILD_TESTS=OFF`, leaving unrelated user dirt untouched

## Acceptance Decision
- Accepted for this slice.
- Reason:
  - No new correctness regressions were introduced in the focused runtime coverage.
  - The known `execbc_aot_pipeline` failures stayed at the same baseline set.
  - WSL structural cost decreased again after the final `Value_Copy` fast-path correction.
  - Windows/MSVC user-facing runtime stays far below the original `20s+ / 32.9s` problem and improved further to `0.266s` (`Release`) and `0.366s` (`RelWithDebInfo`) in this session.
- Remaining risks / blockers:
  - `matrix_add_2d` is still slower than current Windows Node baseline.
  - The remaining structural hotspots are now dominated by interpreter dispatch and residual helper overhead rather than the original call/native-binding stack-anchor path.
  - The repository still has the pre-existing `execbc_aot_pipeline` baseline failures and a dirty-worktree MSVC test-build blocker unrelated to this slice.
