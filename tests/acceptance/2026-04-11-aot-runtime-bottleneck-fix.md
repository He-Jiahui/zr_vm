# AOT Runtime Bottleneck Fix And LLVM Value-Layout Repair

## Status
- Accepted for this slice.
- Scope:
  - remove the proven `generatedFrameSlotCount` hot-path rescan from AOT runtime execution
  - keep `BeginInstruction` semantics while adding a real non-observing fast path
  - bring AOT LLVM static/int lowering up to the current AOT C baseline for `numeric_loops`
  - repair aligned LLVM stack/value layout corruption discovered while validating the AOT LLVM crash

## Root Cause Summary
- Proven catastrophic hotspot before this slice:
  - `numeric_loops` AOT C Callgrind showed `ZrCore_Function_GetGeneratedFrameSlotCount = 4,600,875,921 / 5,185,006,913 Ir = 88.73%`
  - root cause: every AOT frame slot access could rescan the source instruction stream to recompute generated slot count
- Remaining AOT LLVM crash/perf regression root cause found during this slice:
  - LLVM IR modeled `SZrTypeValue` and `SZrTypeValueOnStack` as typed field structs with the wrong layout
  - the real C runtime layout is aligned:
    - `SZrTypeValue size=48 align=16`
    - `SZrTypeValueOnStack size=64 align=16`
  - typed GEPs against the wrong layout corrupted inline slot copy and field access in generated LLVM code

## Implemented Changes
- Cached generated slot counts in public AOT frame state and precomputed per-module generated slot count tables in `zr_vm_library/src/zr_vm_library/aot_runtime.c`.
- Changed AOT frame slot helpers and bounds checks to read the cached count instead of calling `ZrCore_Function_GetGeneratedFrameSlotCount` during execution.
- Split `ZrLibrary_AotRuntime_BeginInstruction` so the non-observing path returns after frame refresh and instruction index update, without publishing PC or line events.
- Extended typed arithmetic/equality support used by `numeric_loops` so AOT C and AOT LLVM both preserve the stronger opcode families in the generated path.
- Replaced LLVM `SZrTypeValue` and `SZrTypeValueOnStack` lowering with C-sized opaque byte blobs plus byte-offset pointer math and corrected alignments for aligned stack values.
- Added regression coverage for:
  - cached slot-count bounds and stack-relocation safety
  - `BeginInstruction` observation semantics
  - LLVM direct static slot/int lowering
  - LLVM generic `ADD` fast path plus helper fallback
  - true-AOT typed signed/unsigned/equality opcode support

## Fresh Artifacts
- Core timing report copy: `/tmp/zr_perf_core_benchmark_report_20260411.md`
- Profile timing report copy: `/tmp/zr_perf_profile_benchmark_report_20260411.md`
- Profile hotspot report copy: `/tmp/zr_perf_profile_hotspot_report_20260411.md`
- Manual AOT C Callgrind annotate: `/tmp/zr_numeric_aot_c_after_20260411.callgrind.annotate.txt`
- Manual AOT LLVM Callgrind annotate: `/tmp/zr_numeric_aot_llvm_after_20260411.callgrind.annotate.txt`

## Performance Evidence

### `numeric_loops`
- User-observed pre-fix symptom at slice start: `aot_c` and `aot_llvm` were about `35x` slower than interp.
- Fresh core timing (`build/codex-wsl-gcc-release-ci-make`, `performance_report`, `2026-04-11`):
  - `ZR interp = 192.409 ms`
  - `ZR aot_c = 679.909 ms` (`3.534x` interp)
  - `ZR aot_llvm = 457.447 ms` (`2.377x` interp)
- Fresh profile timing:
  - `ZR interp = 193.182 ms`
  - `ZR aot_c = 320.625 ms` (`1.660x` interp)
  - `ZR aot_llvm = 209.567 ms` (`1.085x` interp)
- Fresh manual AOT C Callgrind:
  - total `339,380,276 Ir`
  - top hotspots:
    - `zr_aot_fn_0 = 30.38%`
    - `ZrLibrary_AotRuntime_BeginInstruction = 24.04%`
    - `__tls_get_addr = 9.85%`
    - `ZrLibrary_AotRuntime_Mod = 5.53%`
    - `ZrLibrary_AotRuntime_AddSigned = 5.35%`
  - `ZrCore_Function_GetGeneratedFrameSlotCount` is absent from the annotate output
  - total Ir dropped from `5,185,006,913` to `339,380,276` (`15.278x` lower; `6.545%` of the former total remains)
