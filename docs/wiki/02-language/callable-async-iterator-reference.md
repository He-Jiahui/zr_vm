---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_function_syntax.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c
  - zr_vm_library/include/zr_vm_library/task_runtime.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_function_syntax.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_postfix_call.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/12-async-task-job-scheduler/m1-explicit-task-syntax-effect.md
  - docs/plans/syntax/12-async-task-job-scheduler/m2-task-frame-runtime.md
  - docs/plans/syntax/13-iterator-enumerator-yield/m2-yield-syntax-semir.md
tests:
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/task/test_task_runtime.c
  - tests/iterator/test_yield_syntax.c
  - tests/fixtures/projects/syntax_reference_v1/src/callables.zr
  - tests/fixtures/projects/syntax_reference_v1/src/async_jobs.zr
  - tests/fixtures/projects/syntax_reference_v1/src/iterators.zr
doc_type: language-reference
---

# 可调用对象、调用与异步迭代参考

ZR 把“声明一个函数”“创建一个 closure”“执行一次调用”“挂起等待 Task”和“产出 Iterator
元素”表示为不同的 AST 和不同的执行阶段。本页从源代码写法出发，说明参数、命名实参、
可变参数、generic call、lambda、`async`/`await` 和 `yield` 的边界，并给出它们通向 task
runtime、iterator frame 与 C 宿主 API 的路径。

类型语法中的 `fn(...) -> T` 见[类型表达式与参数契约参考](type-expression-reference.md)。
本页讨论的是**可调用值如何声明、创建和调用**，不是将函数类型文本当作函数体。

## 一览：四种相关但不同的形态

| 源码形态 | 例子 | parser 的主要 AST | 运行时/语义责任 |
| --- | --- | --- | --- |
| 命名函数声明 | `fn add(...): i32 { ... }` | `ZR_AST_FUNCTION_DECLARATION` | 符号、overload、frame、返回路径。 |
| 匿名函数/closure | `fn(x: i32): i32 => x + 1` | `ZR_AST_LAMBDA_EXPRESSION` | 捕获、callable object、escape/ownership。 |
| 普通或 optional 调用 | `f(1)`、`f?.(1)` | `ZR_AST_FUNCTION_CALL` | callee 绑定、参数求值、call contract、异常。 |
| async/iterator 控制点 | `await task`、`yield value;` | `ZR_AST_AWAIT_EXPRESSION`、`ZR_AST_YIELD_STATEMENT` | suspension frame、Task/Iterator 协议、GC root、cleanup。 |

即使语法都通过，它们仍在不同阶段失败。例如，`await value` 的 parser 只建立 unary AST；
semantic 会检查 `value` 是 `zr.task.Task<T>`、当前位置有 async effect、没有借用跨越挂起点，
task runtime 才会安排/恢复实际 job。

## 命名函数声明

```ebnf
function-declaration = { decorator }, [ access ], [ "async" ], "fn", identifier,
                       [ generic-declaration ], "(", parameters, ")",
                       [ ":", TypeRef ], [ where-clauses ], block ;
access               = "pub" | "pri" | "pro" ;
```

```zr
pub fn add(left: i32, right: i32): i32 {
    return left + right;
}

fn message(prefix: string, name: string = "world"): string {
    return prefix + name;
}
```

函数声明的 return annotation 使用冒号 `:`。`fn add(...) -> i32 {}` 是旧 function-definition
arrow 写法，parser 会提示将 `->` 留给 function type。缺少 return type 在语法层是可选的，但
能否推导、是否允许暴露为 public API、是否满足 async/FFI contract 是后续检查。

decorator 先于可见性和 `async`；parser 把它们、`fn` keyword location、return delimiter location、
参数、variadic 参数和 body 分别保留在 `SZrFunctionDeclaration`，以供诊断、LSP 和 artifact
writer 使用。

## 参数声明与传递方向

普通参数与一个尾随 variadic 参数的摘要：

