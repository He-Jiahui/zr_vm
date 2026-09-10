---
related_code:
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/execution.h
  - zr_vm_core/include/zr_vm_core/execution_budget.h
  - zr_vm_core/include/zr_vm_core/session_checkpoint.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/log.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_core/src/zr_vm_core/session_checkpoint.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，要求详细介绍 C native 库调用方案和宿主接口
  - docs/core-runtime/index.md
  - docs/library-and-builtins/index.md
tests:
  - tests/core/test_session_checkpoint.c
  - tests/core/test_execution_add_stack_relocation.c
  - tests/core/test_execution_numeric_fast_paths.c
  - tests/core/test_aot_gc_root_frame.c
doc_type: api-reference
---

# Core 宿主状态、执行与预算 C API

本页定义嵌入式宿主与 `zr_vm_core` 打交道时的最小控制面：`SZrGlobalState` 是共享堆、模块
缓存、provider hook 和 allocator 的 owner，`SZrState` 是一个可执行 mutator 的栈、异常状态和
调试上下文。二者不是可随意复制的 POD；只能通过本页列出的构造、退出和释放 API 管理。

如果宿主还要加载项目、标准库或 descriptor plugin，应在 global/state 可用后继续
[Library/Project 包 API](project-file-zrm-api.md) 和 [Native Registry 调用绑定](../03-modules/native-registry-call-binding.md)。
若只需从字符串生成函数，请接 [Parser/Compiler 深度 API](parser-compiler-c-api.md)。

## 1. 对象模型与 owner 边界

```text
application process
  -> SZrGlobalState              one allocator/accounting domain
       -> primary SZrState        main-thread mutator, stack, diagnostics
       -> optional SZrState...    attached-domain mutators only
       -> GC / string table / module cache / native registry state
```

| 对象 | 谁创建 | 谁释放 | 可以跨线程吗 | 关键限制 |
| --- | --- | --- | --- | --- |
| `SZrGlobalState` | `ZrCore_GlobalState_New` | `ZrCore_GlobalState_Free` | 本身不是并发调用许可 | 所有 state 和 provider 必须先退出/释放 |
| 主 `SZrState` | global 创建流程建立 | `ZrCore_State_Free` 或 global 收尾路径 | 否 | 负责 global registry 初始化和 main thread 生命周期 |
| attached `SZrState` | `ZrCore_State_New` | `ZrCore_State_Free` | 仅在其绑定 mutator 线程中使用 | 先 `MutatorLaunch`，后 `MutatorExit` |
| execution budget | 宿主构造并挂到 `state->executionBudget` | 宿主 | token 可被取消端观察 | API 不转移 budget 的所有权 |
| session checkpoint | `Create` | `Free` | 否 | snapshot 只对应创建它的 state/global generation |

`SZrGlobalState` 与 `SZrState` 的字段出现在公共头文件中，是为了模块间协作而非鼓励直接写入。
特别是 `mainThreadState`、GC 指针、module cache、exception stack、stack top 和 registry state
都只能由 Core/Library API 改动。宿主应把它们视为只读诊断信息。

## 2. 创建 global

### 2.1 构造函数

```c
SZrGlobalState *ZrCore_GlobalState_New(
    FZrAllocator allocator,
    TZrPtr userAllocationArguments,
    TZrUInt64 uniqueNumber,
    SZrCallbackGlobal *callbacks);
```

| 参数 | 含义 | 宿主规则 |
| --- | --- | --- |
| `allocator` | Core 的分配/重分配/释放回调 | 必须在 global 生命周期内始终有效；回调须遵循 `FZrAllocator` 的 `pointer/size` 协议 |
| `userAllocationArguments` | 原样传回 allocator 的用户上下文 | 由宿主拥有，不由 Core 释放 |
| `uniqueNumber` | cache identity / hash seed 的输入 | 同一进程中希望隔离缓存的 global 使用不同值 |
| `callbacks` | 全局 callback 表，可为 `ZR_NULL` | 表本身不被 Core 接管；其函数和 user data 必须晚于 global 存活 |

返回 `ZR_NULL` 表示初始化失败。此时不能调用 `ZrCore_GlobalState_Free`，也不能假设 allocator
已经接管任何应用层对象。成功后 global 会拥有堆、GC、字符串表和默认 runtime registry；宿主
不要自行创建第二份这些内部结构。

最小创建骨架如下。示例中的 allocator 名称仅表示符合 `FZrAllocator` 签名的实现，不是库内置
符号：

