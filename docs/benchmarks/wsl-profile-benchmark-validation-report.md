---
related_code:
  - tests/benchmarks/README.md
  - tests/benchmarks/registry.cmake
  - tests/benchmarks/test_benchmark_registry.c
  - tests/cmake/run_performance_suite.cmake
  - tests/core/gdb_clang_execution_dispatch_assert.gdb
  - tests/acceptance/2026-04-09-java-benchmark-integration.md
  - tests/acceptance/2026-04-09-language-benchmark-positioning.md
  - tests/acceptance/2026-04-09-wsl-clang-performance-report-rebuild.md
implementation_files:
  - tests/benchmarks/registry.cmake
  - tests/cmake/run_performance_suite.cmake
  - tests/core/gdb_clang_execution_dispatch_assert.gdb
plan_sources:
  - user: 2026-04-09 接入 Java
  - user: 2026-04-09 还差 fib_recursive、call_chain_polymorphic、object_field_hot、array_index_dense、branch_jump_dense、mixed_service_loop 帮我完善
  - user: 2026-04-09 生成完整测试报告在 docs/benchmarks 里面
  - user: 2026-04-09 WSL clang performance_report 仍是现有 ZR interp 在 execution_dispatch.c (line 1752) 的断言失败 先修复，然后再release构建
tests:
  - tests/benchmarks/test_benchmark_registry.c
  - tests/cmake/run_performance_suite.cmake
  - tests/core/gdb_clang_execution_dispatch_assert.gdb
  - tests/acceptance/2026-04-09-wsl-clang-performance-report-rebuild.md
doc_type: milestone-detail
---

# WSL Release Profile Benchmark Validation Report

## Summary

- 当前正式 benchmark 基线已切到 WSL Release，而不是旧的 Debug 快照。
- 最新正式报表时间戳：
  - clang Release: `2026-04-09T14:51:20Z`
  - gcc Release: `2026-04-09T14:51:23Z`
- 当前 benchmark 集合为 14 个 case。
- `instruction_report` 现在为 14 个 `ZR interp` case 全量产出真实 `*.profile.json`。
- `hotspot_report` 现在为 4 个代表 workload 产出真实 callgrind 工件，不再是统一 `unavailable`。
- WSL clang `performance_report` 已不再复现 `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:1752` 的 `ZR interp` 断言失败。

## Scope And Environment

### 主验收链路

- 宿主：
  - WSL/Linux first
- 主构建目录：
  - `build/codex-wsl-current-clang-release-make`
- 交叉复核目录：
  - `build/codex-wsl-current-gcc-release-make`
- 主 suite：
  - `performance_report`
- tier：
  - `profile`

### Build Mode Confirmation

两条 Release 构建链路都已在 `CMakeCache.txt` 中确认：

| build dir | `CMAKE_BUILD_TYPE` | `CMAKE_C_FLAGS_RELEASE` | note |
| --- | --- | --- | --- |
| `build/codex-wsl-current-clang-release-make` | `Release` | `-O3 -DNDEBUG` | 当前项目标准 Release 配置 |
| `build/codex-wsl-current-gcc-release-make` | `Release` | `-O3 -DNDEBUG` | 当前项目标准 Release 配置 |

当前这轮不是 `-Ofast`、`-march=native`、PGO 或 LTO 切片。
当前能确认的是：报表确实来自项目现有的最高标准 Release 配置，也就是 `Release + -O3 -DNDEBUG`。

## Assertion Root Cause And Resolution

### Root Cause

WSL clang 的 `execution_dispatch.c` 断言不是当前 VM 源码逻辑回归，而是陈旧 clang 构建产物混编导致的 opcode 布局不一致：

- parser 侧对象和 runtime 侧对象使用了不一致的指令枚举布局。
- 结果是 parser 编出来的 `FUNCTION_RETURN` 原始指令字与 runtime 解释的 opcode 不一致。
- 具体证据：
  - 陈旧 clang 构建里，`create_instruction_2(FUNCTION_RETURN, 1, 0, 0)` 产生 `0x10051`
  - 正常 gcc 构建里，同一路径产生 `0x10056`
  - 重新构建相关 clang 目标后，clang 也恢复为 `0x10056`

