# Typed Opcode Second-Wave Perf Closure

## Scope
- Close the remaining performance-evidence gap for the strong-typed opcode second wave.
- This follow-up only changes benchmark fixtures and validation tooling:
  - `tests/fixtures/projects/benchmark_typed_equality`
  - `tests/fixtures/projects/benchmark_unsigned_arithmetic`
  - `tests/fixtures/projects/benchmark_known_call`
  - `scripts/benchmark/run_typed_opcode_perf_closure.sh`
- No parser/runtime/AOT production code was changed in this follow-up. Functional closure for the feature itself remains anchored in `tests/acceptance/2026-04-11-typed-opcode-second-wave-closure.md`.

## Baseline
- Before this follow-up, the typed-opcode second wave already had focused functional acceptance, but the plan still required fresh microbench evidence proving that:
  - typed equality hot paths no longer surface `ZrCore_Value_Equal`
  - unsigned arithmetic hot paths no longer fall back to `ADD_INT` / `SUB_INT`
  - known-call hot paths no longer surface `function_pre_call_dispatch`
- Repository-wide full-suite baselines were not rerun for this document.
- Current worktree note:
  - while rerunning the broader debug matrix, `zr_vm_execbc_aot_pipeline_test` reports one failure on both WSL gcc and WSL clang:
    - `test_aot_runtime_index_helpers_refresh_frame_for_native_binding_paths`
  - This follow-up does not modify `zr_vm_library/src/zr_vm_library/aot_runtime.c`, `zr_vm_core/src/zr_vm_core/object/object.c`, or other production paths touched by that test; only perf fixtures/scripts changed here.

## Test Inventory
- WSL release perf closure:
  - `benchmark_typed_equality`
  - `benchmark_unsigned_arithmetic`
  - `benchmark_known_call`
- Functional smoke directly relevant to this follow-up:
  - `zr_vm_known_call_pipeline_test`
  - `zr_vm_compiler_integration_test`
  - `zr_vm_cli tests/fixtures/projects/hello_world/hello_world.zrp`
- Broader debug-matrix probe for disclosure:
  - `zr_vm_execbc_aot_pipeline_test` on WSL gcc and clang, recorded as current unrelated worktree failure
- Windows compatibility smoke:
  - MSVC Debug build/run of `zr_vm_known_call_pipeline_test`
  - MSVC Debug build/run of `zr_vm_compiler_integration_test`
  - MSVC Debug CLI `hello_world`

## Tooling Evidence
- Toolchains and tools:
  - WSL gcc: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
  - WSL clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`
  - WSL valgrind: `valgrind-3.18.1`
  - Windows MSVC: `cl.exe 19.44.35224.0`
- Why these tools were used:
  - WSL release + callgrind provides the requested structural hot-path evidence.
  - WSL gcc/clang debug keeps the typed-opcode closure honest across both primary Linux toolchains.
  - Windows MSVC Debug confirms the user-facing Windows toolchain still runs the directly relevant smoke targets.
- Exact commands:

```bash
cd /mnt/e/Git/zr_vm
cmake --build build/benchmark-gcc-release --target zr_vm_cli_executable -j 8
bash scripts/benchmark/run_typed_opcode_perf_closure.sh \
    /mnt/e/Git/zr_vm/build/benchmark-gcc-release \
    /mnt/e/Git/zr_vm \
    /mnt/e/Git/zr_vm/build/benchmark-gcc-release/tests_generated/typed_opcode_perf_closure
```

```bash
cd /mnt/e/Git/zr_vm
./build/codex-wsl-gcc-debug/bin/zr_vm_known_call_pipeline_test
./build/codex-wsl-gcc-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
./build/codex-wsl-gcc-debug/bin/zr_vm_execbc_aot_pipeline_test > /tmp/zr_vm_execbc_aot_gcc.log 2>&1
grep -n ':FAIL\|FAIL - Cost\|Failed' /tmp/zr_vm_execbc_aot_gcc.log
```

```bash
cd /mnt/e/Git/zr_vm
./build/codex-wsl-clang-debug/bin/zr_vm_known_call_pipeline_test
./build/codex-wsl-clang-debug/bin/zr_vm_compiler_integration_test
./build/codex-wsl-clang-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp
./build/codex-wsl-clang-debug/bin/zr_vm_execbc_aot_pipeline_test > /tmp/zr_vm_execbc_aot_clang.log 2>&1
grep -n ':FAIL\|FAIL - Cost\|Failed' /tmp/zr_vm_execbc_aot_clang.log
```

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build E:\Git\zr_vm\build\codex-msvc-debug-typed2 --config Debug --target zr_vm_known_call_pipeline_test zr_vm_compiler_integration_test zr_vm_cli_executable --parallel 8
E:\Git\zr_vm\build\codex-msvc-debug-typed2\bin\Debug\zr_vm_known_call_pipeline_test.exe
E:\Git\zr_vm\build\codex-msvc-debug-typed2\bin\Debug\zr_vm_compiler_integration_test.exe
E:\Git\zr_vm\build\codex-msvc-debug-typed2\bin\Debug\zr_vm_cli.exe E:\Git\zr_vm\tests\fixtures\projects\hello_world\hello_world.zrp
```

