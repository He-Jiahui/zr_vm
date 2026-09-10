---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_instruction.c
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/execution_control.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/function.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_out_definite_assignment_flow.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的异常、清理与 C API 说明
  - docs/core-runtime/index.md
tests:
  - tests/exceptions/test_exceptions.c
  - tests/parser/test_cfg_try_catch_edges.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_cfg_typed_catch_flow.c
  - tests/fixtures/reference/core_semantics/exceptions_gc_native_stress/manifest.json
doc_type: language-reference
---

# 异常展开、`finally` 与清理运行时参考

本页解释一条异常从 ZR 源码到 VM/AOT 执行器的实际通路。它面向需要判断 `try/catch/finally`、
`using`、`return`、native callback 和 C 宿主边界的人；基础错误类型与用户侧例子见
[异常、诊断与失败传播](error-handling.md)。这里的关键点是：VM 不把 `finally` 实现为一段
简单的文本后插，而是把控制转移暂存为 `pendingControl`，再由 handler 栈和 cleanup 逻辑恢复。

## 源码语法与 AST 边界

```ebnf
throw-statement = "throw", expression, ";" ;
try-statement   = "try", block,
                  {"catch", "(", parameter-list, ")", block},
                  ["finally", block] ;
```

当前 parser 对 `catch` 使用普通 `parameter-list` production，因此括号是必需的，且 AST 保存的
是参数数组而不是预先简化的字符串。实际可执行的 typed catch 仍需由 semantic/compiler 降成
可表示的异常类型匹配；不要因为 parser 接收一个复杂参数形状，就假定所有模式在 runtime 都有
catch dispatch 语义。

```zr
let fs = import("zr.system.fs");
let exception = import("zr.system.exception");

fn loadText(path: string): string {
    try {
        return fs.readText(path);
    } catch (error: exception.IOException) {
        throw error;
    } finally {
        // 只放置必须在所有离开路径执行的短小清理动作。
    }
}
```

parser 会构建以下节点：

| 源码 | AST 节点 | 主要字段 |
| --- | --- | --- |
| `throw value;` | `ZR_AST_THROW_STATEMENT` | `throwStatement.expr` |
| `try { ... }` | `ZR_AST_TRY_CATCH_FINALLY_STATEMENT` | `block`、`catchClauses`、`finallyBlock` |
| `catch (...) { ... }` | `ZR_AST_CATCH_CLAUSE` | `pattern`、`block` |

缺失 `try`/`catch`/`finally` 的 `{` 或 catch 的 `)` 是 parser 的立即恢复边界。catch 子句可以
重复；运行时按 compiler 输出的 `SZrFunctionCatchClauseInfo` 顺序尝试匹配，不能把 catch 当作
无序的 type-set。

## 从 AST 到函数异常元数据

compiler 在 function 中写入两类异常元数据：

```c
typedef struct SZrFunctionCatchClauseInfo {
    SZrString *typeName;
    TZrMemoryOffset targetInstructionOffset;
} SZrFunctionCatchClauseInfo;

typedef struct SZrFunctionExceptionHandlerInfo {
    TZrMemoryOffset protectedStartInstructionOffset;
    TZrMemoryOffset finallyTargetInstructionOffset;
    TZrMemoryOffset afterFinallyInstructionOffset;
    TZrUInt32 catchClauseStartIndex;
    TZrUInt32 catchClauseCount;
    TZrBool hasFinally;
} SZrFunctionExceptionHandlerInfo;
```

它们由 `compiler_instruction.c` 复制到 `SZrFunction`，因此执行器不需要重新遍历 AST。catch
条目携带的是用于 prototype/type-name 匹配的类型与目标指令 offset；handler 条目描述受保护
区间、finally 入口、finally 之后的位置以及该 handler 覆盖的 catch 范围。写 artifact、做
bytecode instrumentation 或生成 AOT 时，必须同时保持这两张表、instruction offset 和 source
location 表一致。

## VM 状态：异常不是普通返回值

`SZrState` 维护当前异常和 handler 栈。相关状态不是用户代码可任意改写的公开协议，但它解释了
为什么 native callback、GC 和 stack relocation 要遵守严格顺序：

| 状态 | 作用 |
| --- | --- |
| `currentException` / `currentExceptionStatus` / `hasCurrentException` | 当前被 materialize 的 Error 值和 thread status。 |
| `exceptionHandlerStack` | 每个活动 try 的 `SZrVmExceptionHandlerState`。 |
| `pendingControl` | 被 finally 暂存的异常、return、break 或 continue。 |
| handler `phase` | `TRY`、`CATCH` 或 `FINALLY`，防止 finally 自己重新进入同一 finally。 |