- Fresh manual AOT LLVM Callgrind:
  - total `235,112,624 Ir`
  - top hotspots:
    - `ZrLibrary_AotRuntime_BeginInstruction = 34.70%`
    - `zr_aot_fn_0 = 26.99%`
    - `ZrLibrary_AotRuntime_Mod = 7.98%`
    - `ZrLibrary_AotRuntime_ModSignedConst = 4.60%`
    - `__tls_get_addr = 4.40%`
  - `ZrCore_Function_GetGeneratedFrameSlotCount` is absent from the annotate output

### `dispatch_loops`
- Fresh core timing:
  - `ZR interp = 338.945 ms`
  - `ZR aot_c = 1057.378 ms` (`3.120x` interp)
  - `ZR aot_llvm = 660.928 ms` (`1.950x` interp)
- Fresh profile timing:
  - `ZR interp = 262.860 ms`
  - `ZR aot_c = 358.175 ms` (`1.363x` interp)
  - `ZR aot_llvm = 276.251 ms` (`1.051x` interp)
- This benchmark did not regress during the slice, but its remaining gap is still dominated by call/member/dispatch-heavy paths and stays out of scope here.

## Validation
- WSL gcc release performance rerun:
  - core: `ZR_VM_TEST_TIER=core ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp,zr_aot_c,zr_aot_llvm ctest -R '^performance_report$' --test-dir build/codex-wsl-gcc-release-ci-make --output-on-failure`
  - profile: `ZR_VM_TEST_TIER=profile ZR_VM_PERF_CALLGRIND_COUNTING=1 ZR_VM_PERF_ONLY_IMPLEMENTATIONS=zr_interp,zr_aot_c,zr_aot_llvm ctest -R '^performance_report$' --test-dir build/codex-wsl-gcc-release-ci-make --output-on-failure`
  - both commands still report existing unrelated benchmark failures already present in this worktree:
    - `container_pipeline`
    - `string_build`
    - `map_object_access`
- WSL gcc release manual Callgrind rerun:
  - `PROJECT=numeric_loops NUMERIC_SCALE=1 bash scripts/benchmark/callgrind_aot_c.sh /mnt/e/Git/zr_vm/build/codex-wsl-gcc-release-ci-make /mnt/e/Git/zr_vm /tmp/zr_numeric_aot_c_after_20260411`
  - manual `aot_llvm` Callgrind run with `--emit-aot-llvm` and `--execution-mode aot_llvm`
- WSL clang debug targeted rebuild:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_execbc_aot_pipeline_test -j 8"`
- WSL clang targeted test evidence from `zr_vm_execbc_aot_pipeline_test`:
  - `AOT LLVM Backend Directly Lowers Static Slot And Int Ops: PASS`
  - `AOT LLVM Backend Lowers Generic Add With Fast Path And Helper Fallback: PASS`
  - `ExecBC True AOT Lowers Typed Signed Unsigned And Equality Opcodes: PASS`
- WSL clang release runtime smoke:
  - `./tmp_run_numeric_benchmark_mode.sh /mnt/e/Git/zr_vm/build/codex-wsl-clang-release-ci-make aot_c 4`
  - `./tmp_run_numeric_benchmark_mode.sh /mnt/e/Git/zr_vm/build/codex-wsl-clang-release-ci-make aot_llvm 4`
  - both runs produced:
    - `BENCH_NUMERIC_LOOPS_PASS`
    - `793446923`
- Windows MSVC CLI smoke:
  - build: `cmake --build E:\Git\zr_vm\build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8`
  - run: `E:\Git\zr_vm\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe E:\Git\zr_vm\tests\fixtures\projects\hello_world\hello_world.zrp`
  - output: `hello world`

## Acceptance Decision
- Accepted for this slice.
- Acceptance criteria status:
  - `numeric_loops` AOT C Callgrind is no longer dominated by `ZrCore_Function_GetGeneratedFrameSlotCount`: satisfied
  - `numeric_loops` AOT C wall time materially improved from the catastrophic baseline: satisfied
  - `aot_llvm` materially improved on `numeric_loops` and narrowed the helper-heavy gap: satisfied
  - `dispatch_loops` did not regress in this pass: satisfied within scope

## Remaining First-Order Debt
- `ZrLibrary_AotRuntime_BeginInstruction` is now the leading first-order hotspot for both AOT C and AOT LLVM.
- Helper-heavy arithmetic/runtime traffic is still visible after the shared slot-count fix.
- `dispatch_loops` still needs separate work on call/member/dispatch paths; that remains next-slice debt rather than part of this acceptance.