```c
SZrCallbackGlobal callbacks = {0};
SZrGlobalState *global = ZrCore_GlobalState_New(
        host_allocator, host_allocator_context, 0x6a72566dULL, &callbacks);
if (global == ZR_NULL) {
    return HOST_ERR_VM_INIT;
}

/* 注册 parser/library/provider 前，global 已经是有效 owner。 */
```

### 2.2 global 可配置 hook

这些 setter 只改变 global 的关联 hook；不加载模块，也不验证 callback 的语义。应在任何 import、
compile 或执行发生前设置完毕。

| API | 安装的行为 | 何时调用 | 失败/寿命 |
| --- | --- | --- | --- |
| `ZrCore_GlobalState_SetCompileSource` | source -> `SZrFunction` 编译桥 | 链接 parser 后 | 函数指针必须在 global 存活期有效 |
| `ZrCore_GlobalState_SetNativeModuleLoader` | 解释执行时的 native module loader | 采用宿主自定义 native 加载策略时 | loader 返回 module 或 `ZR_NULL`；失败原因写 module diagnostic |
| `ZrCore_GlobalState_SetAotModuleLoader` | AOT module loader | 使用 AOT provider 时 | 与 native loader 是不同 ABI 路径 |
| `ZrCore_GlobalState_SetProviderModuleNameResolver` | provider role -> module 名称 | 自定义角色映射前 | `ResolveProviderModuleName` 只查询该 resolver |
| `ZrCore_GlobalState_SetPostGcCleanup` | GC 后清理 owner state | 初始化阶段 | 同时传入 cleanup state 和 cleanup 函数；替换时要保证旧 state 不再被使用 |
| `ZrCore_GlobalState_SetOwnershipStrongRefObserver` | strong-ref 变化观察器 | 诊断/宿主计数需求 | observer 不应重入分配、GC 或同一 ownership 操作 |

`ZrCore_GlobalState_ClearModuleLoadDiagnostic`、`SetModuleLoadDiagnostic` 和
`GetModuleLoadDiagnostic` 组成 import/provider 失败的文字诊断通道。它是 global 的临时缓冲区：
下一次加载或 setter 可覆盖它，因此宿主若要跨调用保存，必须立即复制字符串。

## 3. state 生命周期

### 3.1 主 state 与附属 mutator 的不同路径

| 情形 | 起始 API | 运行 API | 结束 API | 不允许的操作 |
| --- | --- | --- | --- | --- |
| 主执行线程 | global 的 main state / `ZrCore_State_New` | `ZrCore_State_MainThreadLaunch`、`ZrCore_State_DoRun` | `ZrCore_State_Exit` | 让第二个 state 再次初始化全局 registry |
| worker/attached domain | `ZrCore_State_New(global)` | `ZrCore_State_MutatorLaunch` | `ZrCore_State_MutatorExit` | 同时由两个 OS 线程操作同一 `SZrState` |
| 仅重置执行状态 | 现有 state | `ZrCore_State_ResetThread` | 无 | 把 reset 当作释放堆或 module cache |

`ZrCore_State_New(global)` 返回的 state 归同一 global 管理。`ZrCore_State_Init(state, global)` 是
给已分配 state storage 的初始化入口；它不意味着 storage 的 allocator 或释放方式发生转移。
不要对同一 state 交叉使用 `New` 和重复 `Init`。

```c
SZrState *worker = ZrCore_State_New(global);
if (worker == ZR_NULL) {
    return HOST_ERR_OOM;
}

if (!ZrCore_State_MutatorLaunch(worker)) {
    ZrCore_State_Free(global, worker);
    return HOST_ERR_MUTATOR_START;
}

/* 只在此 attached mutator 所在线程使用 worker。 */

ZrCore_State_MutatorExit(worker);
ZrCore_State_Free(global, worker);
```

`MainThreadLaunch` 接受宿主参数指针，进入主线程启动路径；它不是通用的“运行任意函数”接口。
`DoRun(state, entry)` 按 entry 名称运行已就绪的入口，返回 `EZrThreadStatus`。状态大于等于
runtime error 的判断应使用 `ZrCore_Exception_IsStausError`，不要把非零都简单当作异常，因为
线程状态还表达完成、暂停和控制流边界。

### 3.2 严格的销毁顺序

```text
stop submission / cancel work
  -> wait for each attached mutator to exit
  -> release project, native plugin descriptor references, root handles
  -> ZrCore_State_Exit + ZrCore_State_Free for non-main state
  -> release main state according to its owner path
  -> ZrCore_GlobalState_Free
```