当前 `EZrVmPendingControlKind` 明确列出 `NONE`、`EXCEPTION`、`RETURN`、`BREAK` 和
`CONTINUE`。这意味着这张表可以证明上述控制转移穿过 finally；它本身**不能**证明任意
`yield` 或异步挂起都有同一个 pending-control 路径。涉及 iterator/task 的 finally 语义，应
额外以对应 backend 和 task 测试为证据。

## 展开算法

发生 throw 或 runtime status error 后，执行器的逻辑可概括为：

```text
materialize/normalize current exception
  -> 从当前 call-info 向外找最内层活动 handler
  -> handler 处于 TRY：按 source order 匹配 catch type
       -> 命中：关闭该 handler 的资源登记，跳到 catch target
       -> 未命中且有 finally：把 EXCEPTION 放入 pendingControl，进入 finally
       -> 未命中且无 finally：弹出 handler，继续向外
  -> handler 处于 CATCH：若再次 throw，必要时走 finally 或继续向外
  -> handler 处于 FINALLY：关闭该范围并避免重入，继续向外
  -> 没有 handler：drop inline frame 值、关闭 closure、回退到 previous call-info
```

真实实现位于 `execution_unwind_exception_to_handler`。其中 `ZrCore_Exception_CatchMatchesTypeName`
通过 error object 的 prototype 链检查类型，不是字符串前缀匹配。第一个匹配的 catch 获得控制权；
catch body 再次抛出时，旧 catch 不会第二次处理同一个新异常。

### `finally` 如何保存控制转移

当执行器进入 finally，`execution_enter_finally` 会将可见 `state->pendingControl` 复制到 handler
的 `suspendedControl`；若它代表异常，还会保存 `suspendedException` 和 status。随后清空可见
pending slot，标记 handler 为 `FINALLY`，执行 finally body。`execution_finish_finally` 标记
需要恢复，`execution_pop_exception_handler` 再把暂存控制转移放回 state。

这种设计有两个直接后果：

1. finally 内 `return`、`throw` 或资源 Drop 可以产生新的控制转移；它不是一个“不可失败”的
   延后回调。
2. handler 内的 suspended exception/pending value 被当作 GC root 保护；如果手写 AOT helper 或
   native 代码绕过 helper 修改 state，GC 后可能留下悬空 object 指针。

`execution_set_pending_control` 与 `execution_clear_pending_control` 在替换/释放 pending value
时使用受保护的 root frame 和 `ZrCore_Exception_TryRun`。这样 owner release 或 resource Drop
重入时，旧值、环境中的异常和新异常仍能被明确处理，而不是依赖栈上偶然存活的裸指针。

## `using`、Drop 与异常的顺序

`using` 建立的是资源 cleanup registration；异常 handler 创建时还会记录
`toBeClosedBoundaryOffset`。在 catch 跳转、finally 入口、handler 弹出以及 call frame unwind
时，执行器关闭在该边界内登记的资源。推荐把资源所有权写成结构化作用域：

```zr
let fs = import("zr.system.fs");
let file = init fs.File("audit.log");

using (let stream = file.open("a")) {
    try {
        stream.writeText("begin\n");
        // 可能抛 IOException 的工作
    } finally {
        stream.flush();
    }
}
```

`finally` 与 using close 都可能调用用户/provider 代码，因此不应在 cleanup 内保存 borrowed
inline address、无 root 的 managed object，或待写回的 `out` 参数地址。需要跨调用持有临时值的
native callback 应使用 [Native Call Context](../05-interop/native-call-context-reference.md) 中的
`ZrLibTempValueRoot`；需要理解 frame relocation 时参阅 [VM 栈与执行参考](../09-runtime-stack-execution-reference.md)。

## 核心 C 异常 API

`exception.h` 提供的是 runtime/core 级入口。普通 native module 回调优先使用
`ZrLib_CallContext_RaiseTypeError`、`RaiseArityError` 等 library helper；只有受控宿主或 core
实现才应直接编排 long-jump recovery 边界。

