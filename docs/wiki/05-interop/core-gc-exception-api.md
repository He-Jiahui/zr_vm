---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/raw_object.h
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/callback.h
  - zr_vm_core/include/zr_vm_core/execution_budget.h
  - zr_vm_core/include/zr_vm_core/state.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，要求详细介绍 C native 库调用方案和运行时机制
  - docs/core-runtime/index.md
  - docs/plans/syntax/README.md
tests:
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/core/test_gc_domain_bridge.c
  - tests/core/test_aot_gc_root_frame.c
  - tests/exceptions/test_exceptions.c
doc_type: api-reference
---

# Core GC、异常与执行安全 C API

ZrVm 的 GC、异常和执行终止是同一条受控 unwind 链的三个部分：native callback 可以分配、触发
GC、抛异常或被预算取消，但不能通过 `longjmp`、`free(state)` 或裸地址缓存越过 runtime 的清理
边界。本页给出 public Core API 的精确职责，以及 C native 模块在这些边界上应采用的规则。

本页不替代值 root 的实战细节，参见 [C 值与 GC 生命周期](c-api-value-lifecycle.md)；也不替代
descriptor callback ABI，参见 [Native Callback C 实战配方](native-callback-recipes.md)。

## 1. GC 模型：能移动、能分代、受 domain 约束

当前 public model 暴露 minor/major/full collection、concurrent major phase、region/age、
remembered set、pin 和 GC domain。由此导出四条宿主不变式：

1. managed `SZrRawObject *` 的地址不是长期稳定 ABI，任何会分配、调用 ZR、import、GC step 或
   safepoint 的操作都可能使未保护的借用失效；
2. 从 old/pinned/permanent owner 写入 young managed value 时必须有 write barrier；
3. object 只能在所属 GC domain/generation 中使用，跨 mutator 不等同于跨线程 memcpy；
4. `IgnoreObject`、permanent 和 pin 都是窄用途机制，不能把它们当成“关闭 GC”的通用开关。

`SZrRawObject` 公共字段包括 `gcDomainId`、`gcDomainGeneration`、`garbageCollectMark`、
`ownershipControl` 和 finalizer data。它们为诊断/实现可见，不是业务代码修改点。

## 2. collection、统计和调度

| API | 行为 | 合法调用方 | 重要限制 |
| --- | --- | --- | --- |
| `ZrCore_GarbageCollector_GcFull(state, isImmediate)` | 请求完整 collection | 宿主控制点、测试、调试工具 | 先让其它 mutator 达到安全边界；不可在不安全 native critical section 调用 |
| `GcStep(state)` | 推进一步 incremental 工作 | runtime 或受控宿主 loop | 不是“只扫我新建的对象” |
| `CheckGc(state)` | 根据 debt/策略检查是否需要 GC | interpreter/native 桥 | 不提供持久 root |
| `ZrCore_Gc_SafePoint(state)` | 到达可协调安全点 | runtime/合作式 native 路径 | 不应在 callback 持有未 rooted 借用时盲调 |
| `AddDebtSpace(global, size)` | 计入 GC debt | allocator/runtime | 不等于实际分配/释放 API |
| `ScheduleCollection(global, kind)` | 请求 minor/major/full kind | 配置/调度器 | 请求不保证同步完成 |
| `GetStatsSnapshot(global, out)` | 读取 region、heap、worker、暂停预算快照 | 监控/调试 | snapshot 是观察值，不作为同步锁 |
| `SetHeapLimitBytes` | 配置 heap 上限 | 初始化或明确控制点 | 超限由 budget/GC 边界报告，单次分配可能先成功 |
| `SetPauseBudgetUs` / `SetWorkerCount` | 配置 GC 调度目标 | 初始化期 | 不能在任意并发 GC 周期中无协调改写 |

`SZrGarbageCollectorStatsSnapshot` 包含 domain generation、活跃 mutator 数、region 数、各区域
used/live bytes、remembered object 数和 GC 预算。采样代码应复制该结构并异步输出；不要在 log
callback 中再次触发 allocation-heavy object formatting。

## 3. 写屏障：什么时候必须调用

### 3.1 推荐优先级

1. 优先使用 checked 高层 API，例如 `ZrCore_Object_SetMember`、`SetValue`、container/native
   binding helper；它们会维护必要的 ownership/GC 关系。
2. 如果确实直接把 `SZrTypeValue` 写入一个 managed owner field，调用
   `ZrCore_Gc_WriteBarrier(state, ownerRawObject, value)` 或相应 `ZrCore_Value_Barrier`。
3. 仅在 runtime 已证明“owner 是全新对象、不会被观测且不需要 remembered set”时，才使用名字中
   带 `NoWriteBarrier` 的专用 API。

`ZrCore_GarbageCollector_Barrier` 和 `ZrCore_RawObject_Barrier` 接收 raw object pair，
`BarrierBack` 处理反向/重新扫描情形；这些是 GC/runtime 实现工具。native provider 的默认选择
应是值级 `ZrCore_Gc_WriteBarrier` 或对象 setter。