```ebnf
parameters         = [ parameter, { ",", parameter }, [ ",", variadic ] ] ;
parameter          = [ "const" ], [ identifier, ":" ], [ source-form ], TypeRef,
                     [ "=", expression ] ;
variadic           = "...", [ "const" ], [ identifier, ":" ],
                     [ source-form ], TypeRef ;
source-form        = "in" | "out" | "ref" | "ref", "readonly"
                     | "scoped", "ref" | "scoped", "ref", "readonly" ;
```

```zr
fn scale(ref value: f64, factor: f64): void {
    value = value * factor;
}

fn parse(source: in string, out result: i32): bool {
    result = 42;
    return true;
}

fn log(prefix: string, ... values: string): void {
    // values is the declared variadic tail.
}
```

默认值只由非 variadic 参数解析。`const`、source form 和 TypeRef 都会被保留在
`SZrParameter`；不要把 `const` 当成调用处 `const` 字面量，也不要认为 `ref` 仅是类型修饰。
参数的词序、`readonly` 限制、`in/out` 与 reference type 的区别由[类型表达式与参数契约参考](type-expression-reference.md)
完整规定。

`async fn` 还有额外的 semantic contract：当前 task tests 要求显式、闭合的
`zr.task.Task<T>` return type，并拒绝 `in`、`ref`、`out` 参数。parser 仍会生成带
`isAsync` 的声明；这是“能解析”与“可被 Task compiler 接受”的典型差异。

## 匿名函数和捕获

匿名函数从 `fn` 开始，没有名称，允许 block body 或 `=>` expression body：

```ebnf
lambda = [ "async" ], "fn", "(", parameters, ")", [ ":", TypeRef ],
         ( block | "=>", ( expression | block ) ) ;
```

```zr
var increment = fn(value: int): int => value + 1;

var classify = fn(value: int): string {
    if (value > 0) { return "positive"; }
    return "not-positive";
};

var waitFor = async fn(value: zr.task.Task<int>): zr.task.Task<int> => await value;
```

`=>` expression body 被 parser 归一成带 `return` 的 block AST，因此 diagnostics 和 cleanup
路径仍可按普通函数体处理。`async fn` 的 lambda 不是把 `async` 当成普通 callable name：
primary-expression parser 识别 `async` 后紧随 `fn` 的形状，并在 `SZrLambdaExpression.isAsync`
设置标志。

closure 捕获的语义不是“所有外部变量自动深拷贝”。每个捕获会经过 name binding、Place/Value
分类、owner/loan escape 检查和 closure layout 计算。尤其禁止：

```zr
fn invalid(): fn() -> int {
    var owner = "value";
    var borrowed: ref readonly string = ref owner;
    return fn(): int => borrowed.length(); // borrowed 不能逃进返回的 closure
}
```

诊断具体取决于作用域和调用方式，但设计原则固定：loan 的有效期不能因为 closure object 被
分配就自动延长。跨 `await` 的同类问题也会被 Task effect 检查拒绝。

## 调用表达式与求值顺序

一个 primary/postfix chain 可以连续追加成员、generic call、直接 call 与 optional call：

```zr
let answer = service.client.fetch<int>(key, retries: 3).value;
let maybeAnswer = callback?.(key);
```

```ebnf
call-suffix       = [ "<", generic-arguments, ">" ], "(", arguments, ")"
                  | "?.", "(", arguments, ")" ;
arguments         = [ argument, { ",", argument }, [ "," ] ] ;
argument          = [ identifier, ":" ], [ "ref" | "out" ],
                    [ "..." ], expression ;
```

parser 建立 `ZR_AST_FUNCTION_CALL`，其中 `args`、`argNames`、`argumentMarkers` 和
`genericArguments` 是彼此对齐的 metadata。generic argument 可以是 TypeRef 或 const
expression；它只在 call suffix 的 `<...>(...)` 形状中生效，不能把任意 `<...>` 当作普通
比较运算或 call。

### 位置、命名、`ref/out` 与 spread

```zr
configure("fast", retries: 3, timeout: 10);
scale(ref vector, 0.5);
parse(text, out parsed);
log("events", ... names);
```

规则如下：

