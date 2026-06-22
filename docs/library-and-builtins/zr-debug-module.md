---
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/module.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/module.c
  - zr_vm_lib_debug/CMakeLists.txt
  - tests/library/test_debug_library.c
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
implementation_files:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/module.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/module.c
  - zr_vm_lib_debug/CMakeLists.txt
plan_sources:
  - user: 2026-06-21 按 docs/plans/debug 优化 debug 调试能力
  - docs/plans/debug/04-script-debug-library.md
  - docs/plans/debug/index.md
tests:
  - tests/library/test_debug_library.c
  - tests/debug/test_debug_traceback.c
  - tests/debug/test_debug_hook_core.c
  - tests/debug/test_debug_introspection.c
doc_type: module-detail
---

# zr Debug Module

## Purpose

`debug` 是脚本层调试标准库，桥接 Phase 1-3 已落地的内核 hook、栈信息、自省变量和 traceback 能力。它对齐 Lua `ldblib.c` 的首批脚本 API，但默认不自动装载；宿主必须显式注册，才能让脚本通过 `%import("debug")` 取得模块。

这个模块面向测试、诊断、profiler 原型和断言增强，不应作为普通业务脚本的默认依赖。

## Public Surface

首批导出固定为：

- `traceback(msg: string = null, level: int = 1): string`
- `getinfo(levelOrFunction, what: string = "nSlu"): object`
- `getlocal(level: int, index: int): object`
- `setlocal(level: int, index: int, value): string`
- `getupvalue(func: function, index: int): object`
- `setupvalue(func: function, index: int, value): string`
- `upvalueid(func: function, index: int): nativePointer`
- `sethook(hook: function = null, mask: string|int = "", count: int = 0): null`
- `gethook(): object`

`getlocal` 和 `getupvalue` 现在返回 `{ name, value }` 对象，而不是 Lua 的两个返回值。这是因为当前 native binding 调用面只稳定承载单个返回值；保持对象形状比模拟多返回值更明确，也便于测试断言。

## Registration Model

`zr_vm_lib_debug/include/zr_vm_lib_debug/module.h` 暴露两组描述符和注册入口：

- `ZrVmLibDebug_Register(global)` 注册受信描述符，允许所有读写 API。
- `ZrVmLibDebug_RegisterSandboxed(global)` 注册同名 `debug` 模块的只读描述符。
- `ZrVmLibDebug_GetModuleDescriptor()` / `ZrVmLibDebug_GetSandboxedModuleDescriptor()` 供宿主或测试直接取描述符。
- shared library 模式下，`ZrVm_GetNativeModule_v1()` 返回受信描述符，保持 native module plugin ABI 兼容。

受信和沙箱描述符使用同一组函数表，运行时通过 `ZrLibCallContext::moduleDescriptor` 区分当前调用来自哪一种注册。模块 materialize 时还会写入 `__writeEnabled` 和 `__hook` 字段，前者用于调试观察，后者用于把脚本 hook 函数保留在模块对象上，避免 hook record 只保存裸值导致回收风险。

## Runtime Behavior

`debug.traceback` 调用 `ZrCore_Debug_Traceback`，默认脚本 level `1` 映射到内核 level `0`，因此脚本作者看到的是调用 `debug.traceback` 的当前帧。

`debug.getinfo` 支持两种目标：

- 数字层级：通过 `ZrCore_Debug_GetStack` 取得 activation，再用 `ZrCore_Debug_GetInfo` 填充字段。
- 函数或 closure：直接从函数元数据构造静态信息对象。

`what` 字符串按 Lua 约定解析：`n` 写入 `name/namewhat`，`S` 写入 source 和定义行，`l` 写入当前行，`u` 写入参数和 upvalue 数量，`t` 写入 tail-call 标记，`r` 写入 transfer 信息，`f` 写入函数值。

局部变量 API 通过 `ZrCore_Debug_GetLocal` / `ZrCore_Debug_SetLocal` 访问活动栈帧。upvalue API 通过 `ZrCore_Debug_GetUpvalue` / `ZrCore_Debug_SetUpvalue` / `ZrCore_Debug_GetUpvalueId` 访问 closure cell。

## Hook Bridge

`debug.sethook` 把脚本函数保存在 per-state hook record 中，再安装一个 C trampoline 到 `ZrCore_Debug_SetHook`。trampoline 把内核事件转换成脚本参数：

- `event`: `"call"`、`"return"`、`"line"`、`"count"` 或 `"unknown"`
- `line`: 当前源行

mask 字符串支持 `c/r/l`，也接受整数 mask。`count > 0` 时会额外启用 COUNT mask。`debug.gethook()` 返回 `{ hook, mask, count }`，无 hook 时返回 `{ hook: null, mask: "", count: 0 }`。

内核 `allowDebugHook` 仍负责 hook 重入保护；脚本 hook 被调用期间不会无限递归触发新的 hook。

## Safety

写能力只在受信描述符中开启。以下 API 在 sandboxed 描述符下直接抛出 `"debug write API is disabled"`：

- `setlocal`
- `setupvalue`
- `sethook`

读 API 仍可在 sandboxed 模式中保留，便于宿主允许只读诊断。模块本身也不自动加载到默认沙箱，测试覆盖了“未显式注册时 `%import("debug")` 不存在”的行为。

## Test Coverage

`tests/library/test_debug_library.c` 覆盖 8 条 Phase 4 验收路径：

- 未注册时 `debug` 不可见。
- 显式注册后首批 9 个 API 全部导出。
- `traceback` 包含 marker、调用链函数名和源文件。
- `getinfo(1, "nSlu")` 返回 name/source/line/parameter 字段。
- `getlocal` / `setlocal` 能读取并修改活动局部变量。
- `getupvalue` / `setupvalue` / `upvalueid` 能读取、改写并识别 closure cell。
- `sethook` 触发脚本 hook，`gethook` 返回 hook/mask/count。
- sandboxed 注册拒绝写 API。

底层能力仍由 `tests/debug/test_debug_hook_core.c`、`tests/debug/test_debug_introspection.c` 和 `tests/debug/test_debug_traceback.c` 兜底。

## Follow-Up

Phase 4 只交付脚本层 debug 标准库首批 API。多 task 调试、DAP step 边界、data breakpoint 和后续 profiling/coverage/反汇编属于 Phase 5/6。