不要在 `ZrCore_GlobalState_Free` 之后调用 `ZrCore_State_Free`、native callback、plugin unload 或
logging hook。反方向也不成立：state 退出不会释放 global 的 package cache、registry、GC 或
loader callback context。

## 4. 执行与受控中断

### 4.1 Core 执行入口

`execution.h` 的接口位于编译器/backend 的较低层，通常由 call-info 和已验证 instruction
生成器驱动：

| API | 输入契约 | 输出/副作用 | 供谁使用 |
| --- | --- | --- | --- |
| `ZrCore_Execute` | 有效 `state` 和 `SZrCallInfo` | 推进执行循环，更新 call/frame/thread status | VM dispatch、宿主受控执行包装 |
| `ZrCore_Execution_Add` | 当前 call info 和三个 value slot | 计算 `+`；类型/meta 不适配时建立异常/失败状态 | interpreter/AOT helper |
| `ZrCore_Execution_ToObject` | source、type-name value、destination | 按对象转换语义物化结果 | compiler lowering |
| `ZrCore_Execution_ToStruct` | 同上 | 按 struct 转换语义物化结果 | compiler lowering |

应用嵌入层不应手工伪造 `SZrCallInfo` 或直接把未初始化 `SZrTypeValue` 传给这些函数。对源代码
执行，应使用 compiler/library 的高层入口；对 native 方法调用，应使用 descriptor/call binding
路径。直接调用 Core 执行函数适用于 runtime 作者、AOT glue 或已持有合法 frame 的调试器。

### 4.2 execution budget 与 cancellation

`execution_budget.h` 为宿主提供取消、指令/时间/内存预算的共同观察点。核心对象与操作如下：

| API | 用途 | 调用方须知 |
| --- | --- | --- |
| `ZrCore_ExecutionCancelToken_New` / `Free` | 创建/释放取消 token | token 所有权在宿主；释放前确保执行者不再读取 |
| `ZrCore_ExecutionCancelToken_Cancel` | 请求取消 | 可由协调线程发出；它不是强制杀线程 |
| `ZrCore_ExecutionCancelToken_IsCancelled` | 查询 token | native 长循环应主动轮询 |
| `ZrCore_ExecutionBudget_Poll` | 检查时间、指令和 cancel；可选择消耗一条指令 | interpreter 已自动调用的地方不要重复人为扣账 |
| `ZrCore_ExecutionBudget_NativeEnter` | native callback 进入边界检查 | 自定义 native bridge 应遵循 runtime 的 binding 路径，避免绕过 budget |
| `ZrCore_ExecutionBudget_GcBegin` / `GcEnd` | 标注 GC 期间预算状态 | 仅 GC/runtime 实现使用 |
| `ZrCore_ExecutionBudget_BeginMemory` / `MemoryPeak` | 读取 allocator accounting 基线和峰值 | 返回值用于监控，不是释放额度 |

取消的正确语义是“在可中断点请求停止”：正在运行的 foreign call、阻塞 OS syscall 或不轮询的
native 回调不会被 API 强行中止。对于 C native 模块，建议每批处理一次 token，并让 callback
正常返回失败/异常状态，使 VM 可以执行 cleanup 与 finally。

```c
if (ZrCore_ExecutionCancelToken_IsCancelled(cancelToken)) {
    /* 在 native callback 内把取消转换为受控异常/失败，而不是释放 state。 */
    return ZR_FALSE;
}
```

### 4.3 session checkpoint

checkpoint 用于可回滚宿主会话，不是通用序列化格式：

```c
TZrBool ZrCore_SessionCheckpoint_Create(SZrState *state, ...);
TZrBool ZrCore_SessionCheckpoint_Rollback(SZrState *state, ...);
void ZrCore_SessionCheckpoint_Free(SZrState *state, ...);
```

创建后只允许在相同 state/global 生命周期中 rollback 或 free。它不能跨进程持久化，也不替代
`.zro/.zri/.zrm` artifact；这类文件格式见 [产物格式](../06-reference/artifacts.md)。rollback
前先停止并发 mutator 和 native callback，避免其它线程仍持有旧 frame、对象或 descriptor 的
借用指针。

## 5. source loader、IO 与日志

### 5.1 Source loader

Core 的 `SZrIo` 是 runtime 可消费的读取抽象。常用入口如下：

