---
doc_type: plan-phase
phase: 2
title: 变量与栈帧自省 API
related_code:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_lib_debug/src/debug_snapshot.c
  - lua/src/ldebug.c
  - lua/src/lapi.c
  - lua/src/lfunc.c
---

# Phase 2 — 变量与栈帧自省 API

把「按层级/索引/名字读写局部变量与 upvalue」做成内核一等公民 API，让 DAP 代理与
脚本层 debug 库共用同一份语义（消除 `debug_snapshot.c` 内联实现的漂移风险）。

## 2.1 局部变量 getlocal/setlocal

### 数据基础（已存在）
- `SZrFunctionLocalVariable`（`function.h:55`）：`name`、`stackSlot`、`offsetActivate`、
  `offsetDead`、`scopeDepth`、`escapeFlags`——含 PC 活跃区间，等价 Lua `LocVar.startpc/endpc`。
- `ZrCore_Function_GetLocalVariableName(function, index, programCounter)`（`function.h:600`）
  已能按 PC 过滤活跃变量并返回第 n 个的名字。

### 新增 API（`debug.h`）
```c
// 返回第 localIndex 个在当前 PC 活跃的局部变量名；并通过 outValue 取其值指针。
// localIndex 从 1 起（与 Lua 一致）。返回 NULL 表示越界。
ZR_CORE_API TZrNativeString ZrCore_Debug_GetLocal(struct SZrState *state,
                                                  const SZrDebugActivation *activation,
                                                  TZrInt32 localIndex,
                                                  struct SZrTypeValue *outValue);

// 写回第 localIndex 个活跃局部变量；返回其名字或 NULL（越界）。
ZR_CORE_API TZrNativeString ZrCore_Debug_SetLocal(struct SZrState *state,
                                                  const SZrDebugActivation *activation,
                                                  TZrInt32 localIndex,
                                                  const struct SZrTypeValue *value);
```

### 实现要点（对照 `ldebug.c:223 lua_getlocal` / `lfunc.c:283 luaF_getlocalname`）
1. 从 `activation` 取 `SZrCallInfo*` 与当前 PC offset。
2. 调 `ZrCore_Function_GetLocalVariableName` 解析名字与对应 `stackSlot`。
3. 由 `callInfo->functionBase + stackSlot` 定位栈槽，读/写 `SZrTypeValue`。
4. **越界与 native 帧保护**：native callInfo 无栈布局，直接返回 NULL。
5. **inline struct 槽**（`function.h:68 EZrFunctionFrameSlotKind`）需特殊处理：若槽是
   `INLINE_STRUCT`，getlocal 返回的是结构体内联视图而非单值——Phase 2 先返回类型摘要，
   写入暂不支持（在 setlocal 中对 inline-struct 槽返回失败，留 TODO 给后续）。

### 负索引扩展（可选，借鉴 Lua 5.4）
Lua 用负 index 访问「可变参数」与临时值。zr 的可变参数布局见
`function.c:294 hasVariableArguments` 分支。本阶段**先只做正索引命名局部变量**，负索引
留作 Phase 5 DAP 增强里的「raw stack 视图」。

## 2.2 upvalue（闭包变量）getupvalue/setupvalue

### 数据基础
- `function.h:456 closureValueLength`、`function.h:470 closureValueList`
  （`SZrFunctionClosureVariable`）。
- 运行时闭包对象见 `closure.h`（`ZrCore_Closure_*`）。

### 新增 API
```c
// closure：从 activation 取到的当前函数闭包；upvalueIndex 从 1 起。
ZR_CORE_API TZrNativeString ZrCore_Debug_GetUpvalue(struct SZrState *state,
                                                    struct SZrClosure *closure,
                                                    TZrInt32 upvalueIndex,
                                                    struct SZrTypeValue *outValue);
ZR_CORE_API TZrNativeString ZrCore_Debug_SetUpvalue(struct SZrState *state,
                                                    struct SZrClosure *closure,
                                                    TZrInt32 upvalueIndex,
                                                    const struct SZrTypeValue *value);
// upvalue 唯一标识（用于别名检测，对照 lua_upvalueid）
ZR_CORE_API TZrPtr ZrCore_Debug_GetUpvalueId(struct SZrState *state,
                                             struct SZrClosure *closure,
                                             TZrInt32 upvalueIndex);
```

### 实现要点（对照 `lapi.c:1380 lua_getupvalue`）
- 名字来自 `closureValueList[i].name`，值来自闭包运行时的 upvalue 槽（注意 open/closed
  upvalue 的区分——若 zr 闭包用「指针指向栈或独立 cell」的经典实现，需先解引用）。
- `GetUpvalueId` 返回 upvalue cell 的稳定地址，供 watch 别名检测。

## 2.3 补全 `SZrDebugInfo`（消除 `// todo`）

### 新增 namewhat 分类
对照 Lua getinfo 的 `namewhat`（global/local/field/method/upvalue/""）。新增字段：
```c
// debug.h, struct SZrDebugInfo 内
EZrDebugNameWhat nameWhat;   // 函数名的来源分类
```
```c
typedef enum EZrDebugNameWhat {
    ZR_DEBUG_NAMEWHAT_UNKNOWN = 0,  // 无法判定（""）
    ZR_DEBUG_NAMEWHAT_GLOBAL,
    ZR_DEBUG_NAMEWHAT_LOCAL,
    ZR_DEBUG_NAMEWHAT_FIELD,
    ZR_DEBUG_NAMEWHAT_METHOD,
    ZR_DEBUG_NAMEWHAT_UPVALUE
} EZrDebugNameWhat;
```

