---
doc_type: plan-phase
phase: 1
title: 内核 hook 修复与基础 setter
related_code:
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/state.c
  - lua/src/ldebug.c
  - lua/src/ldo.c
---

# Phase 1 — 内核 hook 修复与基础 setter

本阶段是整个计划的地基：修复 COUNT hook 死代码、暴露 `SetHook` setter、纠正
`DebugInfo_Get` 的语义。**不引入新概念**，只把已存在但不可达/不正确的基础设施补完整。

## 1.1 修复 COUNT hook（bug）

### 问题
`ZrCore_Debug_TraceExecution`（`debug.c:119-188`）只检查 `ZR_DEBUG_HOOK_MASK_LINE`，
从不处理 `ZR_DEBUG_HOOK_MASK_COUNT`。`state.baseDebugHookCount`/`debugHookCount`、
`ZrStateResetDebugHookCount`（`state.c:63`）成为死代码。

### 参考
Lua `luaG_traceexec`（`ldebug.c:928`）：每条指令递减 `L->hookcount`，归零则触发
`LUA_HOOKCOUNT` 并 `resethookcount`。

### 改动（`debug.c` `ZrCore_Debug_TraceExecution`）
在已计算 `currentLine` 之后、设置 `trap` 之前插入 count 处理：

```c
if ((state->debugHookSignal & ZR_DEBUG_HOOK_MASK_COUNT) != 0) {
    // 每条被 trace 的指令递减；归零触发 COUNT 事件并重置
    if (state->debugHookCount > 0) {
        state->debugHookCount--;
    }
    if (state->debugHookCount == 0) {
        ZrStateResetDebugHookCount(state);   // = baseDebugHookCount
        if (state->baseDebugHookCount > 0) {
            ZrCore_Debug_Hook(state, ZR_DEBUG_HOOK_EVENT_COUNT,
                              ZR_RUNTIME_DEBUG_HOOK_LINE_NONE, 0, 0);
        }
    }
    trap = state->debugHookSignal;
}
```

要点：
- count 与 line 掩码相互独立，二者都置位时都要触发（Lua 同样如此）。
- `baseDebugHookCount==0` 视为「不启用 count」，避免 0 计数死循环。
- count 递减必须在 **每条被 dispatch 的指令** 上发生。确认
  `execution_dispatch.c:2009` 的 trap 宏在 count 掩码置位时也会进入 `TraceExecution`
  （目前 trap 来源于 `debugHookSignal`，COUNT 置位即触发，符合）。

### 性能
count hook 会让 VM 走慢路径（trap!=0）。这是预期成本，与 Lua 一致；fast-path 上
`debugHookSignal==0` 的判断已遍布 `function.c`/`execution_dispatch.c`，无需新增检查。

## 1.2 暴露 hook setter（`lua_sethook` 等价）

### 新增公开 API（`debug.h`）
```c
ZR_CORE_API void ZrCore_Debug_SetHook(struct SZrState *state,
                                      FZrDebugHook hook,
                                      TZrUInt32 mask,     // EZrDebugHookMask 位或
                                      TZrUInt32 count);   // COUNT 周期，0=禁用 count

ZR_CORE_API FZrDebugHook ZrCore_Debug_GetHook(struct SZrState *state);
ZR_CORE_API TZrUInt32    ZrCore_Debug_GetHookMask(struct SZrState *state);
ZR_CORE_API TZrUInt32    ZrCore_Debug_GetHookCount(struct SZrState *state);
```

### 实现（`debug.c`）
参照 `lua_sethook`（`ldebug.c:134`）：
```c
void ZrCore_Debug_SetHook(SZrState *state, FZrDebugHook hook, TZrUInt32 mask, TZrUInt32 count) {
    if (hook == ZR_NULL || mask == 0) { mask = 0; hook = ZR_NULL; }
    state->debugHook = hook;
    state->baseDebugHookCount = count;
    ZrStateResetDebugHookCount(state);     // debugHookCount = count
    state->debugHookSignal = (TZrDebugSignal)mask;
    state->allowDebugHook = ZR_TRUE;
    // 关键：让所有活跃栈帧的 trap 生效（Lua 的 settraps(L->ci)）
    // 见 1.3：需要把 mask 反映到当前 callInfo 链的 trap 上
}
```