| API | 作用 | 所有权 |
| --- | --- | --- |
| `ZrCore_Io_New` / `ZrCore_Io_Free` | 分配/释放 IO wrapper | `global` 分配域拥有 wrapper |
| `ZrCore_Io_Init` | 安装 read/close callback 与 custom data | callback context 仍由宿主拥有 |
| `ZrCore_Io_Read` | 读取字节 | buffer 由调用方提供 |
| `ZrCore_Io_LoadSource` | 通过 global source loader 取得 source | 返回 `SZrIoSource`，用 `ReadSourceFree` 释放 |
| `ZrCore_Io_LoadEntryFunctionToRuntime` | 加载可执行入口 | 成功产物归 runtime/module graph，不能随意 free |
| `ZrCore_Io_ReadCallBindings` | 读取 artifact call-binding 段 | 只应在 artifact reader 流程中调用 |

文件系统实现见 `ZrLibrary_File_SourceLoadImplementation`；宿主也可安装自己的 `sourceLoader`，例如
从内存、数据库或加密包读源码。loader 必须遵守 `read`/`close` 配对，即使读取失败也要释放它
自己分配的 reader context。

### 5.2 日志

`ZrCore_Log_Write` 是底层入口；`Printf`、`Resultf`、`Helpf`、`Metaf`、`Diagnosticf`、`Error`、
`Exception` 与 `Fatal` 是按用途分类的格式化 helper。日志是观测通道，不是错误状态本身：
调用 `ZrCore_Log_Error` 不会自动设置 exception，调用 `ZrCore_Exception_Throw` 也不会保证输出。

日志 callback 不应重新进入同一 `SZrState`，尤其不要在 callback 内运行 ZR、触发 GC 或调用同步
import。需要异步写日志时，复制文本后移交给宿主队列。

## 6. 一个受控宿主骨架

下面示例展示 owner 顺序，不试图手工构造 compiler frame：

```c
int host_run(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(
            host_allocator, ZR_NULL, 0x20260910u, &callbacks);
    if (global == ZR_NULL) {
        return HOST_ERR_VM_INIT;
    }

    /* 这里安装 parser/library/provider；失败时仍从同一 cleanup 路径退出。 */
    if (!ZrLibrary_NativeRegistry_Attach(global)) {
        ZrCore_GlobalState_Free(global);
        return HOST_ERR_PROVIDER;
    }

    SZrState *state = global->mainThreadState;
    if (state == ZR_NULL) {
        ZrCore_GlobalState_Free(global);
        return HOST_ERR_STATE;
    }

    /* Compile/load and execute through the owning high-level API. */
    ZrCore_GlobalState_Free(global);
    return HOST_OK;
}
```

该片段故意没有释放 `mainThreadState`：它由 global 建立并应遵循 global 的 owner 路径。对通过
`ZrCore_State_New` 自己创建的附属 state，宿主则必须先 `MutatorExit`（如已启动）再
`ZrCore_State_Free`。

## 7. 常见错误

| 错误 | 原因 | 正确做法 |
| --- | --- | --- |
| 一个 state 在两个线程上执行 | `SZrState` 含 stack、exception 和 debug 可变状态 | 每个 mutator 独占 state；使用 attached-domain API 创建 worker |
| global free 后读取 module diagnostic | diagnostic buffer 属于 global | 在释放前复制需要保存的文本 |
| callback 中长期保存 `SZrState *` | callback state 只在活跃 runtime 生命周期内有效 | 保存应用自己的 opaque handle，并在每次调用重新取得 state |
| `Cancel` 后直接销毁 state | 运行中的 VM/native callback 仍可能访问它 | 先请求取消、等待退出、再释放 |
| 手工写 `state->threadStatus` 或 stack 字段 | 破坏 interpreter/exception 不变式 | 使用 `ResetThread`、异常、执行或项目入口 |
| 把 checkpoint 当作 artifact | session snapshot 与 runtime generation 绑定 | 用 writer/ZRM API 做持久化 |

## 8. 相关页面

- [C 值与 GC 生命周期](c-api-value-lifecycle.md)：`SZrTypeValue`、root/pin、barrier 的使用边界。
- [Core 值、对象与模块 API](core-value-object-api.md)：字符串、对象成员、module cache 和 prototype。
- [Core GC、异常与执行安全 API](core-gc-exception-api.md)：full/step collection、try/throw 和 native pin。
- [Parser/Compiler 深度 API](parser-compiler-c-api.md)：把源码转换为可执行 function/artifact。
- [C 宿主集成指南](c-host-guide.md)：项目和 provider 参与的完整嵌入流程。