## Results
- Perf report written to:
  - `build/benchmark-gcc-release/tests_generated/typed_opcode_perf_closure/typed_opcode_perf_closure_report.md`
- Typed equality:
  - generated `.zri` contains `LOGICAL_EQUAL_BOOL`, `LOGICAL_NOT_EQUAL_SIGNED`, `LOGICAL_NOT_EQUAL_UNSIGNED`, `LOGICAL_EQUAL_FLOAT`, and `LOGICAL_EQUAL_STRING`
  - generated `.zri` does not contain generic `LOGICAL_EQUAL` or `LOGICAL_NOT_EQUAL`
  - callgrind total Ir: `840,524,760`
  - `ZrCore_Value_Equal` absent from annotate output
  - `function_pre_call_dispatch` absent from annotate output
- Unsigned arithmetic:
  - generated `.zri` contains the unsigned arithmetic family and does not contain `ADD_INT` / `SUB_INT`
  - callgrind total Ir: `1,138,334,608`
  - `ZrCore_Value_Equal` absent from annotate output
  - `function_pre_call_dispatch` absent from annotate output
- Known call:
  - the perf script now generates a straight-line temporary source in the copied workdir for this case
  - inference from current quickening behavior and confirmed `.zri` output:
    - same-basic-block provenance is required for `SUPER_KNOWN_VM_CALL_NO_ARGS`
    - loop-shaped callsites naturally fall back to `SUPER_FUNCTION_CALL_NO_ARGS`, so the perf harness intentionally uses an unrolled straight-line call block instead of a loop
  - generated `.zri` contains dense `SUPER_KNOWN_VM_CALL_NO_ARGS` sites
  - callgrind total Ir: `5,383,153`
  - `function_pre_call_dispatch` absent from annotate output
  - `ZrCore_Function_PreCallKnownVmValue` appears in the hot list, which is expected for the dedicated known-VM-call fast path
- Functional smoke:
  - WSL gcc Debug:
    - `zr_vm_known_call_pipeline_test`: `4 Tests 0 Failures 0 Ignored`
    - `zr_vm_compiler_integration_test`: `78 Tests 0 Failures 0 Ignored`
    - `zr_vm_cli hello_world`: `hello world`
  - WSL clang Debug:
    - `zr_vm_known_call_pipeline_test`: `4 Tests 0 Failures 0 Ignored`
    - `zr_vm_compiler_integration_test`: `78 Tests 0 Failures 0 Ignored`
    - `zr_vm_cli hello_world`: `hello world`
  - Windows MSVC Debug:
    - `zr_vm_known_call_pipeline_test`: `4 Tests 0 Failures 0 Ignored`
    - `zr_vm_compiler_integration_test`: `78 Tests 0 Failures 0 Ignored`
    - `zr_vm_cli hello_world`: `hello world`
- Current unrelated debug-matrix failure, captured but not caused by this follow-up:
  - WSL gcc `zr_vm_execbc_aot_pipeline_test`: `91 Tests 1 Failures 0 Ignored`
  - WSL clang `zr_vm_execbc_aot_pipeline_test`: `91 Tests 1 Failures 0 Ignored`
  - failing case:
    - `test_aot_runtime_index_helpers_refresh_frame_for_native_binding_paths`

## Acceptance Decision
- Accepted as the perf-evidence follow-up for the typed-opcode second wave.
- Exact reason:
  - the missing microbench evidence now exists for typed equality, unsigned arithmetic, and known-call
  - the generated release `.zri` artifacts prove new lowering behavior instead of relying on inspection alone
  - the requested hot-path bans are satisfied in callgrind output:
    - `ZrCore_Value_Equal` absent for typed equality and unsigned arithmetic
    - `function_pre_call_dispatch` absent for all three cases, including the dedicated known-call benchmark
  - WSL gcc/clang and Windows MSVC still pass the directly relevant known-call/compiler/CLI smoke targets
- Remaining risks:
  - this document does not re-accept the full typed-opcode milestone by itself; it extends the prior functional closeout with release perf evidence
  - current worktree still has an unrelated `zr_vm_execbc_aot_pipeline_test` failure in `test_aot_runtime_index_helpers_refresh_frame_for_native_binding_paths`; that issue should be handled separately before any broader “all targeted debug matrix is green” claim