### Debug Evidence

- 可复用 gdb 脚本：
  - `tests/core/gdb_clang_execution_dispatch_assert.gdb`
- 复测结果：
  - WSL clang `hello_world`: PASS
  - WSL gcc `hello_world`: PASS
  - WSL clang `benchmark_registry`: PASS
  - WSL clang Release `performance_report(profile)`: PASS

### Resolution

- 这次没有额外修改 VM runtime 源码来“绕过”断言。
- 真正修复动作是重新构建受影响的 clang benchmark 相关目标，让 parser/runtime 重新回到一致的指令布局。
- 当前 clang Release 报表已经证明：原来的 `execution_dispatch.c:1752` 断言已消失。

## Validation Matrix

| environment | command shape | result | note |
| --- | --- | --- | --- |
| WSL clang Release | `ctest --test-dir build/codex-wsl-current-clang-release-make -R '^benchmark_registry$' --output-on-failure` | PASS | 14-case registry contract is green |
| WSL clang Release | `ZR_VM_JAVA_EXE=... ZR_VM_JAVAC_EXE=... ZR_VM_TEST_TIER=profile ctest --test-dir build/codex-wsl-current-clang-release-make -R '^performance_report$' --output-on-failure` | PASS | formerly failing chain is now green |
| WSL gcc Release | `ctest --test-dir build/codex-wsl-current-gcc-release-make -R '^benchmark_registry$' --output-on-failure` | PASS | cross-toolchain contract is green |
| WSL gcc Release | `ZR_VM_JAVA_EXE=... ZR_VM_JAVAC_EXE=... ZR_VM_TEST_TIER=profile ctest --test-dir build/codex-wsl-current-gcc-release-make -R '^performance_report$' --output-on-failure` | PASS | cross-check release report is green |

说明：

- 这轮验证使用的是 benchmark 所需 target 的定向 Release 构建。
- 当前仓库仍存在与 benchmark 主线无关的全树构建阻塞：
  - `zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c`
  - 问题形态：
    - `semantic_type_from_ast` conflicting types
- 该阻塞不影响 `zr_vm_cli_executable`、`zr_vm_perf_runner`、`zr_vm_native_benchmark_runner`、`zr_vm_benchmark_registry_test` 的 Release benchmark 验收链路。

## Canonical Artifacts

当前正式产物路径固定为：

- clang Release:
  - `build/codex-wsl-current-clang-release-make/tests_generated/performance/benchmark_report.md`
  - `build/codex-wsl-current-clang-release-make/tests_generated/performance/benchmark_report.json`
  - `build/codex-wsl-current-clang-release-make/tests_generated/performance/comparison_report.md`
  - `build/codex-wsl-current-clang-release-make/tests_generated/performance/comparison_report.json`
  - `build/codex-wsl-current-clang-release-make/tests_generated/performance/instruction_report.md`
  - `build/codex-wsl-current-clang-release-make/tests_generated/performance/instruction_report.json`
  - `build/codex-wsl-current-clang-release-make/tests_generated/performance/hotspot_report.md`
  - `build/codex-wsl-current-clang-release-make/tests_generated/performance/hotspot_report.json`
- gcc Release:
  - `build/codex-wsl-current-gcc-release-make/tests_generated/performance/benchmark_report.md`
  - `build/codex-wsl-current-gcc-release-make/tests_generated/performance/comparison_report.md`
  - `build/codex-wsl-current-gcc-release-make/tests_generated/performance/instruction_report.md`
  - `build/codex-wsl-current-gcc-release-make/tests_generated/performance/hotspot_report.md`

本文后续的语言定位和热点摘要默认以 clang Release 报表为主，因为它是本轮需要恢复的失败链路。

## Suite Coverage

### Case Set

