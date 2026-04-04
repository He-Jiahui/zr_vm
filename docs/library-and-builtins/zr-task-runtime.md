---
related_code:
  - zr_vm_task/include/zr_vm_task/module.h
  - zr_vm_task/include/zr_vm_task/runtime.h
  - zr_vm_task/src/zr_vm_task/module.c
  - zr_vm_task/src/zr_vm_task/runtime.c
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project.c
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli/project.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compile_time_executor_support.c
  - zr_vm_core/src/zr_vm_core/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
implementation_files:
  - zr_vm_task/src/zr_vm_task/module.c
  - zr_vm_task/src/zr_vm_task/runtime.c
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compile_time_executor_support.c
  - zr_vm_core/src/zr_vm_core/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
plan_sources:
  - user: 2026-04-05 实现 `zr_vm_task` 异步、协程与多线程运行时方案，并增加 `supportMultithread` / `autoCoroutine`
tests:
  - tests/task/test_task_runtime.c
  - tests/exceptions/test_exceptions.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_compiler_features.c
doc_type: module-detail
---

# zr.task Runtime

## Purpose

`zr.task` 是当前仓库里独立的并发内建模块。它把 async handle、协作式 scheduler、`%async/%await` sugar 和项目级并发开关接到同一条执行链上，让单线程协程路径先成立，再为后续真正的 worker isolate / shared arena 预留模块边界。

这次实现的重点不是一次性做完完整并发计划，而是先把下面四层打通：

- 项目配置能声明 `supportMultithread` 与 `autoCoroutine`
- CLI 能注册 `zr.task` native 模块
- parser 能把 `%async` / `%await` lower 到 `zr.task`
- VM 在 native call 抛错时也能正确进入 `try/catch/finally`

## Related Files

`zr_vm_task/src/zr_vm_task/runtime.c` 是当前 runtime 主体。它定义 `Async<T>` / `Scheduler` / `Channel<T>` / `Mutex<T>` / `Shared<T>` / `Transfer<T>` / `WeakShared<T>` / `Atomic*` 的 native descriptor，以及 `spawn`、`spawnThread`、`await`、`yieldNow`、`sleep`、`currentScheduler` 的实际行为。

`zr_vm_library/include/zr_vm_library/project.h` 和 `zr_vm_library/src/zr_vm_library/project.c` 扩展 `.zrp` 项目结构，新增：

- `supportMultithread`
- `autoCoroutine`

`zr_vm_parser/src/zr_vm_parser/parser_reserved_task.c` 负责 `%async` / `%await` 的 source-level sugar。`parser_types.c` 负责 `%mutex` / `%atomic` type sugar。`parser_expressions.c`、`parser_statements.c`、`parser_declarations.c` 与 `parser_expression_primary.c` 负责把这些保留形式接入现有表达式、声明和 lambda 入口。

`zr_vm_parser/src/zr_vm_parser/compiler_task_effects.c` 与 `compiler.c` 负责在正式发射函数前做 task effect 预检查，补上 `await` 上下文和显式 borrowed-across-await 诊断。`compile_expression_support.c` 与 `compile_time_executor_support.c` 则负责避免 `zr.task` import/member 调用被错误地拉进 compile-time projection。

`zr_vm_core/src/zr_vm_core/execution_dispatch.c` 与 `execution_control.c` 负责 native call 抛错后的异常回卷。没有这层修正，`spawnThread(...)` 在多线程关闭时虽然能创建异常对象，但 VM 不会把它送进局部 `catch`。

## Behavior Model

### Module Surface

当前 `zr.task` 根模块公开以下函数：

- `spawn(fn): Async<T>`
- `spawnThread(fn): Async<T>`
- `currentScheduler(): Scheduler`
- `await(handle): T`
- `yieldNow(): null`
- `sleep(ms): null`

公开以下类型：

- `Async<T>`
- `Scheduler`
- `Channel<T>`
- `Mutex<T>`
- `Shared<T>`
- `Transfer<T>`
- `WeakShared<T>`
- `AtomicBool`
- `AtomicInt<T>`
- `AtomicUInt<T>`

其中只有 `Async<T>` 与 `Scheduler` 已经有真实行为；其余类型目前还是 descriptor 级占位，先稳定模块面、泛型 metadata 和类型提示，再等待后续 shared arena / lock-free / channel 语义落地。

### Scheduler Storage Model

当前 scheduler 不放在 parser 私有状态，也不放在线程局部静态变量里，而是挂在 `global->zrObject` 上的隐藏字段中：

- `__zr_task_scheduler`
- `__zr_task_worker_scheduler`

每个 scheduler 自带：

- 队列数组 `__zr_task_queue`
- 队列头 `__zr_task_queue_head`
- `__zr_task_auto_coroutine`
- `__zr_task_support_multithread`
- `__zr_task_is_pumping`

这个结构意味着当前实现仍然是“单个 `SZrGlobalState` 内的 cooperative scheduler”，还不是多 isolate 真并发；但 `main scheduler` 和 `worker scheduler` 已经被拆成两个显式槽位，后续换成真正的 worker runtime 时不用再改模块 API。

