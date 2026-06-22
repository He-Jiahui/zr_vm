---
doc_type: plan-phase
phase: 6
title: Profiling、Coverage 与诊断工具
related_code:
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/debug_heap.c
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/profile.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - lua/QuickJS-master/quickjs.c
  - lua/cpython/
---

# Phase 6 — Profiling、Coverage 与诊断工具

在 Phase 1 的 count/call hook 之上构建「测量类」能力。这些是增量增强，可独立于调试器交付。

## 6.1 Profiling hook（CPU 采样 / 调用计数）

对标 CPython `sys.setprofile`（call/return 粒度）与「count hook 定时采样」两种模式。

### 模式 A：确定性 profiling（call/return 计数）
复用已有 CALL/RETURN hook：
- 每次 CALL 记 enter 时间戳与函数身份，RETURN 累加 self/total 时间与调用次数。
- 输出每函数：调用次数、累计耗时、自耗时。
- 新增内核侧轻量累加器或经 `FZrDebugHook` 由库实现（推荐后者：内核只提供 hook，profiler 在
  `zr_vm_lib_debug` 或新 `zr_vm_lib_profile` 中实现，避免污染内核）。

### 模式 B：采样 profiling（count hook）
- 用 Phase 1 的 COUNT hook，每 N 条指令记录当前栈顶函数+行，聚合成热点直方图。
- 低开销、统计式，适合长跑程序。

### 输出格式
- 文本 top-N + 可选 `pprof`/`speedscope` JSON（便于现成可视化）。CLI 新增 `--profile[=out]`。

## 6.2 Coverage（行覆盖）

对标 CPython `trace` 模块：
- 用 LINE hook 记录每个 (function, line) 是否被执行。
- 输出每文件已执行行集合，便于生成 lcov。CLI 新增 `--coverage[=out]`。
- 与行号表（`SZrFunctionExecutionLocationInfo`）的「可执行行集合」对照，区分「未覆盖」与
  「不可执行行」（对应 Lua getinfo 的 `L`/activelines）。

> 需要内核提供 `ZrCore_Debug_GetActiveLines(function)`：枚举该函数所有有行号映射的源行，
> 作为覆盖率的分母。数据来自 `executionLocationInfoList`，实现简单。

## 6.3 反汇编视图

开发/教学价值。已有 `ZrCore_Debug_PrintPrototypesFromData`（`debug.c:521`）做 prototype 转储，
扩展为字节码反汇编：
- 新增 `ZrCore_Debug_DisassembleFunction(state, function, FILE*)`：逐指令打印
  `offset  opcode  operands  ; line N`，行号取自行号表。
- CLI 新增 `--dump-bytecode <file.zr>`（编译后转储），便于排查 codegen。
- DAP 可选 `disassemble` 请求支持（VS Code 反汇编视图），低优先。

## 6.4 内存 / 堆检查（轻量）

不做完整堆分析，仅提供：
- `ZrCore_Debug_HeapSummary`：对象计数按类型/原型分桶、总分配字节、GC 代际统计（数据取自
  GC 子系统现有计数器）。
- CLI `--heap-summary`（退出时打印）或经 DAP custom 请求拉取。
- GC 事件 hook（可选）：GC 开始/结束的轻量回调，供 profiler 关联停顿。

## 6.5 验收

- `tests/profile/test_profile_deterministic.c`：已知调用次数的脚本，断言计数精确。
- `tests/profile/test_coverage.c`：部分分支脚本，断言已覆盖/未覆盖行集合正确，分母=activelines。
- `tests/debug/test_disassemble.c`：反汇编输出含正确 opcode 数与行号注释。
- `tests/debug/test_heap_summary.c`：堆摘要输出含 object/type/prototype/GC 统计。
- `--profile`/`--coverage`/`--dump-bytecode`/`--heap-summary` 的 CLI e2e。

## 6.6 风险

- profiling/coverage 经 hook 实现，开启时显著拖慢——必须默认关闭、按需开启，并在文档标注。
- 采样 profiler 的时间戳需单调时钟；注意跨平台（win32/wsl）时钟源差异。

## 6.7 状态与产出记录