- `numeric_loops`
- `dispatch_loops`
- `container_pipeline`
- `sort_array`
- `prime_trial_division`
- `matrix_add_2d`
- `string_build`
- `map_object_access`
- `fib_recursive`
- `call_chain_polymorphic`
- `object_field_hot`
- `array_index_dense`
- `branch_jump_dense`
- `mixed_service_loop`

### Coverage Facts

- `C`: 14 / 14
- `ZR interp`: 14 / 14
- `ZR binary`: 8 / 14
- `Python`: 14 / 14
- `Node.js`: 14 / 14
- `QuickJS`: 8 / 14
- `Lua`: 8 / 14
- `Rust`: 8 / 14
- `C#/.NET`: 8 / 14
- `Java`: 14 / 14

## Language Positioning

比值口径固定为 `ZR interp / 对比语言`：

- `> 1.0x` 表示 `ZR interp` 更慢
- `< 1.0x` 表示 `ZR interp` 更快

### Aggregated Position

| language | covered cases | min ratio | max ratio | arithmetic mean | geometric mean | position |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Lua | 8 | 3.727x | 9.367x | 6.557x | 6.215x | `ZR interp` still far slower |
| QuickJS | 8 | 3.816x | 7.414x | 5.490x | 5.346x | `ZR interp` still far slower |
| Rust | 8 | 3.028x | 19.052x | 10.640x | 9.595x | `ZR interp` still far slower |
| Python | 14 | 0.838x | 2.073x | 1.539x | 1.510x | `ZR interp` slower overall, but no longer universal loss |
| Node.js | 14 | 0.616x | 2.163x | 1.101x | 1.045x | near parity, slightly slower overall |
| Java | 14 | 0.654x | 1.368x | 0.825x | 0.806x | `ZR interp` faster overall |
| C#/.NET | 8 | 0.144x | 0.304x | 0.198x | 0.192x | `ZR interp` faster on all covered cases |

### Representative Read

- 对 `Java`：
  - `ZR interp` 已在 14 个 case 中赢下 12 个
  - 当前只剩两个明显落后点：
    - `numeric_loops = 1.181x`
    - `dispatch_loops = 1.368x`
- 对 `Node.js`：
  - `ZR interp` 已在 14 个 case 中赢下 6 个
  - 几乎打平或略输的 case 仍集中在通用解释器主路径：
    - `numeric_loops = 1.752x`
    - `dispatch_loops = 2.163x`
    - `map_object_access = 1.266x`
- 对 `Python`：
  - 当前只在 `container_pipeline = 0.838x` 上赢
  - 其余 13 个 case 仍慢于 Python
- 对 `Lua / QuickJS`：
  - 当前仍存在明显差距
  - 这说明下一轮优化还不该过早转向 SIMD 或汇编级 leaf helper，主路径解释器开销仍然是主矛盾

### Full Comparison Table

| case | workload | vs C | vs Lua | vs QuickJS | vs Node.js | vs Python | vs .NET | vs Java | vs Rust |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| numeric_loops | arith,branch,loop | 24.343 | 7.161 | 4.922 | 1.752 | 1.603 | 0.248 | 1.181 | 13.560 |
| dispatch_loops | dispatch,call,loop | 49.308 | 3.727 | 5.173 | 2.163 | 1.774 | 0.304 | 1.368 | 19.052 |
| container_pipeline | container,object,copy | 22.159 | 4.411 | 4.118 | 0.616 | 0.838 | 0.155 | 0.688 | 3.028 |
| sort_array | index,branch,loop | 27.858 | 8.887 | 7.318 | 1.108 | 1.934 | 0.144 | 0.656 | 10.630 |
| prime_trial_division | branch,arith,loop | 30.312 | 7.577 | 4.969 | 1.130 | 1.547 | 0.178 | 0.815 | 9.818 |
| matrix_add_2d | index,copy,loop | 34.262 | 4.215 | 3.816 | 0.741 | 1.334 | 0.198 | 0.877 | 8.545 |
| string_build | string,object,copy | 17.591 | 7.111 | 6.189 | 0.921 | 1.366 | 0.159 | 0.771 | 8.836 |
| map_object_access | object,index,branch | 38.744 | 9.367 | 7.414 | 1.266 | 2.073 | 0.198 | 0.878 | 11.650 |
| fib_recursive | call,recursion | 22.676 | - | - | 1.021 | 1.481 | - | 0.730 | - |
| call_chain_polymorphic | call,dispatch | 19.630 | - | - | 1.048 | 1.574 | - | 0.705 | - |
| object_field_hot | object,member | 28.989 | - | - | 0.938 | 1.601 | - | 0.722 | - |
| array_index_dense | index,array | 29.630 | - | - | 0.924 | 1.423 | - | 0.725 | - |
| branch_jump_dense | branch,loop | 22.279 | - | - | 1.020 | 1.517 | - | 0.654 | - |
| mixed_service_loop | call,object,index,branch | 23.580 | - | - | 0.773 | 1.481 | - | 0.786 | - |