### trap 传播（对应 Lua `settraps`）
启用 hook 时，**当前正在执行的 callInfo** 必须立即把 `context.context.trap`
置为非 0，否则要等到下一个自然 trap 点才生效。新增内部辅助：
```c
static void debug_settraps(SZrCallInfo *callInfo, TZrDebugSignal signal) {
    for (; callInfo != ZR_NULL; callInfo = callInfo->previous) {
        if (ZR_CALL_INFO_IS_VM(callInfo)) {
            callInfo->context.context.trap = signal;
        }
    }
}
```
在 `SetHook` 末尾调用 `debug_settraps(state->callInfoList, state->debugHookSignal)`。

> 注意：`SetHook` 与现有 `SetTraceObserver` 正交——前者驱动 `FZrDebugHook`，后者驱动
> 旁路观察者。二者可同时启用（`TraceExecution` 已分别处理）。DAP 代理继续用 observer，
> 新的内核/脚本 hook 用 `SetHook`，互不干扰。

## 1.3 修正 `ZrCore_DebugInfo_Get` 的 `type` 与层级语义

### 问题
`debug.c:58` 当前 `ZR_UNUSED_PARAMETER(type)`，且只取 `state->callInfoList`（栈顶）。

### 目标契约（贴近 Lua getstack+getinfo 拆分）
拆成两步，避免一个函数既选层级又选字段：

```c
// 取得第 level 层栈帧（0=当前，向外递增）；成功写入 outActivation
ZR_CORE_API TZrBool ZrCore_Debug_GetStack(struct SZrState *state, TZrUInt32 level,
                                          SZrDebugActivation *outActivation);

// 针对某个 activation，按 type 位掩码填充 debugInfo 字段
ZR_CORE_API TZrBool ZrCore_Debug_GetInfo(struct SZrState *state,
                                         const SZrDebugActivation *activation,
                                         EZrDebugInfoType type,
                                         SZrDebugInfo *outInfo);
```

其中 `SZrDebugActivation` 是轻量句柄（封装 `SZrCallInfo*` + 解出的 `SZrFunction*`），
避免把裸 `callInfo` 暴露给调用方。

`ZrCore_DebugInfo_Get` 保留为兼容包装：`GetStack(0)` + `GetInfo(ALL)`。

### `GetInfo` 按位填充
真正使用 `type`，只填请求的字段（对照 Lua getinfo 的 `S/l/n/u/t` 选项）：

| `EZrDebugInfoType` 位 | 填充字段 |
|---|---|
| `SOURCE_FILE` | `source`, `sourceLength`, `definedLineStart/End`, `isNative` |
| `LINE_NUMBER` | `currentLine` |
| `FUNCTION_NAME` | `name`, `scope`（+ Phase 2 的 namewhat） |
| `CLOSURE` | `closureValuesCount` |
| `TAIL_CALL` | `isTailCall` |
| `PUSH_FUNCTION` | 把函数对象压栈（对应 Lua getinfo 的 `f`） |

## 1.4 验收

- 新增单测 `tests/debug/test_debug_hook_core.c`：
  - 设 COUNT hook（count=N），跑一段已知指令数脚本，断言 hook 触发次数 = ⌊总指令/N⌋。
  - 设 LINE hook，断言每个源行只触发一次（去重逻辑 `debug.c:159`）。
  - 同时设 LINE|COUNT，断言两类事件都到达。
  - `SetHook(NULL,0,0)` 后断言不再触发，且 `debugHookSignal==0`、fast-path 恢复。
- `GetStack` 遍历多层调用，逐层断言 source/line/name 与已知脚本一致。
- 回归：现有 `tests/debug/*`、`tests/cli/test_cli_debug_e2e.c` 全绿（DAP observer 路径不受影响）。

## 1.5 风险