### 3.2 反例

```c
/* 错误：把可能 young 的 child 直接塞进已有 old owner 的自定义字段。 */
owner->embeddedValue = childValue;

/* 正确：使用 owner 的 checked setter，或在已验证 layout 后补齐 barrier。 */
ZrCore_Object_SetMember(state, &ownerValue, memberName, &childValue);
```

错误写法可能只在 minor GC 后暴露，因为 old owner 没进入 remembered set，young child 会被错误
回收或移动。

## 4. native pin 与 temporary root

### 4.1 pin API

```c
SZrGcNativeCallPin pin = {0};
if (!ZrCore_Gc_NativeCallPinValue(state, &value, &pin)) {
    return ZR_FALSE;
}

/* 现在仅按 pin 的合同在 native call 边界持有该对象。 */
foreign_library_uses_object(pin.object);

ZrCore_Gc_NativeCallUnpin(state->global, &pin);
```

| API | 适用对象 | 必须配对 |
| --- | --- | --- |
| `ZrCore_Gc_NativeCallPinObject` | 已知 `SZrRawObject *` | `NativeCallUnpin` |
| `ZrCore_Gc_NativeCallPinValue` | `SZrTypeValue` 中的 managed object | `NativeCallUnpin` |
| `ZrCore_Gc_NativeCallUnpin` | 上述 pin | 即使 foreign call 失败也要执行 |
| `ZrCore_Gc_AotRootFramePush/Pop` | AOT generated frame/root map | LIFO 严格配对 |
| `ZrCore_Gc_AotRootFrameDepth` | 调试 root stack | 只读验证用途 |

pin 的目的不是让对象永久不可收集，而是使 foreign/native 调用期间的对象地址与可达性受 runtime
追踪。长时间 pin 会恶化 compact/region 效率；不能跨 asynchronous callback、plugin reload、
线程移交或 state teardown 保存。异步交互应转为明确的 owned/serialized payload，回到 VM 时再
创建/root 新值。

`ZrLibTempValueRoot` 是 native binding 提供的临时 root 机制，适合 callback 内多次分配的局部
值。不要把 `SZrGcNativeCallPin`、`ZrLibTempValueRoot` 和 `IgnoreObject` 混用来“多重保险”；
先选择与时长相符的机制，并确保每条错误分支都释放它。

### 4.2 Ignore、permanent 与 escaped promotion

| API | 作用 | 不可替代的条件 |
| --- | --- | --- |
| `IgnoreObject` / `UnignoreObject` | 临时从普通回收路径排除对象 | 谁添加 ignore，谁在对象仍 live 时撤销；先检查 `IsObjectIgnored` |
| `PinObject` | 按 `EZrGarbageCollectPinKind` pin | 只由 runtime/明确 native ownership 边界使用 |
| `MarkRawObjectEscaped` / `MarkValueEscaped` | 标记跨 scope/domain 的逃逸并请求 promotion | 必须传真实 escape flags、scope depth 和 promotion reason |
| `RawObject_MarkAsPermanent` | 仅最新生成对象转 permanent | 不是对象缓存通用 API |

`IgnoreObject` 和 permanent 可能使对象延迟回收甚至跨 major cycle 存活，误用会形成逻辑泄漏。
native module 若需要拥有外部资源，应优先通过 resource descriptor/finalizer/ownership 规则表达
资源生命周期，而非 ignore 所有对象。

## 5. GC domain 与多 mutator

`ZrCore_State_MutatorLaunch` 将 secondary state 作为 attached-domain mutator 启动。跨 domain
传递值前，必须满足 ZR 的 `Send`/`Sync`、transfer/clone 和 provider contract；一个 raw pointer
不因 `MutatorLaunch` 自动变为共享安全。

典型正确顺序：

```text
producer state: establish owned/transfer-safe value
  -> scheduler/domain bridge validates contract
  -> receiver state materializes value in its domain
  -> each side releases its own owner/root at its own safe boundary
```

不要将 source state 的 `SZrTypeValue *`、`SZrObject *` 或 `SZrString *` 放入 OS thread queue；
这些通常是 frame/object 内部地址。任务/线程 provider 的正式 API 和约束见
[Task API](../03-modules/task-api.md) 与 [Thread API](../03-modules/thread-api.md)。

## 6. 异常 API：受控捕获而不是 C 异常替代品

### 6.1 捕获外层 C 入口

```c
static void host_try_body(SZrState *state, TZrPtr userData) {
    HostRunArgs *args = userData;
    args->ok = run_bound_operation(state, args);
}

EZrThreadStatus status = ZrCore_Exception_TryRun(state, host_try_body, &args);
if (ZrCore_Exception_IsStausError(status)) {
    ZrCore_Exception_LogUnhandled(state, &state->currentException);
    ZrCore_Exception_ClearCurrent(state);
    return ZR_FALSE;
}
```

