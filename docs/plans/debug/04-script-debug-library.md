---
doc_type: plan-phase
phase: 4
title: 脚本层 debug 标准库
related_code:
  - zr_vm_lib_debug/
  - zr_vm_library/src/zr_vm_library/
  - zr_vm_core/include/zr_vm_core/debug.h
  - lua/src/ldblib.c
---

# Phase 4 — 脚本层 `debug` 标准库

把 Phase 1–3 的内核能力以受控方式开放给 zr 脚本，对标 Lua `ldblib.c`。这让脚本作者能写
自定义 profiler、断言增强、测试框架的栈检查等，而不必走 DAP。

## 4.1 模块定位

- 新增可注册的原生模块（沿用 zr 现有 native 模块注册机制，参见 `zr_vm_library` 下其它
  lib 的注册范式与 `native_registry.h`）。
- 模块名暂定 `debug`（与 Lua 对齐）。**默认不自动加载到沙箱**：debug 库能力强（可改局部
  变量、读任意栈），需由宿主显式开启（参照 Lua 把 debug 库视为「可选/受信」）。

## 4.2 API 表（首批）

| 脚本 API | 内核映射 | 说明 |
|----------|----------|------|
| `debug.traceback([msg[, level]])` | `ZrCore_Debug_Traceback`（P3） | 返回回溯字符串 |
| `debug.getinfo(level_or_func[, what])` | `GetStack`+`GetInfo`（P1/P2） | 返回信息表 |
| `debug.getlocal(level, idx)` | `ZrCore_Debug_GetLocal`（P2） | 返回 name,value |
| `debug.setlocal(level, idx, v)` | `ZrCore_Debug_SetLocal`（P2） | 返回 name |
| `debug.getupvalue(func, idx)` | `ZrCore_Debug_GetUpvalue`（P2） | 返回 name,value |
| `debug.setupvalue(func, idx, v)` | `ZrCore_Debug_SetUpvalue`（P2） | 返回 name |
| `debug.upvalueid(func, idx)` | `ZrCore_Debug_GetUpvalueId`（P2） | 别名检测 |
| `debug.sethook([hook[, mask[, count]])` | `ZrCore_Debug_SetHook`（P1） | mask 用字符串 "crl" 或位 |
| `debug.gethook()` | `GetHook*`（P1） | 返回当前 hook/mask/count |

第二批（依赖后续阶段，可延后）：`debug.getregistry`、`debug.sethook` 的 count 采样配合
Phase 6 profiling。

## 4.3 hook 回调桥接

`debug.sethook(zrFunction, "lr", 0)` 需把脚本函数包装成 `FZrDebugHook`：
- 内核 hook 触发时，C 侧 trampoline 把 `SZrDebugInfo` 转成脚本可读的事件参数
  （event 名 "line"/"call"/"return"/"count" + currentLine），压栈调用脚本 hook。
- **重入与 allowDebugHook**：调用脚本 hook 期间 `allowDebugHook=FALSE`（`debug.c:273` 已有），
  避免 hook 内再触发 hook 无限递归。
- 每个 state（线程/task）独立 hook —— 与内核 `state->debugHook` 的 per-state 语义一致。

## 4.4 安全与作用域

- `what` 选项字符串解析复用 Lua 约定：`"n"`=name/namewhat, `"S"`=source, `"l"`=line,
  `"u"`=nups/nparams, `"t"`=tailcall, `"f"`=push function。映射到 `EZrDebugInfoType` 位。
- 越界/类型错误：返回 nil 或抛 zr 异常（与 zr 其它 stdlib 的错误风格一致）。
- 写能力（setlocal/setupvalue/sethook）受模块开关保护，沙箱默认禁用。

## 4.5 验收

- `tests/library/test_debug_library.zr`（或对应 C 驱动）：
  - `debug.traceback` 在已知调用链返回预期文本。
  - `debug.getinfo(1,"nSl")` 字段正确。
  - `debug.getlocal/setlocal` 改值生效。
  - `debug.sethook` 行 hook 统计行数；count hook 统计指令数。
  - 沙箱模式下写 API 被拒。

## 4.6 风险

- 脚本 hook 性能：行/指令 hook 调脚本函数开销大，文档需标注「仅调试/测试用」。
- 把内核裸指针（callInfo/closure）泄漏给脚本是危险的——脚本侧只暴露**层级整数**与
  **函数值**，绝不暴露 activation 句柄本身。
