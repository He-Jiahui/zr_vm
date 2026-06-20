---
doc_type: plan-phase
phase: 3
title: Traceback 生成与错误诊断
related_code:
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - lua/src/lauxlib.c
  - lua/QuickJS-master/quickjs.c
---

# Phase 3 — Traceback 生成与错误诊断

让运行时错误/未捕获异常携带**人类可读的多帧栈回溯**，并把生成逻辑做成可复用单元，供
CLI 错误输出、DAP 异常详情、脚本 `debug.traceback` 三处共用。

## 状态与产出记录

| 时间 | 状态 | 完成项目 | 后续/阻塞 |
|------|------|----------|-----------|
| 2026-06-20 11:51:09 +08:00 | 进行中；核心生成器完成；阶段验收未完成 | 新增 `ZrCore_Debug_Traceback` API 与独立 `debug_traceback.c`，避免继续扩张已超过 1000 行的 `debug.c`；实现前缀、多帧脚本栈、native/script/native 混合帧、深栈折叠、小 buffer NUL 截断；异常规范化时写入 `stack` 文本字段并保留 `stacks` 数组，未捕获异常格式器优先使用统一文本；新增 `tests/debug/test_debug_traceback.c` 并注册 CMake/CTest | 手工 WSL/GCC focused 链接运行 4/4 PASS；旧 WSL/GCC 目录正式目标构建两次在 CMake/Ninja glob/regeneration 阶段超时；新 `build/codex-debug-phase3-wsl-gcc` 配置成功，但目标构建因完整依赖编译超时仍未产出正式可执行。仍需补 CLI e2e 命令级验证、DAP exception detail、MSVC/WSL 正式目标构建与 CTest |
| 2026-06-20 12:47:18 +08:00 | 完成；里程碑 A 全矩阵待补跑 | 正式 WSL/GCC `zr_vm_debug_traceback_test` 目标构建与 `debug_traceback` CTest 通过；补齐 CLI 未捕获错误回溯 e2e；debug agent 异常停止事件输出 `exceptionStack`；VS Code DAP adapter 支持 `exceptionInfo` 并把异常栈写入 stderr output；focused 回归 7/7 PASS，扩展 compile/unit 29/29 PASS | Phase 3 DoD 已满足；进入 Phase 4 前按 `07-testing-and-acceptance.md` §7.5 补跑里程碑 A 全量三构建 `tests/` 与扩展桌面 smoke |

## 3.1 现状

- `ZrCore_Debug_RunError`（`debug.c:190`）：格式化单行 message → 规范化为 Error 对象 →
  进入 VM 异常链路。**不含栈回溯**。
- 异常对象已带 "stacks"（`exception.c` 内 NormalizeStatus 路径提及），但缺「渲染成
  `file:line: in function` 文本」的统一函数。
- CLI 未捕获错误输出（`runtime.c`）目前只打印 message。

## 3.2 新增统一 traceback 生成器

### API（`debug.h`）
```c
// 从 state 当前调用栈生成多帧回溯文本，写入 buffer（截断安全，保证 NUL 结尾）。
// level: 起始层级（0=当前帧）；maxFrames: 最多渲染帧数，0=不限。
// 返回写入的字节数（不含 NUL）。
ZR_CORE_API TZrSize ZrCore_Debug_Traceback(struct SZrState *state,
                                           TZrNativeString prefixMessage,
                                           TZrUInt32 level,
                                           TZrUInt32 maxFrames,
                                           TZrChar *buffer,
                                           TZrSize bufferSize);
```

### 实现（对照 `luaL_traceback` `lauxlib.c:132` 与 QuickJS `build_backtrace`）
逐层 `ZrCore_Debug_GetStack(level)` + `GetInfo`，每帧渲染一行：
```
  at <function_name> (<source_file>:<line>)        // VM 帧
  at <native_name> [native]                        // native 帧
  at <function_name> (<source_file>:<line>:<col>)  // 有列号时（zr 行号表带列，优于 Lua）
```
- **省略中段**：帧数超过阈值（如 > 21）时，按 Lua 风格折叠为
  `... (skipping N levels)`，避免超长栈刷屏。
- **尾调用标注**：`isTailCall` 为真时追加 `(...tail calls...)`。
- **截断安全**：所有写入用边界检查的追加器，buffer 满即停止并保证 NUL 结尾。

## 3.3 把 traceback 接入异常对象

### 在错误规范化时附加
在 `ZrCore_Exception_NormalizeStatus` / `ZrCore_Debug_RunError` 路径中，于异常对象上
设置 `stack` 字段（字符串），值来自 `ZrCore_Debug_Traceback`。要点：
- **生成时机**：在栈尚未被 unwind 之前抓取（抛出点），否则帧已丢失——参照 QuickJS 在
  构造 Error 时立即 build_backtrace。
- 若异常由用户 `throw` 抛出，也在抛出点捕获栈快照。
- 避免在 OOM/字符串创建失败路径再分配大 buffer：使用栈上固定 buffer（如 4KB），
  失败则降级为仅 message。

## 3.4 CLI 未捕获错误输出

`runtime.c` 未捕获错误边界：打印 `message` + 换行 + traceback。格式示例：
```
runtime error: index out of range
  at compute (main.zr:42:7)
  at main (main.zr:10:3)
```
受环境变量控制详略：已有 `ZR_VM_TRACE_RUN_ERROR`（`debug.c:205`）可复用为「附加内部诊断」。

## 3.5 验收

- `tests/debug/test_debug_traceback.c`：
  - 三层调用中抛错，断言 traceback 含三帧且行号正确。
  - native→script→native 混合帧，断言 native 帧标 `[native]`。
  - 深栈（> 阈值）触发折叠，断言出现 `skipping` 行且首尾帧保留。
  - 截断：极小 buffer 下断言不溢出、NUL 结尾、无崩溃。
- `tests/cli/test_cli_debug_e2e.c` 扩展：未捕获错误 stdout 含 traceback 文本。
- DAP：异常停止事件的详情包含 `stack`（供 5 阶段 UI 使用）。

## 3.6 风险

- **抓取时机**是最大陷阱：unwind 后再生成会得到残缺栈。必须在抛出点（最深帧仍在）生成。
- 重入：traceback 生成本身若触发 GC/分配进而再次出错，需保证不递归进 RunError。用栈 buffer +
  仅读不分配的渲染路径规避。