| 时间 | 阶段 | 状态 | 完成项目 | 后续/阻塞 |
|------|------|------|----------|-----------|
| 2026-06-22 04:21:19 +08:00 | Phase 6.1 profiling | 部分完成；确定性 profiling 与 CLI `--profile` 已完成 | 新增 `zr_vm_lib_debug/profile.h` 与 `profile.c`，通过 CALL/RETURN hook 统计函数 call/return 次数、total/self 时间；新增 `tests/profile/test_profile_deterministic.c`；CLI 新增 `--profile` / `--profile=<out>`，运行成功后输出 `ZR_PROFILE deterministic` 文本报告；解析层拒绝 `--profile` 与 `--debug` 组合；`tests/cli/test_cli_args.c` 和 `tests/cli/test_cli_import_basic_fixture.c` 覆盖 parse 与 e2e report | 6.1 模式 B（COUNT hook 采样 profiling）尚未实现；6.2 coverage、6.3 反汇编、6.4 heap summary 尚未开始。WSL/GCC、WSL/Clang、Windows/MSVC focused gate `profile_deterministic|cli_args|cli_import_basic_fixture` 均 3/3 PASS |
| 2026-06-22 04:46:47 +08:00 | Phase 6.1 profiling | 完成；Phase 6 继续 | 补齐模式 B COUNT hook 采样 profiling：`ZrDebug_Profile_StartWithSampling` 按 N 条指令采样当前函数+行并聚合热点桶；`--profile` report 在确定性区块后追加 `ZR_PROFILE samples` 区块；`tests/profile/test_profile_deterministic.c` 新增采样回归，CLI fixture 断言采样 report header/columns | Phase 6.1 DoD 已满足。后续进入 6.2 coverage；6.3 反汇编、6.4 heap summary 尚未开始。WSL/GCC、WSL/Clang、Windows/MSVC focused gate `profile_deterministic|cli_args|cli_import_basic_fixture` 均 3/3 PASS |
| 2026-06-22 05:44:30 +08:00 | Phase 6.2 coverage | 完成；Phase 6 继续 | 新增 `ZrCore_Debug_GetActiveLines(function, outLines, cap)`，从 `executionLocationInfoList` 枚举函数可执行源行；新增 `zr_vm_lib_debug/coverage.h` 与 `coverage.c`，通过 LINE hook 标记 `(function,line)` 已执行状态，并在注册函数树时先写入 executable denominator；新增 `tests/profile/test_coverage.c`；CLI 新增 `--coverage` / `--coverage=<out>`，运行成功后输出 `ZR_COVERAGE lines` 文本报告；普通 interp project 在 coverage 模式下显式加载 entry function，以便执行前注册函数树；解析层拒绝 `--coverage` 与 `--debug` / `--profile` 组合；CLI args 与 import fixture 覆盖 parse/e2e report | Phase 6.2 DoD 已满足。后续进入 6.3 反汇编；6.4 heap summary 尚未开始。WSL/GCC、WSL/Clang、Windows/MSVC focused gate `coverage|cli_args|cli_import_basic_fixture` 均 3/3 PASS |
| 2026-06-22 06:36:30 +08:00 | Phase 6.3 反汇编 | 完成；Phase 6 继续 | 新增 `ZrCore_Debug_DisassembleFunction(state,function,FILE*)`，逐 instruction 输出 offset、opcode name、通用 operand raw view 与 `; line N` 注释；新增 `tests/debug/test_disassemble.c`，断言 opcode 行数等于 `function->instructionsLength` 且包含 line 注释；CLI 新增 `--dump-bytecode <out>`，在 project run / compile `--run` 加载 entry 后、执行前写出反汇编；`tests/cli/test_cli_args.c` 覆盖 parse/missing path/compile-only 约束，`tests/cli/test_cli_import_basic_fixture.c` 覆盖 binary run 输出 `ZR_DISASSEMBLY` report | Phase 6.3 DoD 已满足。后续进入 6.4 heap summary。RED 证据：WSL/GCC 手动链接在实现前报 `undefined reference to ZrCore_Debug_DisassembleFunction`。WSL/GCC、WSL/Clang、Windows/MSVC focused gate `debug_disassemble|cli_args|cli_import_basic_fixture` 均 3/3 PASS |
| 2026-06-22 07:08:48 +08:00 | Phase 6.4 内存/堆检查 | 完成；Phase 6 计划项已交付 | 新增 `ZrCore_Debug_HeapSummary(state,FILE*)`，按 raw object type/prototype 汇总对象数与 base bytes，并输出 GC region/bytes/collection counters；实现拆分到 `zr_vm_core/src/zr_vm_core/debug_heap.c`，避免继续扩大 `debug.c`；CLI 新增 `--heap-summary[=out]`，成功运行后裸参数写 stdout、带路径写文件；新增 `tests/debug/test_heap_summary.c`、CLI parse 与 import fixture e2e 覆盖 | RED：MSVC 新核心测试先报 `unresolved external symbol ZrCore_Debug_HeapSummary`，CLI RED 先报 `SZrCliCommand` 缺 `heapSummaryEnabled`/`heapSummaryOutputPath`；WSL/GCC、WSL/Clang、Windows/MSVC focused gate `debug_heap_summary|cli_args|cli_import_basic_fixture` 均 3/3 PASS。正常 WSL 多目标 build 曾被无关 parser/AOT 重编译拖到超时，已清理残留进程并用 focused 对象重编译加 registered CTest 取得验证 |
