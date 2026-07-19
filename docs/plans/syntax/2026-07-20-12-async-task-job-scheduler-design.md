# 12 `async/await`、Task/Job/Scheduler与线程协程模型实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 用显式`zr.task.Task<T>`返回类型、一个cold `zr.task.Job<T>`和一个`zr.task.Scheduler.schedule`contract表达异步、协程与线程执行，不引入spawn/thread/coroutine/job关键字、重复模块或第二套启动协议。

**Architecture:** `async fn`是返回`Task<T>`的普通`fn`；`await`挂起当前state。需要延迟或指定执行位置时，以既有`init`构造non-Copy Job并交给Scheduler。`zr.thread.ThreadScheduler`只提供独立isolate上的同一Scheduler contract，不创建第二套Task类型。

**Tech Stack:** Canonical callable/effect、Place/CFG suspension facts、borrow checker、TypeLayout/GC map、ZR VM state machine、`zr.task`、`zr.thread`、AOT、artifact、LSP、Unity/CMake tests。

---

> 状态：按Occam原则修订，等待按里程碑实施。
>
> 上游契约：[01 Semantic IR/CFG/artifact](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[02 borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md)、[03 ref struct/layout](./2026-07-18-03-struct-ref-struct-span-layout-design.md)、[04 ownership/GC](./2026-07-18-04-resource-ownership-drop-gc-bridge-design.md)、[09 PoolRef](./2026-07-19-09-generational-pool-handle-ref-struct-design.md)、[10 modules](./2026-07-19-10-native-ffi-module-package-design.md)。

## 1. Occam硬约束

第一版public entities只有：

```text
Task<T>             hot completion
Job<T>              cold, non-Copy work value
Scheduler           schedule contract
ThreadScheduler     Scheduler的thread-isolate provider
Send                 可跨isolate move的type capability
```

只保留两个相关关键字：已有迁移目标`async`和`await`。不增加`spawn`、`thread`、`coroutine`、`job`、`scheduler`或`task`关键字。

第一版删除此前草案中的：`Async<T>`、`TaskRunner<T>`、TaskGroup、detach、SharedCell、Sync、CancellationToken、Channel、autoCoroutine、普通业务`pump/step`和`create/start/spawn` helper。

必要性：

- Task不可省：await需要稳定completion/exception identity。
- Job不可省：用户明确需要cold work和指定scheduler；直接调用async fn已经是hot，二者语义不同。
- Scheduler不可省：coroutine、host loop与thread pool需要一个共同投递边界。
- ThreadScheduler不可省：它是Scheduler的provider，不是新语言概念。
- Send不可省：不同GC isolate之间必须静态拒绝裸GC/native pointer捕获。

### 1.1 Native ModuleId唯一归属

这些public entity全部由第10章OfficialNative domain的N1/N2 native descriptor提供，不由ZR source、Package或RegisteredNative module定义：

| Public entity | Canonical owner | provider |
|---|---|---|
| `zr.task.Task<T>` | `zr.task` | N1 Runtime native descriptor + compiler async frame ABI |
| `zr.task.Job<T>` | `zr.task` | N1 Runtime native-described non-Copy value |
| `zr.task.Scheduler` | `zr.task` | N1 Runtime native interface/capability |
| `zr.thread.ThreadScheduler` | `zr.thread` | N2 Runtime native provider class |
| `zr.thread.Send` | `zr.thread` | N2 Runtime native capability metadata |

`let task = import("zr.task");`只建立ModuleNamespace alias，因此源码中的`task.Task<T>`仍规范化为`zr.task.Task<T>`。禁止`zr.async`、`zr.coroutine`、`zr.job`、`zr.scheduler`或`zr.thread.Task/Job`同义定义。compiler按descriptor注册的TaskCarrier/JobWork/Scheduler/Send capability id绑定，不按短名或完整名字字符串特判。

## 2. 所有async函数显式返回`zr.task.Task` TypeRef

```zr
let task = import("zr.task");

async fn readConfig(path: string): task.Task<Config> {
    let bytes: Buffer<byte> = await fs.readAll(path);
    return parseConfig(bytes);
}

fn loadUser(id: UserId): task.Task<User> {
    return readConfig(userPath(id));
}

let loader: fn(UserId) -> task.Task<User> = loadUser;
```

规范：

- `async fn f(...): Task<T>`显式声明真实call result；不允许写`: T`后由compiler/LSP隐藏改成Task。
- body内`return value;`要求`value: T`，与C# async Task<T> method一致；`Task<void>`使用`return;`。
- `async`只改变body lowering/effect，不改变declaration TypeRef。
- `await expr`第一版只接受canonical`Task<T>`，结果类型为T；不实现C#任意`GetAwaiter` member pattern。
- async lambda同样写`async fn(args): Task<T> { ... }`或`=> expression`。
- `await`只出现在显式async function/lambda或13定义的AsyncIterator function中。
- async iterator显式返回`AsyncIterator<T>`，是13基于同一suspension contract的唯一扩展，不产生hidden Task<AsyncIterator<T>>。

### 2.1 参数、receiver与结果限制

第一版禁止async function的`in/ref/out`参数和ref return；`Task<T>`中的T不能是ref/ref struct/Span/PoolRef/lock guard。

- GC class/interface receiver可进入frame。
- resource/Unique receiver必须move进frame，调用后原owner为Moved。
- mutable struct receiver禁止；readonly struct receiver按value copy进入frame。
- ref、Span、PoolRef、pin和guard可在await前短暂使用，但最后使用必须早于所有可达suspension edge。
- 不引入runtime borrow proxy或async out slot。

## 3. Task语义

```text
TaskState = Running | Suspended | Completed | Faulted
```

- async invocation立即在caller执行，直到完成、throw或首个未完成await。
- completed fast path直接读取inline result，不入queue、不创建continuation node。
- pending await登记continuation并返回scheduler；完成后恢复到awaiter所属scheduler。
- fault在await point重抛structured exception并保留logical async stack；finally/Drop只执行一次。
- Task属于创建它的isolate且不是Send。ThreadScheduler返回caller-isolate中的proxy Task，worker只传输Send result/exception。
- 当T为Copy/GC handle时可多次await；当T为non-Copy owner时，await消费唯一Task并move结果。
- Task是must-use。第一版不提供detach/forget函数；未await、未return或未存入可证明consumer的Task产生diagnostic。故意fire-and-forget不属于第一版安全模型。
- scheduler关闭时pending Task统一fault并释放frame/captures，不增加Cancelled状态或CancellationToken实体。

## 4. Job只用`init`构造

```zr
let task = import("zr.task");
let thread = import("zr.thread");

let checksumJob: task.Job<int> = init task.Job<int>(
    fn(): int => calculateChecksum()
);

let local: task.Task<int> =
    task.currentScheduler.schedule(checksumJob);

let workers: thread.ThreadScheduler =
    new thread.ThreadScheduler(workerCount: 4);

let compression: task.Job<Buffer<byte>> =
    init task.Job<Buffer<byte>>(
        fn(): Buffer<byte> => compress(input)
    );

let compressed: task.Task<Buffer<byte>> =
    workers.schedule(compression);
```

- Job是non-Copy value，拥有callable和captures；`schedule(job)`按值消费。
- Job constructor接受`fn() -> T`。要调度可挂起work，callable写`fn() -> Task<T>`并由constructor overload规范化为`Job<T>`；两种constructor都通过既有`init`选择，不增加`create/createAsync`函数。
- local scheduler允许普通合法captures；ThreadScheduler要求captures和T满足Send。
- Job尚未开始而scheduler关闭时直接Drop captures并fault proxy Task。
- priority/deadline/affinity/work stealing属于provider configuration，不进入第一版语言或Job contract。

## 5. Scheduler最小contract

```zr
interface Scheduler {
    fn schedule<T>(job: Job<T>): Task<T>;
}
```

`zr.task`native descriptor只增加两个有source reference的协作式函数和一个readonly property；以下代码是descriptor生成的interface projection，不是ZR实现源码：

```zr
property currentScheduler: Scheduler { pub get; }
fn yieldNow(): Task<void>;
fn delay(duration: Duration): Task<void>;
```

- 普通业务不调用`pump/step`。embedding host通过C host ABI驱动event loop，该ABI不成为ZR源码函数。
- scheduler是cooperative，不preempt用户代码。CPU loop显式await `yieldNow`或包装为thread Job。
- continuation总回到awaiter scheduler；不增加SynchronizationContext第二套capture对象。
- custom provider实现Scheduler capability，VM不检查具体type name。
- delay只表达timer completion；第一版没有CancellationToken overload。

## 6. thread isolate

`zr.thread.ThreadScheduler`实现同一Scheduler：

- 每个worker拥有独立`SZrGlobalState`、GC、module registry、scheduler和root state。
- ThreadScheduler object是caller isolate中的provider proxy；worker heap pointer不暴露给caller。
- Job capture和result必须是Send。Send由Canonical Type capability和closed field/layout递归计算，不依赖marker type name。
- blittable value、immutable string/blob和显式resource transfer可Send；普通class、closure捕获的GC object、ref/ref struct、native pointer默认不可Send。
- 用户第一版不能unsafe声明Send。
- 一个Job只有一个request/result transport，不引入Channel/Shared/Sync等常驻跨线程实体。

## 7. Suspension IR、frame与GC

```text
AsyncEnter(declaredTaskType)
AwaitPoll(taskValue, readyEdge, suspendEdge)
Suspend(stateId, livePlaces, cleanupState)
Resume(stateId)
AsyncComplete(value?)
AsyncFault(exception)
```

- suspension edge在borrow/dataflow前可见；backend不从`zr.task.await`函数名反推。
- 只hoist跨suspension live locals；每个state有precise initialized/drop/GC map。
- invocation先使用普通call frame；实际pending await时才promotion到pooled coroutine frame。
- promotion前必须证明没有escaping ref指向旧stack slot。
- Task header和frame允许合并一次pool allocation；同步完成不分配frame。
- VM/AOT共享FrameLayout/state ids；dynamic object field不是async truth。
- try/finally、owner Drop和partial initialization统一进入cleanup CFG。

## 8. Artifact、LSP与debug

- Canonical callable保存Async effect和显式`Task<T>` return TypeId。
- public return TypeId规范化为`zr.task.Task<T>`；ThreadScheduler返回的proxy也使用同一TypeId，不存在thread-local Task type。
- `.zri`保存suspension CFG、frame liveness/layout、awaiter scheduler fact和source map。
- `.zro`保存public `zr.task.Task<T>` signature、Task/Await ABI version、owner ModuleId和required scheduler capability。
- LSP hover只显示源码真实signature，不再显示“completion T / hidden result Task<T>”双层信息。
- debugger显示logical async stack；step-over await在suspend时返回host，resume后停在下一source statement。
- dropped Task、borrow across await和non-Send Job提供定向diagnostic。

## 9. 迁移

| 旧形态 | 目标 |
|---|---|
| `%async fn f(): T` | `async fn f(): zr.task.Task<T>`或import alias后的`task.Task<T>` |
| `%await expr` | `await expr` |
| `Async<T>` / `TaskRunner<T>` | `Task<T>`或`Job<T>`，按hot/cold语义人工确认 |
| `runner.start()/spawn(...)` | `scheduler.schedule(init Job<T>(callable))` |
| autoCoroutine/pump业务调用 | 删除；host loop自动驱动 |
| cross-thread shared wrapper | 第一版改为Job capture/result的Send value；无法表达则blocked |

旧artifact的TaskRunner contract不能静默解释为Task，必须重编译或迁移。

## 10. 分层里程碑与验收

### M1 explicit Task syntax/effect

为`zr.task`descriptor登记Task carrier role，覆盖async named/member/lambda、显式Task TypeRef、return/await、invalid modifiers/params/results和suspension CFG。

### M2 Task/frame runtime

覆盖sync completion、single/multiple suspension、fault/finally、multi-await、non-Copy result、frame GC/Drop map和pooling。

### M3 Job/Scheduler

在同一`zr.task`descriptor登记Job/Scheduler及constructor/functions，覆盖single consume、schedule/currentScheduler/yieldNow/delay、must-use和provider ABI。

### M4 ThreadScheduler/Send

在`zr.thread`descriptor只登记ThreadScheduler/Send，覆盖worker isolate、capture/result transport、awaiter affinity、scheduler close和non-Send diagnostics；不得re-export Task/Job/Scheduler TypeDef。

### M5 artifact/debug/LSP/migration

删除Async/TaskRunner/autoCoroutine public surface和concrete Task type-name checks。

晋级要求：

- 所有async declaration显式写`: Task<T>`或13的`: AsyncIterator<T>`；
- `Task<T>`、`Job<T>`、`Scheduler`的owner TypeId只能是`zr.task`，ThreadScheduler/Send只能是`zr.thread`；builtin/plugin/source import得到相同contract hash。
- parser没有新增job/scheduler/thread/coroutine关键字；
- public ZR functions仅有reference ledger列出的`schedule/yieldNow/delay`；
- sync-complete path allocation counter为0；
- illegal borrow在lowering前失败；
- Task/frame全exit释放；
- thread provider不跨isolate传裸GC/native pointer；
- million completed awaits、100k suspended Tasks、worker churn和GC stress有测试。

## 11. Public API reference ledger

任何未列在本表的公开function/type role不属于第一版。表内implementation与test必须写成当前仓库中可解析的文件路径，不能只写类名或目录名。

| ZR surface | 必要性 | reference implementation | reference tests |
|---|---|---|---|
| `async fn ...: zr.task.Task<T>` / `await` | 显式异步状态机与等待 | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Threading/Tasks/Task.cs` | `lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenAsyncTests.cs`；`lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/BindingAsyncTests.cs`；`lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenAsyncEHTests.cs` |
| `zr.task.Task<T>` | completion/result/fault identity | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Threading/Tasks/Task.cs`；`lua/jdk/src/java.base/share/classes/java/util/concurrent/FutureTask.java` | `lua/runtime/src/libraries/System.Runtime/tests/System.Threading.Tasks.Tests/Task/TaskStatusTest.cs`；`lua/jdk/test/jdk/java/util/concurrent/tck/FutureTaskTest.java` |
| `zr.task.Job<T>`；`init Job<T>(fn() -> T | fn() -> Task<T>)` | cold、non-Copy work type及两种constructor contract | `lua/jdk/src/java.base/share/classes/java/util/concurrent/Callable.java`；`lua/jdk/src/java.base/share/classes/java/util/concurrent/FutureTask.java`；`lua/rust/library/core/src/future/future.rs` | `lua/jdk/test/jdk/java/util/concurrent/tck/FutureTaskTest.java`；`lua/rust/tests/ui/async-await/async-fn-nonsend.rs` |
| `zr.task.Scheduler.schedule(job): Task<T>` | 唯一投递边界 | `lua/jdk/src/java.base/share/classes/java/util/concurrent/ExecutorService.java`；`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Threading/Tasks/TaskScheduler.cs` | `lua/jdk/test/jdk/java/util/concurrent/tck/AbstractExecutorServiceTest.java`；`lua/runtime/src/libraries/System.Runtime/tests/System.Threading.Tasks.Tests/TaskScheduler/TaskSchedulerTests.cs` |
| `task.currentScheduler` | 明确continuation归属 | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Threading/Tasks/TaskScheduler.cs` | `lua/runtime/src/libraries/System.Runtime/tests/System.Threading.Tasks.Tests/TaskScheduler/TaskSchedulerTests.cs`；`lua/testes/coroutine.lua` |
| `task.yieldNow(): Task<void>` | cooperative fairness | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Runtime/CompilerServices/YieldAwaitable.cs` | `lua/runtime/src/libraries/System.Runtime/tests/System.Threading.Tasks.Tests/System.Runtime.CompilerServices/YieldAwaitableTests.cs`；`lua/testes/coroutine.lua` |
| `task.delay(Duration): Task<void>` | timer await without blocking thread | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Threading/Tasks/Task.cs`；`lua/jdk/src/java.base/share/classes/java/util/concurrent/ScheduledThreadPoolExecutor.java` | `lua/runtime/src/libraries/System.Runtime/tests/System.Threading.Tasks.Tests/Task/TaskRtTests.cs`；`lua/jdk/test/jdk/java/util/concurrent/tck/ScheduledExecutorTest.java` |
| `new zr.thread.ThreadScheduler(workerCount: int)` | 同一contract的worker provider及唯一public constructor | `lua/jdk/src/java.base/share/classes/java/util/concurrent/ThreadPoolExecutor.java` | `lua/jdk/test/jdk/java/util/concurrent/tck/ThreadPoolExecutorTest.java`；`lua/jdk/test/jdk/java/util/concurrent/tck/ThreadPoolExecutorSubclassTest.java` |
| `zr.thread.Send` capability | 静态限制跨isolate value | `lua/rust/library/core/src/marker.rs` | `lua/rust/tests/ui/async-await/async-fn-nonsend.rs`；`lua/rust/tests/ui/async-await/async-fn-send-uses-nonsend.rs` |

刻意差异：ZR采用.NET显式Task return和eager execution，但不复制custom awaiter、SynchronizationContext、TaskCreationOptions或完整cancellation体系；thread只作为Scheduler provider。
