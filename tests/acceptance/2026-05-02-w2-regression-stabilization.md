# 2026-05-02 W2 Regression Stabilization

## Scope

This round stabilized current W2 quickening work around the existing pending opcode/quickening changes. The focus was the existing non-GC regression set:

- `mixed_service_loop`
- `call_chain_polymorphic`
- `branch_jump_dense`
- `sort_array`
- `matrix_add_2d`
- `numeric_loops`
- `dispatch_loops`

## Change Summary

Three liveness/slot-contract bugs were fixed in `compiler_quickening.c`:

- `SET_MEMBER_SLOT_NULL` no longer reports a write to `operandExtra` slot `0`; it reads only the receiver slot.
- `SET_MEMBER_SLOT` no longer treats its cache/member index as a stack slot and no longer reports local stack writes.
- `SET_MEMBER` and `SET_MEMBER_SLOT` read-slot replacement now rewrites receiver reads as well as value reads, so GET_STACK copy forwarding cannot delete a receiver temp while leaving a stale receiver operand behind.

The first two issues could incorrectly stop GET_STACK copy forwarding around member-store quickening. The receiver rewrite issue was a semantic stabilization fix for the same forwarding pass.

## Focused Tests

WSL GCC Release quickening target:

```bash
cmake --build build/benchmark-gcc-release --target zr_vm_compiler_w2_performance_quickening_test --parallel 12
./build/benchmark-gcc-release/bin/zr_vm_compiler_w2_performance_quickening_test
```

Result:

- `20 Tests 0 Failures 0 Ignored`

New regression tests:

- `test_w2_set_member_slot_null_does_not_kill_slot_zero_forwarding`
- `test_w2_set_member_slot_cache_index_does_not_kill_slot_zero_forwarding`
- `test_w2_set_member_slot_receiver_forwarding_rewrites_receiver`

All three tests were confirmed red before their corresponding production fixes.

## Benchmark Evidence

The WSL GCC Release quickening and benchmark targets were checked after the final receiver-read fix:

```bash
cmake --build build/benchmark-gcc-release --target zr_vm_cli zr_vm_perf_runner zr_vm_native_benchmark_runner zr_vm_compiler_w2_performance_quickening_test --parallel 12
```

The broader `cmake --build build/benchmark-gcc-release --parallel 12` command stopped at `zr_vm_gc_test` with an unrelated link error:

- `/usr/bin/ld: ... undefined reference to symbol 'ZrLib_Array_New'`
- `/mnt/e/Git/zr_vm/build/benchmark-gcc-release/lib/libzr_vm_library.so: error adding symbols: DSO missing from command line`

Focused after-report from the first member-slot liveness fix:

- `build/benchmark-gcc-release/tests_generated_after_member_slot_liveness/performance/benchmark_report.md`
- `build/benchmark-gcc-release/tests_generated_after_member_slot_liveness/performance/benchmark_report.json`

Focused profile after-report:

- `build/benchmark-gcc-release/tests_generated_profile_after_member_slot_liveness/performance/hotspot_report.md`
- `build/benchmark-gcc-release/tests_generated_profile_after_member_slot_liveness/performance/*.profile.json`

Focused after-report after the receiver-read rewrite:

- `build/benchmark-gcc-release/tests_generated_after_set_member_receiver_forwarding/performance/benchmark_report.md`
- `build/benchmark-gcc-release/tests_generated_after_set_member_receiver_forwarding/performance/benchmark_report.json`

Focused profile after-report after the receiver-read rewrite:

- `build/benchmark-gcc-release/tests_generated_profile_after_set_member_receiver_forwarding/performance/hotspot_report.md`
- `build/benchmark-gcc-release/tests_generated_profile_after_set_member_receiver_forwarding/performance/*.profile.json`

Interpretation:

- Wall-clock numbers on `/mnt/e` were noisy across runs and moved with the C baselines.
- Instruction-profile counts for the seven focused cases did not change after these contract fixes, including the final receiver-read rewrite.
- The fixes are therefore accepted as W2 liveness/correctness stabilization, not as a proven wall-time win for the focused benchmark set.

## Matrix Validation

Fresh WSL matrix command after the receiver-read rewrite:

```powershell
.\.codex\skills\zr-vm-dev\scripts\validate-matrix.ps1 -Jobs 12 -SkipWindows
```

Observed result:

- The command timed out after 30 minutes while the WSL gcc `ctest` phase was still running.
- The leftover gcc `ctest` process was stopped after timeout so it would not keep consuming resources.
- The partial gcc `ctest` log still showed existing broad failures, including `system_fs`, `debug_agent`, `cli_integration`, and earlier `core_runtime`/`language_server_stdio_smoke` failures.
- Because the command timed out before completing, this is not counted as a fresh full gcc/clang matrix result.