### Async Handle State Machine

`Async` handle 通过对象字段保存任务状态：

- `CREATED`
- `QUEUED`
- `RUNNING`
- `COMPLETED`
- `FAULTED`

同时记录：

- `__zr_task_callable`
- `__zr_task_result`
- `__zr_task_error`
- `__zr_task_scheduler_owner`

`spawn` / `spawnThread` 先创建 handle，再把 handle 入队到对应 scheduler。执行完成后：

- 成功时把 `result` 写回 handle，状态改成 `COMPLETED`
- 失败时把异常值写回 `error`，状态改成 `FAULTED`

`Async.result()` 与 `%await handle` 最终都走 `zr_vm_task_wait_for_handle(...)`，因此二者对 pending / faulted / completed 的处理一致。

### autoCoroutine And Manual Pump

`autoCoroutine` 的默认值来自项目配置：

- `.zrp` 未声明时默认为 `true`
- `.zrp` 明确写 `false` 时，需要外部显式调用 `Scheduler.step()` 或 `Scheduler.pump()`

运行时行为是：

- `spawn(...)` 入队后，如果 scheduler `autoCoroutine == true` 且当前不在 pump 递归中，runtime 立即自动 drain 队列
- `await(...)` 发现 handle 仍是 pending 时，如果 `autoCoroutine == true`，会主动 pump 所属 scheduler 再重试
- 如果 `autoCoroutine == false` 且任务仍未完成，`await` 直接报运行时错误，要求调用方先手动 `pump`

这条路径对应用户要求里的“像 GC 一样自动启动，也可以关闭后由游戏引擎手动控制 Tick/Update”。

### supportMultithread Gate

`supportMultithread` 只控制 worker 相关入口，不影响本地 async / coroutine：

- `false` 时，`spawnThread` 会构造异常并让脚本层 `try/catch` 可捕获
- `true` 时，`spawnThread` 目前把任务排进 `worker scheduler` 占位队列

当前 `worker scheduler` 仍然和主调度器共处同一个 `SZrGlobalState`，因此它只是“接口和调度槽位已分离”的过渡实现，不是最终的多虚拟机 state。

## Parser Lowering

### `%await`

`%await expr` 会被 parser 直接改写成：

```zr
%import("zr.task").await(expr)
```

这样 runtime 只需要维护一个普通 native function，不需要新增解释器层特殊 await opcode。

### `%async`

`%async func addOne(value: int): int { ... }` 当前会被改写成一个普通函数声明，其函数体再包装成：

```zr
return %import("zr.task").spawn(() -> {
    // original body
})
```

如果源代码写了显式返回类型，parser 现在会把它从 `T` 包成 `Async<T>`，例如：

```zr
%async func addOne(value: int): int
```

会落成返回 `Async<int>` 的声明，而不是直接清空返回类型。

这意味着当前版本的 `%async` 是“parser sugar + native runtime handle”，而不是 semir / execbc 层一等 async 函数形态。它先满足了语言可写性和基本行为验证，后续再继续向 effect/opcode 方案收敛。

### `%mutex` / `%atomic`

这一版把并发类型 sugar 也提前接到了 parser：

- `%mutex T` 会改写成 `Mutex<T>`
- `%atomic bool` 会改写成 `AtomicBool`
- `%atomic int` / `i8/i16/i32/i64` 会改写成 `AtomicInt<T>`
- `%atomic uint` / `u8/u16/u32/u64` 会改写成 `AtomicUInt<T>`

`%atomic` 目前仍然只接受布尔或整数标量。像 `string`、数组、带 ownership 包装的复合类型或普通 GC 引用，在 parser 阶段就会被拒绝。

### Compile-Time Guard

`compile_expression_support.c` 额外跳过了 `%import("zr.task")...` 的 compile-time projection。否则 `%async` 包装体里的 `zr.task.spawn(...)` 会被错误当作 compile-time import/member 解析，直接在编译阶段失败。

`compile_time_executor_support.c` 也避开了 async wrapper 函数的 compile-time 注册，防止编译器把 sugar 生成的包装体误判成普通 compile-time candidate。

`compiler_task_effects.c` 则在正式编译前补上一轮 task effect 预检查，当前落地了两条规则：

- `await` 只允许出现在 `%async` 包装出来的 async body，或脚本顶层这种 scheduler-managed coroutine body 里
- 显式 `%borrowed` 参数或局部变量不能在 `await` 之后再次使用

这里的 borrowed 检查还是保守版，只覆盖显式 `%borrowed` 绑定，不尝试推导所有隐式借用或复杂 capture 图。

## Native Exception Integration

这次并发运行时补丁还修正了一个 core 级缺口：native call 返回 `NULL` 时，dispatch loop 之前只会恢复 `base/trap`，不会检查“native 绑定是否已经把异常规范化到 `state->currentException`”。

修正后：

1. `execution_dispatch.c` 为全部 native call opcode 统一走 `RESUME_AFTER_NATIVE_CALL(...)`
2. 如果 native 绑定已经设置 `hasCurrentException`，dispatch 会立即调用 `execution_unwind_exception_to_handler(...)`
3. `execution_control.c` 在跳转到 `catch` 或 `finally` 前把 `threadStatus` 恢复为 `FINE`

