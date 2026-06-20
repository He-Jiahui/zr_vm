---
doc_type: plan-phase
phase: 6
title: Profiling、Coverage 与诊断工具
related_code:
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/include/zr_vm_core/debug.h
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
- `--profile`/`--coverage`/`--dump-bytecode` 的 CLI e2e。

## 6.6 风险

- profiling/coverage 经 hook 实现，开启时显著拖慢——必须默认关闭、按需开启，并在文档标注。
- 采样 profiler 的时间戳需单调时钟；注意跨平台（win32/wsl）时钟源差异。