| 形式 | 语法阶段规则 | semantic/call binding 还要验证 |
| --- | --- | --- |
| 位置实参 | 正常 expression | 参数数量、可转换性、overload。 |
| 命名实参 `name: expr` | 第一个 named argument 后不能再跟位置参数 | 名称存在、无重复、默认值和顺序重排。 |
| `ref expr` | marker 记录到 `argumentMarkers` | `expr` 是可写 Place，callee contract 为 writable ref。 |
| `out expr` | marker 记录到 `argumentMarkers` | `expr` 可写，callee/正常返回路径满足 out contract。 |
| `... expr` | 生成 `ZR_AST_SPREAD_ARGUMENT` | `expr` 可展开、只用于可变参数/允许的 call shape。 |

调用参数按 source order 构建 AST。`&&`、`||`、`?:` 和 optional chain 的短路仍由语义/backend
保证；不要把 AST 的数组存储顺序误当成允许 native callee 重排副作用的授权。

optional call `callback?.(arg)` 会在 receiver 缺失时短路调用以及参数求值。`?.[` 不是可用的
可选索引写法；先保存 receiver 并显式判断，或使用普通可选成员/调用链。

## 从函数值到 call binding

一次调用的真实路径通常是：

```text
callee expression + source arguments
    -> AST function-call metadata
    -> name/overload/generic binding
    -> canonical parameter contracts and conversions
    -> CFG evaluation + cleanup edges
    -> ExecBC call / AOT direct-or-checked call / native binding
    -> return value, exception, or suspension result
```

下面几项必须在 binding 后才能确定：

- named argument 是否对应某一唯一候选；
- `ref` / `out` marker 是否与 callee 的 parameter form 匹配；
- generic 实参是否满足 constraint，const expression 是否可用；
- default 参数在哪个调用侧物化；
- 调用是 direct、virtual/protocol、closure、native provider 还是 optional short-circuit；
- receiver 是否只读、owner 是否被转移、借用在异常/return 后何时结束。

因此 native callback 不能只读取“第 N 个参数”并忽略 descriptor。应通过已绑定的
`ZrLibCallContext`/native binding helper 读取值，具体示例见[Native callback 配方](../05-interop/native-callback-recipes.md)。

## async 与 `await`

```zr
let task = import("zr.task");

async fn addOne(value: int): task.Task<int> {
    return value + 1;
}

async fn consume(value: task.Task<int>): task.Task<int> {
    let result = await value;
    return result;
}
```

`async` 是 contextual marker，`await` 是 unary expression。parser 在 `await` 后解析一个
unary operand 并建立 `ZR_AST_AWAIT_EXPRESSION`，所以 `await work()`、`await task` 等都是
同一语法类。它不通过 parser 立即调度任务。

Task semantic/runtime 必须依次确认：

1. `await` 位于 async body 或 scheduler 管理的顶层 coroutine；
2. operand 的 closed type 是 `zr.task.Task<T>`；
3. async callable 自己声明与 payload 一致的 closed `Task<T>` return；
4. 参数不是会跨 suspended frame 的 `in/ref/out` view；
5. 活动 borrow、scoped guard、未完成 out contract 和 thread-affine 资源不会跨 suspension；
6. task provider 已注册，scheduler 可以接管 work item。

例如下例的 parser 可以建立 AST，但 compiler 应拒绝它，因为 `value` 是借用且会在 `await`
之后继续使用：

```zr
async fn invalid(task: zr.task.Task<int>): zr.task.Task<int> {
    var owner = "ok";
    var value: ref readonly string = ref owner;
    await task;
    return value.length();
}
```

旧 `%await` / `%async` 语法已被移除。`await` 也不是普通 identifier 调用；不要写
`await(value)` 来表达等待。

## `yield`、Iterator 与恢复边界

```zr
let iteration = import("zr.iteration");

fn values(value: int): iteration.Iterator<int> {
    yield value;
    yield value + 1;
}
```

`yield` 是专用 statement，必须带 expression 和结尾分号：

```ebnf
yield-statement = "yield", expression, ";" ;
```