## Hotspot Read

当前 representative callgrind 采样已经恢复可用，热点结论仍然支持继续压 `stack/value/call` 主路径：

- `numeric_loops`
  - `ZrCore_Execute = 95.48%`
  - top helper:
    - `value_copy = 5,881,197`
- `dispatch_loops`
  - `ZrCore_Execute = 63.47%`
  - top helper counts:
    - `value_copy = 6,002,637`
    - `stack_get_value = 331,060`
    - `precall = 153,852`
  - top helper functions in callgrind:
    - `ZrCore_Object_TryGetMemberWithKeyFastUnchecked = 7.48%`
    - `ZrCore_Object_TrySetMemberWithKeyFastUnchecked = 4.50%`
- `map_object_access`
  - `ZrCore_Execute = 13.18%`
  - `ZrCore_Object_CallValue = 5.18%`
  - top helper counts:
    - `stack_get_value = 272,577`
    - `value_copy = 244,594`
    - `precall = 20,496`
  - top helper function in callgrind:
    - `ZrCore_Function_PreCallKnownValue = 3.22%`
- `matrix_add_2d`
  - callgrind 已成功生成
  - top helper counts:
    - `value_copy = 26,439`
    - `stack_get_value = 14,367`

当前热点仍然没有收敛到“纯计算已成为主矛盾”的阶段，因此下一轮继续优先压：

- `dispatch/call`
- `stack slot access`
- `value copy`
- `member/index` fast path
- `ZrCore_Function_PreCall` 与 `callInfo` 初始化路径

## Known Debts

- 全树 Release 构建
  - 仍被 `zr_vm_language_server` 的 `semantic_type_from_ast` 类型冲突阻塞

这些债务没有阻塞本轮：

- WSL clang Release `benchmark_registry`
- WSL clang Release `performance_report(profile)`
- WSL gcc Release 交叉复核

## Commands Run

### WSL clang Release

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-current-clang-release-make -R '^benchmark_registry$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ZR_VM_JAVA_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/java.exe ZR_VM_JAVAC_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/javac.exe ZR_VM_TEST_TIER=profile ctest --test-dir build/codex-wsl-current-clang-release-make -R '^performance_report$' --output-on-failure"
```

### WSL gcc Release

```bash
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-current-gcc-release-make -R '^benchmark_registry$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ZR_VM_JAVA_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/java.exe ZR_VM_JAVAC_EXE=/mnt/d/Tools/development/jdk/jdk8/bin/javac.exe ZR_VM_TEST_TIER=profile ctest --test-dir build/codex-wsl-current-gcc-release-make -R '^performance_report$' --output-on-failure"
```

## Acceptance Decision

- 接受本轮 WSL Release benchmark 验证结果。
- 原因：
  - 之前失败的 WSL clang `performance_report` 链路已经恢复
  - Release 报表明确来自 `Release + -O3 -DNDEBUG`
  - 14 个 `ZR interp` case 都有真实 profile artifact
  - 4 个代表 workload 都有真实 callgrind 摘要
  - 语言定位已经更新到最新 Release 快照，不再引用过期 Debug 结论
