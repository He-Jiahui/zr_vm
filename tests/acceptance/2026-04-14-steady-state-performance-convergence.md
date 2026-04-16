# 2026-04-14 Steady-State Performance Convergence

## Scope

This round followed the user-selected hot-path-only scope:

- optimize steady-state runtime paths for `zr_interp` and `zr_binary`
- exclude archived implementations that no longer participate in mainline performance acceptance
- reuse the existing benchmark suite and filters
- treat WSL/Linux as the primary evidence environment
- validate touched shared runtime code with the WSL matrix, then run a Windows MSVC CLI smoke

## Changed Files

- `zr_vm_core/include/zr_vm_core/object.h`
- `zr_vm_core/src/zr_vm_core/object/object_internal.h`
- `zr_vm_core/src/zr_vm_core/object/object.c`

## Change Summary

The implementation adds a minimal one-entry hot cache for string-key object lookups:

- `SZrObject` now stores `cachedStringLookupPair`
- object hot-field reset clears the new cache
- `object_get_own_value()` and `object_get_own_value_unchecked()` try the cached string pair before falling back to `ZrCore_HashSet_Find()`
- `ZrCore_Object_SetValue()` refreshes the cache when the stored key is a string

The intent is to reduce repeated string-key member lookup overhead in shared object/member access paths used by both `zr_interp` and `zr_binary`.

## Benchmark Method

Mounted-worktree (`/mnt/e/...`) wall times were not used for before/after acceptance because fresh builds on `/mnt/e` and fresh builds on WSL local `/tmp` showed large filesystem-dependent timing skew even for the `C` baseline. To isolate the code change itself, acceptance used a same-filesystem A/B setup under `/tmp`.

Source roots:

- no-patch source root: `/tmp/zr_vm_perf_baseline_minus_patch_20260414` with `object.h`, `object.c`, and `object_internal.h` restored from `HEAD`
- with-patch source root: the same `/tmp` source tree with the current working-tree versions of those three files copied back in

Build roots:

- no-patch build: `/tmp/zr_vm_perf_baseline_minus_patch_20260414/build/benchmark-gcc-release-no-patch-final`
- with-patch build: `/tmp/zr_vm_perf_baseline_minus_patch_20260414/build/benchmark-gcc-release-with-patch`

Tracked case filter:

- `numeric_loops`
- `dispatch_loops`
- `container_pipeline`
- `matrix_add_2d`
- `map_object_access`
- `string_build`
- `call_chain_polymorphic`
- `mixed_service_loop`
- `gc_fragment_baseline`
- `gc_fragment_stress`

Implementation filter:

- `c,zr_interp,zr_binary,python,node,qjs,lua,rust,dotnet,java`

Commands used:

```bash
export ZR_VM_TEST_TIER=core
export ZR_VM_PERF_ONLY_CASES=numeric_loops,dispatch_loops,container_pipeline,matrix_add_2d,map_object_access,string_build,call_chain_polymorphic,mixed_service_loop,gc_fragment_baseline,gc_fragment_stress
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary,python,node,qjs,lua,rust,dotnet,java
ctest -R '^performance_report$' --test-dir /tmp/zr_vm_perf_baseline_minus_patch_20260414/build/benchmark-gcc-release-no-patch-final --output-on-failure
ctest -R '^performance_report$' --test-dir /tmp/zr_vm_perf_baseline_minus_patch_20260414/build/benchmark-gcc-release-with-patch --output-on-failure
```

Focused profile command:

```bash
export ZR_VM_TEST_TIER=profile
export ZR_VM_PERF_CALLGRIND_COUNTING=1
export ZR_VM_PERF_ONLY_CASES=dispatch_loops,map_object_access
export ZR_VM_PERF_ONLY_IMPLEMENTATIONS=c,zr_interp,zr_binary
ctest -R '^performance_report$' --test-dir /tmp/zr_vm_perf_baseline_minus_patch_20260414/build/benchmark-gcc-release --output-on-failure
ctest -R '^performance_report$' --test-dir /tmp/zr_vm_perf_baseline_minus_patch_20260414/build/benchmark-gcc-release-with-patch --output-on-failure
```

## Hotspot Evidence

Focused same-filesystem Callgrind evidence for `dispatch_loops`:

- before total IR: `806,483,753`
- after total IR: `780,484,256`
- before top helper: `ZrCore_Object_GetMemberCachedDescriptorUnchecked` at `4.88%`
- after top helper: `ZrCore_Object_GetMemberCachedDescriptorUnchecked` at `4.65%`
- top shared families stayed the same: `ZrCore_Execute`, `execution_member_get_cached`, `ZrCore_Object_GetMemberCachedDescriptorUnchecked`

Focused profile wall-time results:

- `dispatch_loops`
  - `ZR interp`: `85.440 ms -> 82.316 ms` (`-3.7%`)
  - `ZR binary`: `66.250 ms -> 63.113 ms` (`-4.7%`)
- `map_object_access`
  - `ZR interp`: `22.334 ms -> 18.316 ms` (`-18.0%`)
  - `ZR binary`: `24.701 ms -> 16.324 ms` (`-33.9%`)

Interpretation: the change reduces steady-state cost in the shared string-key member lookup path without changing the dominant hotspot family.

## Full Core A/B Results

Same-filesystem full-core comparison (`/tmp` A/B):

| case | mode | before ms | after ms | delta |
| --- | --- | ---: | ---: | ---: |
| `numeric_loops` | `zr_interp` | 62.418 | 61.673 | -1.2% |
| `numeric_loops` | `zr_binary` | 61.948 | 59.966 | -3.2% |
| `dispatch_loops` | `zr_interp` | 250.458 | 251.224 | +0.3% |
| `dispatch_loops` | `zr_binary` | 249.394 | 231.401 | -7.2% |
| `container_pipeline` | `zr_interp` | 83.508 | 71.789 | -14.0% |
| `container_pipeline` | `zr_binary` | 70.326 | 67.988 | -3.3% |
| `matrix_add_2d` | `zr_interp` | 12.637 | 12.450 | -1.5% |
| `matrix_add_2d` | `zr_binary` | 12.227 | 10.186 | -16.7% |
| `map_object_access` | `zr_interp` | 30.814 | 30.197 | -2.0% |
| `map_object_access` | `zr_binary` | 28.678 | 28.021 | -2.3% |
| `string_build` | `zr_interp` | 10.119 | 10.074 | -0.4% |
| `string_build` | `zr_binary` | 9.004 | 8.051 | -10.6% |
| `call_chain_polymorphic` | `zr_interp` | 7.790 | 7.056 | -9.4% |
| `mixed_service_loop` | `zr_interp` | 36.768 | 37.016 | +0.7% |
| `gc_fragment_baseline` | `zr_interp` | 2298.697 | 1497.784 | -34.8% |
| `gc_fragment_baseline` | `zr_binary` | 1678.166 | 1450.541 | -13.6% |
| `gc_fragment_stress` | `zr_interp` | 11716.915 | 11661.746 | -0.5% |
| `gc_fragment_stress` | `zr_binary` | FAIL | FAIL | no change |

Key acceptance points:

- the targeted non-GC hot-path family improved
- no non-GC tracked case regressed by more than 5%
- `zr_binary` gained the clearest win in `dispatch_loops`
- `map_object_access` improved modestly in full-core and strongly in focused profile

## Regression Validation

WSL matrix:

```powershell
powershell -ExecutionPolicy Bypass -File .\.codex\skills\zr-vm-dev\scripts\validate-matrix.ps1 -Jobs 8 -SkipWindows
```

Observed result:

- WSL gcc configure/build/ctest/CLI smoke passed
- WSL clang configure/build/ctest/CLI smoke passed

Windows MSVC CLI smoke:

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8
.\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

Observed result:

- `zr_vm_cli.exe` built successfully under MSVC
- `hello_world.zrp` printed `hello world`

## Residual Risks

- `gc_fragment_stress` / `ZR binary` still segfaulted in both same-filesystem A/B full-core runs, so this change does not claim to fix that instability.
- Mounted-worktree (`/mnt/e/...`) benchmark wall times remain useful for local smoke, but not for precise acceptance comparisons against `/tmp` A/B numbers.
- The implementation deliberately stayed minimal and does not change descriptor layout or callsite-cache structure; if a later optimization round is needed, the next shared-layer candidates remain `callsite_cache_lookup` and member-descriptor retrieval.