### 推断来源（对照 Lua `funcnamefromcode` `ldebug.c`）
通过**调用者**的当前指令反查被调函数是如何被引用的：
- 调用指令的操作数指向全局表 → GLOBAL；
- 指向局部槽 → LOCAL；
- 指向 GETFIELD/方法调用指令 → FIELD/METHOD。

这需要读 zr 字节码的 call 指令族（见 `execution_dispatch.c` 的 CALL/方法调用 opcode）。
**实现成本中等**；若 Phase 2 时间紧，可先填 UNKNOWN，把推断作为 Phase 2 的可选尾项，
但字段与枚举先落地（DAP 与脚本库依赖该字段存在）。

## 2.4 DAP 代理改造为复用内核 API

`debug_snapshot.c`（2565 行，最大文件）当前自行遍历栈/变量。改造方向：
- 栈遍历改用 `ZrCore_Debug_GetStack(level)`；
- locals 作用域改用 `ZrCore_Debug_GetLocal`；
- closures 作用域改用 `ZrCore_Debug_GetUpvalue`；
- 保留 snapshot 层负责的「值预览渲染、分页、variablesReference 分配」——这些是 DAP 特有的，
  不下沉到内核。

收益：DAP 与未来脚本 `debug.getlocal` 行为一致；减少内核语义二次实现的 bug 面。

## 2.5 验收

- `tests/debug/test_debug_introspection.c`：
  - 多层嵌套函数，逐层 `GetStack`→`GetLocal`，断言名字/值与脚本预期一致。
  - `SetLocal` 修改后继续执行，断言可观测副作用（被改的变量影响后续输出）。
  - 闭包场景 `GetUpvalue`/`SetUpvalue`；`GetUpvalueId` 对共享 upvalue 的两个闭包返回相同 id。
  - PC 活跃区间边界：变量在其作用域外（offsetActivate 之前 / offsetDead 之后）不可见。
- DAP 回归：`test_debug_agent.c`、`test_debug_variable_child_shape.c` 在改用内核 API 后仍全绿。

## 2.6 风险

- **inline struct 槽**与值类型 lowering（参见近期 commit `1dfdccf`）使「一个局部=一个栈槽=一个值」
  的假设不再恒成立。getlocal 必须查 `EZrFunctionFrameSlotKind` 区分，否则会把结构体内部字节
  当成裸值读出。这是本阶段最大正确性风险，需专门测试值类型局部变量。
- open/closed upvalue 的解引用若处理错，setupvalue 可能写到已失效的栈位置。

## 状态与产出记录

| 时间 | 状态 | 完成项目 | 验证 | 后续/阻塞 |
|------|------|----------|------|-----------|
| 2026-06-20 07:59:23 +08:00 | 核心完成；MSVC 验收通过；WSL 新增目标构建待补 | 新增 `EZrDebugNameWhat` 与 `SZrDebugInfo.nameWhat`；实现 `ZrCore_Debug_GetLocal`/`SetLocal`/`GetUpvalue`/`SetUpvalue`/`GetUpvalueId`；`debug_snapshot.c` 的 closure/upvalue 解析改用内核 upvalue API；新增 `tests/debug/test_debug_introspection.c` 与 CTest；修复 Phase 2 验证暴露的 byte-frame value slot 与 dense stack overlap 所有权问题：open upvalue 按 `SZrTypeValue*` 解引用，`Stack_SetRawObjectValue` 覆盖前只释放真实可释放 ownership | Windows/MSVC：`zr_vm_cli --project ... -m fixed_data --execution-mode interp` 输出 `null` 且 exit 0；项目入口 `language_debug_gauntlet.zrp --execution-mode interp` 输出 `GAUNTLET_OK checksum=13910` 且 exit 0；`zr_vm_closure_capture_runtime_test` 1/0 PASS；`ctest -C Debug -R "^(debug_introspection|debug_variable_child_shape|debug_agent|debug_expression_diagnostics)$"` 4/4 PASS | WSL gcc/clang 旧 debug 回归中的 `debug_expression_diagnostics` 通过，gcc 的 `debug_variable_child_shape` 通过；新增 `debug_introspection` 可执行文件在旧 WSL 构建目录未产出，显式构建目标多次超时，且同机存在外部 `build/codex-wsl-gcc-debug` 构建占用。进入 Phase 3 前需补跑 WSL gcc/clang `debug_introspection` 或重建干净 WSL 目录 |
| 2026-06-20 10:52:13 +08:00 | 完成 | 保持 Phase 2 核心 API、DAP 复用、runtime ownership 修复与 focused 覆盖不变；补齐旧 WSL gcc/clang 构建目录中缺失的 `zr_vm_debug_introspection_test` 与 clang 侧 `zr_vm_debug_variable_child_shape_test` | WSL gcc：`ninja -C build/codex-debug-phase1-wsl-gcc -j2 zr_vm_debug_introspection_test` 通过，随后 `ctest --test-dir build/codex-debug-phase1-wsl-gcc -R "^(debug_introspection|debug_variable_child_shape|debug_expression_diagnostics)$" --output-on-failure` 3/3 PASS；WSL clang：`ninja -C build/codex-debug-phase1-wsl-clang -j2 zr_vm_debug_introspection_test zr_vm_debug_variable_child_shape_test` 通过，随后同范围 CTest 3/3 PASS | Phase 2 DoD 满足；进入 Phase 3 traceback/error diagnostics |