Direct WSL quickening target checks after the receiver-read rewrite:

- WSL gcc Debug `zr_vm_compiler_w2_performance_quickening_test`: `20 Tests 0 Failures 0 Ignored`
- WSL clang Debug `zr_vm_compiler_w2_performance_quickening_test`: `20 Tests 0 Failures 0 Ignored`
- WSL GCC Release `zr_vm_compiler_w2_performance_quickening_test`: `20 Tests 0 Failures 0 Ignored`

Known matrix blockers remain outside the touched quickening slot-contract tests and should be treated as current baseline failures unless a later A/B proves otherwise.

Windows MSVC CLI smoke:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

Observed result:

- MSVC CLI build succeeded.
- `hello_world.zrp` printed `hello world`.

## Residual Risks

- These fixes do not unlock additional items-cache coverage in `mixed_service_loop`; calls/member writes still make that loop unsafe under the current rules.
- Benchmark wall time on the mounted worktree remains too noisy for precise small-delta acceptance.
- Full WSL matrix validation is still blocked by existing ctest failures/timeouts.
- The earlier full `build/benchmark-gcc-release` rebuild blocker is tracked below and was cleared by the follow-up build gate.
- No diagnostic pass disables remain in the production code.

## Follow-up Build And Matrix Gate

The `zr_vm_gc_test` link blocker was rechecked after the current test target linked directly against `zr_vm_library_*`:

```bash
cmake --build build/benchmark-gcc-release --target zr_vm_gc_test --parallel 12
cmake --build build/benchmark-gcc-release --parallel 12
./build/benchmark-gcc-release/bin/zr_vm_gc_test
ctest --test-dir build/benchmark-gcc-release -R '^core_runtime$' --output-on-failure --parallel 1
ctest --test-dir build/codex-wsl-gcc-debug -R '^core_runtime$' --output-on-failure --parallel 1
ctest --test-dir build/codex-wsl-clang-debug -R '^core_runtime$' --output-on-failure --parallel 1
```

Observed result:

- `zr_vm_gc_test` links in WSL GCC Release.
- Full `build/benchmark-gcc-release` rebuild succeeds.
- WSL GCC Release `zr_vm_gc_test`: `64 Tests 0 Failures 0 Ignored`.
- `core_runtime` ctest passes in WSL GCC Release, WSL gcc Debug, and WSL clang Debug.

The Node 12 `language_server_stdio_smoke` parser blocker was fixed by removing the only optional chaining expression from `tests/language_server/stdio_smoke.js`.

Validation:

```bash
node --check tests/language_server/stdio_smoke.js
ctest --test-dir build/benchmark-gcc-release -R '^language_server_stdio_smoke$' --output-on-failure --parallel 1
ctest --test-dir build/codex-wsl-gcc-debug -R '^language_server_stdio_smoke$' --output-on-failure --parallel 1
ctest --test-dir build/codex-wsl-clang-debug -R '^language_server_stdio_smoke$' --output-on-failure --parallel 1
```

Observed result:

- Node version: `v12.22.9`.
- Syntax check passes.
- `language_server_stdio_smoke` passes in WSL GCC Release, WSL gcc Debug, and WSL clang Debug.

Focused profile after the build/matrix gate:

- `build/benchmark-gcc-release/tests_generated_profile_after_gc_link_gate/performance/hotspot_report.md`
- `build/benchmark-gcc-release/tests_generated_profile_after_gc_link_gate/performance/*.profile.json`

Focused core timing after the build/matrix gate:

- `build/benchmark-gcc-release/tests_generated_after_gc_link_gate_core_retry/performance/benchmark_report.md`
- `build/benchmark-gcc-release/tests_generated_after_gc_link_gate_core_retry/performance/benchmark_report.json`

Interpretation:

- ZR interpreter instruction counts for the seven focused cases did not change relative to `tests_generated_profile_after_set_member_receiver_forwarding`.
- Helper counters did improve with the current Array PushValue dense pair-pool work: `value_copy`/`value_reset_null` each dropped by 282 to 894 calls depending on the focused case.
- Representative Callgrind total Ir also moved down slightly: `numeric_loops` -0.09%, `dispatch_loops` -0.05%, `matrix_add_2d` -0.57%.
- Single-run `/mnt/e` wall-clock timing remains too noisy for small-delta acceptance.

Remaining matrix blocker sampled after the Node fix:

```bash
ctest --test-dir build/codex-wsl-gcc-debug -R '^system_fs$' --output-on-failure --parallel 1
```

Observed result:

- `system_fs` still fails with 2 runtime assertion failures plus `No matching overload for function 'acceptFd'`.
- This is outside the W2/benchmark gate and still needs a dedicated system fs diagnosis pass.