| API | 作用 | 调用约束 |
| --- | --- | --- |
| `ZrCore_Exception_TryRun` | 安装临时 native recovery point 并返回 `EZrThreadStatus` | callback 只通过 state 完成工作；不要从 callback 跳到自己的未登记栈帧。 |
| `ZrCore_Exception_Throw` | 向当前 recovery point 抛 thread status | core 路径；无可恢复点时会走 panic/abort 逻辑。 |
| `ZrCore_Exception_NormalizeThrownValue` | 把 payload materialize 为当前 Error | 传入有效 state、payload 与 throw call-info。 |
| `ZrCore_Exception_NormalizeStatus` | 将 status error 转换为 Error（若尚无 current exception） | 对终止状态不生成普通 Error。 |
| `ZrCore_Exception_RaiseNamedRuntimeError` | 按 prototype 名称创建 runtime Error | prototype 必须已经注册在当前 state。 |
| `ZrCore_Exception_ClearCurrent` | 清除 current exception 状态 | 仅在异常已处理/被显式消费后调用。 |
| `ZrCore_Exception_PrintUnhandled` / `LogUnhandled` | 格式化未处理异常 | error value 和 stream/state 必须在调用期间有效。 |

一个受控嵌入边界的最小模式如下。代码只演示 recovery ownership；`run_callback` 自身必须遵守
当前 host 的 state/GC 生命周期：

```c
static void run_callback(SZrState *state, TZrPtr argument) {
    FHostRun *run = (FHostRun *)argument;
    run->ok = run->invoke(state, run->userData);
}

EZrThreadStatus run_protected(SZrState *state, FHostRun *run) {
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, run_callback, run);
    if (status != ZR_THREAD_STATUS_FINE && state->hasCurrentException) {
        ZrCore_Exception_LogUnhandled(state, &state->currentException);
    }
    return status;
}
```

不要在 `run_callback` 中缓存 `state->exceptionRecoverPoint`，不要把 `currentException` 的 raw
object 地址留到 `TryRun` 返回之后，也不要把 `ZrCore_Exception_Throw` 当作普通错误返回。若要
把异常信息跨线程/跨状态交付，提取 host-owned diagnostic/message snapshot，而不是复制 VM object
地址。

## AOT 入口和恢复点

AOT generated code 不应手动写 state fields。`aot_runtime.h` 提供对应的 helper：

| Helper 组 | 用途 |
| --- | --- |
| `ZrLibrary_AotRuntime_Try` / `EndTry` | 建立、结束函数 handler slot。 |
| `ZrLibrary_AotRuntime_Throw` | 从 generated frame source slot 抛出并报告 resume instruction。 |
| `ZrLibrary_AotRuntime_Catch` | 将匹配到的 current exception 物化到 catch destination。 |
| `ZrLibrary_AotRuntime_EndFinally` | 结束 finally，并给出待恢复的 instruction。 |
| `SetPendingReturn` / `SetPendingBreak` / `SetPendingContinue` | 在进入 finally 前保存非异常控制转移。 |
| `CloseScope` | 按 generated frame 的 cleanup scope 关闭资源。 |

这些 helper 负责 generated frame、root map、call-info 与 interpreter/AOT 交界的一致性。错误的
做法是先对 slot 取一个 raw managed pointer，再调用 `Throw` 或 `EndFinally`，最后继续使用旧
指针；任意 helper 都可能触发 cleanup、GC 或 frame 重定位。AOT 生成、root map 和注册细节见
[AOT Lowering、运行时 Helper 与注册](../10-aot-lowering-registration-reference.md)。

## 诊断与调试表

| 现象 | 首先检查 | 典型根因 |
| --- | --- | --- |
| catch 从未命中 | `SZrFunctionCatchClauseInfo.typeName` 与 prototype registration | type name 未解析、Error prototype 链不匹配。 |
| finally 执行两次 | handler phase 与 generated `EndFinally` 控制流 | 手写跳转绕过 helper，或在 finally 内错误重新进入同一 handler。 |
| cleanup 时崩溃 | pending/root 保护和资源 close 的重入 | 释放前保留 borrowed/raw object 指针。 |
| native callback false 后继续执行 | callback 的 raise/return 约定 | 设置异常后没有立即结束 callback。 |
| AOT 与解释器异常不同 | function exception metadata、target offsets、AOT helper 结果 | 只更新了 instruction body，没有同步 handler/catch 表。 |
| 错误被错误覆盖 | finally/Drop 再次失败的路径 | 没有记录原始异常，或宿主在 recovery 外清除了 current exception。 |

验证这类改动时至少覆盖：typed catch 顺序、嵌套 try/finally、return/break/continue 穿越 finally、
using close 时抛错、native callback 失败，以及解释器/AOT 的相同行为。不要仅以“异常消息打印了”
作为正确性证据，handler depth、pending state、root frame 和资源结束顺序同样是 contract。