- **trap 传播遗漏**会导致 hook「下一行才生效」。务必在 `SetHook` 内做 `settraps`，并加
  「启用后第一条指令即触发」的测试。
- count 递减放错位置（如放进 LINE 去重分支）会漏计。必须在 LINE 处理之外、对每条
  trace 指令独立递减。

## 状态与产出记录

| 时间 | 状态 | 完成项目 | 证据 |
|------|------|----------|------|
| 2026-06-20 02:05:50 +08:00 | 核心完成；完整 Phase 1 验收受 CLI import 基线阻塞 | 接通 `ZR_DEBUG_HOOK_MASK_COUNT`；新增 `ZrCore_Debug_SetHook`/`GetHook`/`GetHookMask`/`GetHookCount`；新增 `SZrDebugActivation`、`ZrCore_Debug_GetStack`、`ZrCore_Debug_GetInfo`；`ZrCore_DebugInfo_Get` 改为 level 0 兼容包装并按 type mask 填充；LINE/COUNT trap 传播覆盖当前 VM callInfo 链；CALL/RETURN-only hook 不强制进入 instruction trap；新增 `tests/debug/test_debug_hook_core.c` 与 CTest `debug_hook_core`；修正 `tests/debug/test_debug_trace.c` 的 DebugInfo mask 预期；修复 Debug semantic-summary 中 assignment write/member-write fact 被 read/access fact 遮蔽的问题 | `zr_vm_debug_hook_core_test` 3/3 PASS；`zr_vm_debug_trace_test` PASS；`zr_vm_debug_expression_diagnostics_test` PASS；`ctest -R "debug_(metadata|hook_core|trace|agent|agent_protocol|expression_diagnostics|variable_child_shape)$"` 7/7 PASS。`cli_debug_e2e` 当前停在 `import_basic` 的 import signature mismatch，直接运行同一 project 也复现，记录在 `tests/acceptance/2026-06-20-debug-phase1-core-hooks.md` |
| 2026-06-20 02:22:57 +08:00 | 补充验证通过；完整 Phase 1 阻塞不变 | 使用 WSL clang 与 Windows/MSVC 补跑 Phase 1 focused debug set | WSL clang `debug_(hook_core|trace|expression_diagnostics)` 3/3 PASS；Windows/MSVC `debug_(hook_core|trace|expression_diagnostics)` 3/3 PASS |
| 2026-06-20 02:30:05 +08:00 | 阻塞归因补充 | 复核 `import_basic` 重新编译与 import canonicalization focused 基线 | `zr_vm_cli --compile tests/fixtures/projects/import_basic/import_basic.zrp --run` 仍复现同一 mismatch；`zr_vm_project_import_canonicalization_test` 35/0 PASS，说明阻塞集中在 `import_basic` exported-callable fixture 路径，非本轮 Debug hook/API 变更 |
| 2026-06-20 04:38:33 +08:00 | Phase 1 完成 | 新增 `tests/cli/test_cli_import_basic_fixture.c` 覆盖 `import_basic` source project 重编译与运行；修复 `pub var greet = () => ...` 这类 callable export 在 module-init summary 中停留为 prescan `VARIABLE`/`FIELD_SIG`，导致 consumer 期望签名与 provider `FUNCTION`/`METHOD_SIG` 不一致的问题；补充 Debug semantic-summary identifier read-by-kind fallback，保留 assignment write/member-write 同时恢复 computed index 的 `reference read index`；补齐重复 include 下 `SZrConstantReferencePath` 共享声明保护 | WSL gcc `cli_import_basic_fixture|cli_debug_e2e|debug_metadata|debug_hook_core|debug_trace|debug_agent|debug_agent_protocol|debug_expression_diagnostics|debug_variable_child_shape` 9/9 PASS；`zr_vm_project_import_canonicalization_test` 35/0 PASS；直接 `zr_vm_cli --compile .../import_basic.zrp --run` 输出 `compiled=2 skipped=0 removed=0` 和 `hello from import`；WSL clang `cli_import_basic_fixture|debug_expression_diagnostics` 2/2 PASS；Windows/MSVC 同两项 2/2 PASS |
