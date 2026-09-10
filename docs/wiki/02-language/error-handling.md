---
related_code:
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/execution_control.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_lib_system/include/zr_vm_lib_system/exception_registry.h
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/execution_control.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_lib_system/src/zr_vm_lib_system/exception_registry.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/index.md
tests:
  - tests/core/test_exception.c
  - tests/system/test_system_exception_module.c
  - tests/ffi/test_native_extern_contract.c
  - tests/fixtures/projects/syntax_reference_v1/negative
doc_type: language-reference
---

# 异常、诊断与失败传播

ZR 把 parser diagnostic、compile diagnostic 和 runtime exception 分成三条通道。它们都带
结构化状态，但生命周期不同：parser message 只在 parser state/revision 有效，runtime
exception 属于当前 state，provider/registry error 由对应 global 保存。宿主不能把某一路径的
`NULL`、负数或空字符串当成另一条路径的错误码。

## 源码语法

```ebnf
try-statement   = "try", block,
                  {"catch", "(", catch-parameters, ")", block},
                  ["finally", block] ;
catch-parameters = [parameter, {",", parameter}] ;
throw-statement = "throw", expr, ";" ;
```

`catch` 可以捕获一个带类型约束的错误参数；空参数 catch 只在 parser 接受且 semantic phase
允许的场景用于兜底。多个 catch 按声明顺序建立候选，具体匹配依据 canonical Error 类型。
`finally` 总会运行，包括 `return`、`throw`、`break`、`continue`、`yield` 和异常传播。

```zr
fn readConfig(path: string): string {
    try {
        return import("zr.system.fs").readText(path);
    } catch (error: import("zr.system.exception").IOException) {
        throw error;
    } finally {
        // 临时 guard 在这里释放
    }
}
```

`throw` 的表达式必须可归一化为 Error 或 provider 声明的异常类型。普通 `string` 会在
core exception normalizer 中包装成 runtime error（若调用上下文允许），但公共库应直接创建
具体 Error，以便调用方按类型捕获。

## 标准异常层级

`zr.system.exception` 提供 `Error`、`StackFrame`、`RuntimeError`、`NullReferenceError`、
`IOException`、`TypeError`、`MemoryError` 和 `ExceptionError`。Error 至少包含 `message`、
`stacks` 和原始 `exception` 关联；stack frame 在抛出点捕获，不保证跨 module reload 后仍
能解析到同一 function pointer。

| 异常 | 常见来源 | 调用者动作 |
| --- | --- | --- |
| `TypeError` | native 参数类型、property/index 类型不符 | 修正类型/检查 descriptor |
| `IOException` | 文件、socket、资源读写失败 | 捕获后关闭相关 handle |
| `NullReferenceError` | 对 null 解引用 | 增加 guard 或 optional access |
| `MemoryError` | GC/分配/预算失败 | 释放临时 root，降低批量或终止 state |
| `RuntimeError` | 任务、VM、未分类 runtime 错误 | 记录 stack 并按边界恢复 |
| `ExceptionError` | 已包装的宿主/用户异常 | 读取 `exception` 原值 |

## cleanup 与 pending control

编译器把控制转移编码为 pending-control 状态，再沿 cleanup edge 展开：

```text
body -> set pending control -> close innermost using/loan
     -> run finally -> close outer resources
     -> dispatch return/throw/break/continue/yield
```

如果 cleanup 自身抛异常，原始 pending control 会被保存在 exception cause/context 中；
实现不得静默覆盖最初错误。资源 close 设计为幂等，允许 finalizer 在显式 close 后再次检查。

## native callback 错误

`FZrLibBoundCallback` 返回 `TZrBool`，但失败语义不是“返回 false 给脚本”。callback 应使用
`ZrLib_CallContext_RaiseTypeError`/`RaiseArityError` 或 core exception helper 设置当前 state
的异常，再返回 false。调用 bridge 会恢复保存的 call-info、stack top、handler depth 和
pending control；native callback 不得 longjmp 离开 VM，也不得保存 context 指针。

```c
TZrBool my_native(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrInt64 value = 0;
    if (!ZrLib_CallContext_CheckArity(context, 1u, 1u)) {
        return ZR_FALSE;
    }
    if (!ZrLib_CallContext_ReadInt(context, 0u, &value)) {
        ZrLib_CallContext_RaiseTypeError(context, 0u, "int");
    }
    ZrLib_Value_SetInt(context->state, result, value + 1);
    return ZR_TRUE;
}
```

## parser 和 compiler diagnostic

parser diagnostic 通常包含：

| 字段 | 说明 |
| --- | --- |
| `code` | 稳定机器可读错误码 |
| `severity` | note、warning、error、fatal |
| `location` | source id、byte/line/column range |
| `message` | 当前语言环境下的文本 |
| `fixes` | 可选替换/插入建议 |

恢复后的 AST 节点可能带 invalid/recovered 标志。宿主应在 `hasError` 或
`hasFatalError` 为真时停止 compiler；不能因为 AST 非空就继续生成 artifact。跨线程/跨
generation 保存诊断时调用复制 API，把 range 和 message 复制到宿主-owned storage。

## unhandled handler

```zr
let exception = import("zr.system.exception");
exception.registerUnhandledException((error: exception.Error) => {
    import("zr.system.console").printErrorLine(error.message);
    return false; // false: 继续默认终止策略
});
```

handler 运行在异常尚未最终终止的边界，只能读取稳定 snapshot；不要在 handler 中再次触发
可能抛异常的复杂调用或释放当前 frame 持有的值。返回约定由 provider descriptor 固定，
应以当前版本的 `registerUnhandledException` 文档为准。

## 测试中的异常

`zr.testing.throws<E>(action: fn() -> void): E` 只接受同步 action，不隐式 await Task。成功
返回捕获的 E；未抛出、抛出错误类型或 action 自身执行失败都会生成结构化
`AssertionFailure`。failure 保存 assertion kind、source span、bounded expected/actual/
exception snapshot 和 truncation 标志。

## C 侧检查顺序

嵌入宿主建议遵循：

1. 检查每个 API 的 bool/status 返回值；
2. 失败时读取当前 state/global 的 exception 或 last-error message；
3. 先释放临时 root、call frame 和 native handle，再决定重试/终止；
4. 关闭 state 前清空未处理异常和 debugger/test callbacks。

不要把 `ZrLib_CallContext_Raise*` 之后的代码当作可达路径；这些声明为 `ZR_NO_RETURN`。
registry 的 ABI/version/phase 错误详见 [Native Registry](../03-modules/native-api.md)，
parser 错误恢复详见[诊断与错误恢复](diagnostics.md)。
