---
related_code:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
tests:
  - tests/task/test_task_runtime.c
  - tests/task/test_task_job_scheduler.c
doc_type: module-detail
---

# `zr.task` 详细参考

**状态：`current`；Runtime provider，descriptor 版本 `3.0.0`，公开 contract
`zr.task:v3:task-job-scheduler`。**

## 导出签名

| 导出 | 签名 | 结果与约束 |
| --- | --- | --- |
| `currentScheduler` | `property: Scheduler` | 当前 state 的 cooperative scheduler；只读 |
| `Task<T>.result` | `result(): T` | 等待并取得完成值；fault 会重新抛出原异常 |
| `Task<T>.isCompleted` | `isCompleted(): bool` | completed 或 faulted 时为 `true`，pending 时为 `false` |
| `Job<T>` | `init Job<T>(callable: fn() -> T \| fn() -> Task<T>)` | 创建 cold、non-Copy、只能消费一次的工作值 |
| `Scheduler.schedule` | `schedule<T>(job: Job<T>): Task<T>` | 按值消费 `job`，返回 caller-domain Task |
| `yieldNow` | `yieldNow(): Task<void>` | 产生一个 cooperative turn |
| `delay` | `delay(duration: Duration): Task<void>` | 通过当前 scheduler 延迟完成 |

`delay` 的 descriptor 契约使用 `Duration` 名称；当前 runtime callback 读取非负整数
turn 表示（测试也以整数传入），在独立 Duration provider 落地前应把它视为兼容实现细节，
不要在跨 provider 的公共接口中假定具体单位。

## 类型和状态

`Task<T>` 的逻辑状态为 pending、completed、faulted、cancelled；状态转换只允许一次，
`result()` 对 completed 返回值，对 faulted 重新抛出原异常，对 cancelled 返回终止诊断。
`Job<T>` 只有 ready/consumed 状态；消费后再次提交是 ownership error。`Scheduler` 保存
本地队列和 provider await hook，脚本看不到内部 worker。

## 调度语义

`currentScheduler.schedule(job)` 先建立 caller-domain Task root，再发布完整 request；
发布失败仍保持 Job consumed 并 fault Task。`yieldNow` 让出当前 cooperative turn，
`delay` 将任务放入 provider 的计时等待。`Task.result` 在 scheduler active frame 中不会
递归 pump 自己，避免重入；在外部调用则等待 provider 完成。

## 取消、预算与异常

取消由宿主 execution budget/cancel token 驱动，不会强行中断 native callback。await 传播
原始 exception type 和 stack frame；调度器关闭会把未完成任务转换为 fault/cancelled，且
释放 queue root 和 closure capture。

## C 调用接口

核心只负责把 canonical `Job/Scheduler` surface 注册进 native registry；provider 通过
窄 handoff API 操作 job，而不能读取脚本对象的私有字段：

| C 符号 | 用途 | 生命周期/返回 |
| --- | --- | --- |
| `ZrCore_TaskRuntime_RegisterBuiltins(global)` | 注册 `zr.task` canonical descriptor | `TZrBool`；失败时 registry 保留错误码 |
| `ZrLibrary_TaskRuntime_ScheduleJob(state, scheduler, job, result)` | 一步消费 Job 并创建 caller-domain Task | `TZrBool`；Job 无论排队是否成功都只消费一次 |
| `ZrLibrary_TaskRuntime_PrepareJob(...)` | 为异步 provider 建立 `ZrLibraryTaskRuntimeWorkItem` | `TZrBool`；成功后必须 execute/fault/release 恰好一次 |
| `ZrLibrary_TaskRuntime_ExecutePreparedJob` / `FaultPreparedJob` / `ReleasePreparedJob` | 执行、故障结算和释放 work item | execute 返回 bool；fault/release 无返回值 |
| `ZrLibrary_TaskRuntime_CopyPreparedCallable` / `CompletePreparedJob` | isolated provider 读取 callable、在 caller domain 结算结果 | `TZrBool`；不允许 worker-domain task root |
| `ZrLibrary_TaskRuntime_RegisterAwaitHook` / `AwaitProviderTask` | 注册并调用 provider wait hook | `TZrBool`；`AwaitProviderTask` 通过 `outHandled` 区分是否接管等待 |
| `ZrLibrary_TaskRuntime_IsTaskComplete` | 查询完成或 fault 状态 | `TZrBool` |

`ZrLibraryTaskRuntimeWorkItem.taskRoot` 是 GC root，所有权属于 work item；provider 关闭、
异常或分配失败时仍须走 `FaultPreparedJob` 后再 `ReleasePreparedJob`。上述接口声明于
`zr_vm_library/task_runtime.h`，不是脚本可见的 scheduler pump。