第三步很关键。没有它，脚本层虽然已经 catch 住异常，外层测试框架仍会把残留的错误线程状态当成未处理异常。

这项修正不只服务 `zr.task`。任何 future native module 只要按现有 `NormalizeThrownValue(...)` 路径抛错，现在都能进入局部 `try/catch/finally`。

## Design And Rationale

### Why `zr.task` Is A Separate Module

这次没有把并发 API 塞进 `zr.system`，因为并发语义本身会继续演进：

- cooperative scheduler
- worker isolate
- shared wrappers
- channels
- atomics

把它独立成 `zr.task`，可以让 parser、CLI、type hints、native registry 和项目配置围绕一个清晰命名空间收敛，不和系统 I/O、进程、GC 控制 API 混在一起。

### Why v1 Uses Cooperative Pump First

当前仓库还没有完成“多 `SZrGlobalState` 真 worker isolate + shared arena + sendable ownership rules”。在这个前提下，先做 cooperative scheduler 有两个价值：

- `%async/%await` 可以立即变成可执行语言能力，而不是只停留在设计稿
- `autoCoroutine` / 手动 `pump` 的生命周期控制可以先被引擎侧验证

这也符合原计划里的里程碑顺序：先把 runtime 拓扑与 safe point 管理做实，再继续向真正的多线程扩展。

## Edge Cases And Constraints

- 当前 `spawnThread` 不是透明降级。多线程关闭时，它明确抛错，不会偷偷回退到主线程 `spawn`
- `yieldNow()` 目前只是对 main scheduler 执行一次 `step()`，还没有真正的 suspension effect 分析
- `sleep()` 仍是占位实现，只返回 `null`
- `Channel<T>` / `Mutex<T>` / `Shared<T>` / `Transfer<T>` / `WeakShared<T>` / `Atomic*` 目前只有模块类型面，没有共享内存、worker isolate 或原子 opcode 实现
- 当前 `%async` / `%await` 仍然是 parser lowering，不等于已完成计划中的 semir effect、真实 worker isolate 或跨线程发送规则；本次只把 `await` 上下文和显式 borrowed-across-await 诊断先落地
- 当前 `worker scheduler` 仍处于同一 global state，因此没有实现文档计划里的“多个 isolate 各自 GC”

## Test Coverage

`tests/task/test_task_runtime.c` 当前覆盖：

- `.zrp` 默认 `supportMultithread = false`、`autoCoroutine = true`
- `.zrp` 显式读取 `supportMultithread` / `autoCoroutine`
- `zr.task` 模块成功注册到 native registry
- `Async<T>` / `Channel<T>` / `Mutex<T>` / `Shared<T>` / `Transfer<T>` / `WeakShared<T>` / `Atomic*` 的泛型 descriptor 面
- 手动 `Scheduler.pump()` 可执行本地任务
- `spawnThread` 在多线程关闭时抛错且能被脚本 `catch`
- `%async` / `%await` sugar 能 lower 并执行
- `%async` 显式返回类型会被包装成 `Async<T>`
- `%mutex` / `%atomic` type sugar 能通过 parser，且 `%atomic string` 这类非法用法会被拒绝
- 非 async 函数体里的 `%await` 会在编译阶段被拒绝
- 显式 `%borrowed` 绑定跨 `await` 复用会在编译阶段被拒绝

`tests/exceptions/test_exceptions.c` 是这次 core 修正的回归护栏，确保已有 `throw/catch/finally` 行为仍保持原语义。

`tests/parser/test_type_inference.c` 与 `tests/parser/test_compiler_features.c` 是这次 parser/compiler 改动的额外回归样本，用来确认 type parser、generic type materialization 和 compiler 入口的既有行为没有被并发 sugar 破坏。

## Plan Sources

本文件对应 2026-04-05 的用户并发运行时方案，当前已经落地的部分是：

- `zr.task` 独立标准模块
- 项目级 `supportMultithread` / `autoCoroutine`
- `%async` / `%await` source sugar
- cooperative scheduler 与手动 pump
- native exception -> VM `try/catch` 回卷

尚未落地但已明确留出扩展边界的部分是：

- 真正的 worker isolate / 多 `SZrGlobalState`
- shared arena / `Transfer` / `Shared` / `WeakShared`
- `%mutex` / `%atomic` 对应的一等 opcode 与同步原语运行时
- 更完整的 suspension/thread effect 与 beyond-explicit-borrow 的 capture 诊断

## Open Issues Or Follow-up

- `spawnThread` 目前只有配置门禁和独立 scheduler 槽位，没有真实线程或 isolate 创建
- `Async<T>` 现在已经在 `%async` 显式返回注解上 materialize 成 parser-level 泛型返回类型，但还没有进入更深层的 semir / execbc async effect 模型
- 若后续把 async/coroutine 升级为 semir / execbc 一等 effect，需要把当前 parser sugar 文档替换成正式 lowering 文档，而不是继续叠加兼容描述