parser 产生 `ZR_AST_YIELD_STATEMENT`。它不接受空 `yield;`，也不需要、更不接受旧的
`iterator fn` modifier。后续 iterator lowering 将函数转换为可恢复 frame，保存 program
counter、跨暂停存活的局部、GC root map 和 cleanup 状态。再次驱动 iterator 时，运行时从
yield 后的安全点继续；正常结束、close、异常和取消走不同终态。

与 async 一样，yield 周围的 borrow 和资源不允许靠“frame 保存了局部”来逃避生命周期规则。
迭代器的公开协议、`moveNext`/current 等 provider API 见[`zr.iteration`](../03-modules/iteration.md)
及其[接口目录](../03-modules/iteration-api.md)。

## C 宿主与 task/iterator provider

源级 `async` / `yield` 不是直接导出 C 函数指针。宿主需要先在同一个 global 注册 provider，
再通过 Library task runtime 驱动已绑定的 callable：

| C 接口 | 角色 | 生命周期重点 |
| --- | --- | --- |
| `ZrVmTask_Register` | 注册 `zr.task` descriptor | descriptor 为静态借用对象；global 释放前保持注册有效。 |
| `ZrVmLibIteration_Register` | 注册 `zr.iteration` descriptor | 先于依赖它的模块/编译任务。 |
| `ZrLibrary_TaskRuntime_ScheduleJob` | 提交 callable/job | 只提交已满足 provider/contract 的工作。 |
| `ZrLibrary_TaskRuntime_PrepareJob` | 建立 work item | item 有明确 owner，完成、fault 或 release 都必须收尾。 |
| `ZrLibrary_TaskRuntime_ExecutePreparedJob` | 执行 prepared item | 遵守 state/thread domain 约束。 |
| `ZrLibrary_TaskRuntime_CompletePreparedJob` | 发布成功完成 | 用任务结果完成而不是自行篡改 frame。 |
| `ZrLibrary_TaskRuntime_FaultPreparedJob` | 发布 fault | 保留 exception/diagnostic，而不是吞掉失败。 |
| `ZrLibrary_TaskRuntime_ReleasePreparedJob` | 释放未完成/已完成 item | 恰好一次，避免泄漏 callable root。 |

`RegisterAwaitHook` 和 `AwaitProviderTask` 用于把 provider Task 的等待行为接到 runtime；
`IsTaskComplete` 只查询状态，不替代 completion/fault 的所有权处理。完整 C 回调的 root、
exception 和 cancellation 规则见[核心运行时宿主 API](../05-interop/core-runtime-host-api.md)与
[原生库调用契约](../05-interop/ffi-contract.md)。

## 排错表

| 现象 | 优先检查 | 常见修正 |
| --- | --- | --- |
| `fn ... -> T {}` 被拒绝 | 命名函数返回分隔符 | 改为 `fn ...: T {}`。 |
| `fn(...) => T` 被拒绝 | function type 与 lambda 混淆 | type 用 `->`；lambda body 用 `=>`。 |
| named 后出现位置实参 | argument list parser | 把位置实参放在第一个 named 参数之前。 |
| `ref` 调用失败 | Place 与 callee contract | 传可写变量/字段/元素，并匹配 `ref` parameter。 |
| `await` 不在 async body | Task effect 检查 | 标记 callable 为 `async` 并写 closed `Task<T>` return。 |
| `await` operand 不是 Task | canonical payload type | 使用 `zr.task.Task<T>`，不要等待裸 `T`。 |
| 借用跨 await/yield 失败 | loan/escape graph | 在挂起前复制/转移为合规 owner，或缩短 borrow。 |
| `yield;` 或 `iterator fn` 失败 | yield parser | `yield expression;`，用普通 `fn` 返回 Iterator。 |

对于二进制或 AOT 差异，不能仅凭解释器成功就断言调用 ABI 正确。应在相同 provider 注册、
artifact schema 和 native contract 下运行对应 task/iterator/FFI 测试，并查看[构建与验证](../04-tools/build-validation.md)。