`TryRun` 为给定 state 建立 runtime recovery point，并返回 `EZrThreadStatus`。它是 C host 调用
可能触发 ZR 异常的操作时的边界。不要在同一 callback 中用 `setjmp`/`longjmp` 绕开它，因为那会
遗漏 VM frame、root、ownership 和 `finally` 清理。

### 6.2 API 分类

| API | 用途 | 调用后的责任 |
| --- | --- | --- |
| `TryRun` | 运行 C try body 并捕获 runtime throw | 检查 status 和 `currentException` |
| `Throw` | 以 thread status 发起 runtime throw | 不应继续依赖普通后续执行 |
| `TryStop` | 在指定栈级别停止/恢复控制 | runtime exception machinery 用 |
| `MarkError` | 写入 error/status/previous stack top | instruction/runtime helper 用 |
| `NormalizeThrownValue` | 将 ZR payload 归一为 exception | payload 必须在调用期间有效 |
| `NormalizeStatus` | 将 status 转为 exception 表示 | 适合 native/loader 错误桥接 |
| `RaiseNamedRuntimeError` | 用 prototype 名与消息生成具名 runtime error | 名称必须是已知/可解析 error prototype |
| `CatchMatchesTypeName` | 判断 catch 类型 | typeName 是 managed string 借用 |
| `ClearCurrent` | 清当前 exception | 只在已消费/记录异常后调用 |
| `PrintUnhandled` / `LogUnhandled` | 输出未处理异常 | 不改变异常状态 |

`ClearCurrent` 不能被用来吞掉任意失败后继续运行。只有在 host 已决定把异常转换成自己的错误结果、
并且 VM stack/control 已由 `TryRun` 边界恢复后才调用。否则旧 call frame、pending control 或
resource cleanup 可能尚未完成。

## 7. budget 终止如何与异常/GC 协作

`SZrExecutionBudget.termination` 是 sticky 状态，枚举包括 instruction limit、deadline、
cancelled、heap limit、native call limit 和 GC time limit。`Poll` 在 dispatch 前检查；native
poll 不消耗 instruction；`NativeEnter` 在 native 边界计数；`GcBegin/GcEnd` 统计 GC 时间。

这意味着宿主不应把取消实现为异步 free 或强制 longjmp。推荐策略：

1. 取消端调用 `ExecutionCancelToken_Cancel`；
2. VM 或合作式 native code 在下一个 poll/entry/safepoint 观察 termination；
3. 运行时正常 unwind，执行 `using`/finally/ownership cleanup；
4. host 在 `TryRun` 或高层 run 返回后读取结果并销毁资源。

heap limit 同样是边界检测：allocator accounting 包含 VM allocator 的 requested bytes，单次分配
或合作式 native call 在下一边界前可能超出少量额度。安全策略应把上限视为终止阈值，不是硬件式
瞬时配额。

## 8. 失败路径模板

```c
static TZrBool native_work(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrGcNativeCallPin pin = {0};
    TZrBool pinned = ZR_FALSE;

    if (!ZrCore_ExecutionBudget_NativeEnter(context->state, ZR_TRUE)) {
        return ZR_FALSE;
    }
    if (!ZrCore_Gc_NativeCallPinValue(context->state, context->selfValue, &pin)) {
        return ZR_FALSE;
    }
    pinned = ZR_TRUE;

    if (!foreign_operation()) {
        ZrCore_Exception_NormalizeStatus(context->state, ZR_THREAD_STATUS_RUNTIME_ERROR);
        goto fail;
    }

    ZrCore_Value_ResetAsNull(result);
    if (pinned) {
        ZrCore_Gc_NativeCallUnpin(context->state->global, &pin);
    }
    return ZR_TRUE;

fail:
    if (pinned) {
        ZrCore_Gc_NativeCallUnpin(context->state->global, &pin);
    }
    return ZR_FALSE;
}
```

示例中的 status 仅表示模式：实际 native 模块应使用与 provider 定义一致的 named error 或现有
callback helper，且确认 `selfValue` 确实非空。重要的是每个 acquisition 都有单一路径的 release，
并让 runtime 而不是 native 代码决定最终 unwind。

## 9. 诊断检查表

- 使用了 managed 指针后，下一次分配/回调/GC 前是否 root 或 pin？
- 向既存 object 写入 managed value 时是否走 checked setter 或 barrier？
- 所有 `NativeCallPin*` 是否在成功、失败、异常返回前 `Unpin`？
- 所有 `TryRun` 是否检查了返回 status，并只在恢复后 clear exception？
- 是否避免在 log/finalizer/GC callback 中重入同一 state？
- 多线程路径是否传递 owner-safe payload，而非 frame/object 裸指针？
- 取消路径是否等待 unwind，而不是直接释放 state/global？

## 10. 相关页面

- [Core 宿主状态、执行与预算 C API](core-runtime-host-api.md)
- [Core 值、字符串、对象与模块 C API](core-value-object-api.md)
- [C 值与 GC 生命周期](c-api-value-lifecycle.md)
- [Native Callback C 实战配方](native-callback-recipes.md)
- [异常、诊断与失败传播](../02-language/error-handling.md)
