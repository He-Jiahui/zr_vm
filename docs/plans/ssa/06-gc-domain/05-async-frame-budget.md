---
related_code:
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_core/include/zr_vm_core/execution_budget.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_core/include/zr_vm_core/execution_budget.h
  - zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c
  - zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/task/test_ssa_async_frame_budget.c
  - tests/task/test_task_runtime.c
  - tests/task/test_task_job_scheduler.c
  - tests/task/test_task_frame_runtime.c
  - tests/acceptance/2026-09-29-ssa-async-frame-gc-root-drop.md
  - tests/acceptance/2026-09-29-ssa-async-frame-budget-ndebug-tests.md
doc_type: milestone-detail
status: planned
---

# 06.05 协程等待、后台编译与帧预算

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 让锁等待、编译和预算耗尽在允许挂起的边界交还调度权，减少游戏帧线程阻塞。

**Architecture：** 同步与异步 API 明确分离；ExecIR maySuspend 与 task frame map 决定可挂起点，编译 worker 使用不可变输入快照并在 generation 边界提交结果。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2 frame-safe。本任务是 M7 异步后端服务（10.01）的前置输入，不等待 M7 交付。
- 前置：[06.02 并发 Major、GC 预算与移动边界](../06-gc-domain/02-major-budget.md)；[06.03 同域多 Worker 与自动 Send/Sync](../06-gc-domain/03-domain-sharing.md)；[01.04 Ownership、挂起与状态恢复映射](../01-execir-ssa/04-state-maps.md)。
- 交付：非阻塞等待状态机、执行预算 poll、后台编译取消与加载预热。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

task_runtime/task_frame_runtime 以及当前工作区 execution_budget/session_checkpoint 已有相关工作；实施前复用其协议，不能新增竞争的 budget/continuation 所有者。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/task_frame_runtime.c` | 挂起/恢复和 cleanup |
| 现有，修改/复用 | `zr_vm_library/src/zr_vm_library/task_runtime.c` | 调度/取消接缝 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_budget.c` | 复用现有预算检查，确认提交状态 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/execution_budget.h` | 统一 host 可见预算合同 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_async_wait.c` | 锁/资源的 await 适配 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_compile_queue.c` | 后台编译快照、取消、完成队列 |
| 计划新增测试 | `tests/task/test_ssa_async_frame_budget.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 划分允许挂起区域** 只有 async/task contract 允许挂起；持有不可跨 await 借用、栈别名或非挂起锁 guard 时禁止 yield。同步锁 API 保留同步语义或显式 offload。

- [ ] **2. 实现无丢唤醒等待** 先登记 waiter，再重检条件；wake/cancel/timeout 通过单一原子状态竞争取得 continuation，恰好恢复一次，避免 lost wakeup。

- [ ] **3. 布置预算 poll** 长循环 backedge 和 call 边界按有界工作量 poll；预算耗尽保存 state map，不能切在半写回/半搬迁/native 不可中断区。

- [ ] **4. 后台编译与预热** 编译输入快照不引用可移动 AST/VM 内存；取消或 generation 变化使完成结果失效，旧代码继续运行；host 在加载期主动发 warm-up。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
WaitState = Registering -> Waiting -> {Ready, Cancelled, TimedOut} -> Resumed
registerWaiter(); recheckCondition(); CAS to winnerState
resumeContinuationExactlyOnce()
CompileJob = {immutableIR, contractHashes, requestedGeneration, cancellation}
completeJob only if hashesAndGenerationStillMatch
frameThread never waits synchronously for compile completion
```

自动优化不能把同步 API 悄悄变成异步。预算到达是正常挂起状态，不等于异常，更不能中断任意 native 指令。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| wait 与 wake/cancel 同时发生 | 恰好一次恢复，无丢唤醒 |
| 持 ref-like/锁 guard 到 await | 编译拒绝或明确不挂起 |
| 编译期间 module reload | 过期结果丢弃 |
| 长 hot loop、不可中断 native | loop 可让出；native 记录超预算来源 |
| async frame GC-root 表扩容失败 | 已复制的 slot 仍运行已注册 drop；后续 task/pool cleanup 不重复 drop |
| focused test 编译时定义 `NDEBUG` | frame/wait/queue 操作和检查仍执行；仅清理已初始化且无活动 handle/worker lease 的对象 |

复用回归入口：`tests/task/test_task_runtime.c`、`tests/task/test_task_job_scheduler.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_async_frame_budget` 和可执行目标 `zr_vm_ssa_async_frame_budget_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_async_frame_budget_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_async_frame_budget$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

### 已有 Task Frame slot 的根注册失败回滚

`tests/task/test_task_frame_runtime.c` 另行覆盖现有 frame runtime 的一个
资源失败边界：先注入一次根表 `ARRAY` 扩容失败填满域根表，再在真实
挂起 task 的 `StoreSlot` 中拒绝下一次扩容。StoreSlot 已复制带 drop 的
GC 值但尚未取得 root handle 时，失败路径必须调用常规 slot cleanup，
让 drop 先观察该值，再释放其所有权并清除 initialized 状态。之后重复
`Task_Free`/`Pool_Free` 不应重复调用 drop。该回归补充现有 frame cleanup
契约，不表示本计划的 waiter、预算或后台编译里程碑已完成；对应的
RED 与 MSVC 验收记录见
[`2026-09-29-ssa-async-frame-gc-root-drop.md`](../../../../tests/acceptance/2026-09-29-ssa-async-frame-gc-root-drop.md)。
最终源的 MSVC Debug direct run 为 7/7；CTest 查询未发现对应注册项，且
没有执行 `NDEBUG` 构建。具体命令和验证边界记录在上述 acceptance 中。

### Focused frame-budget harness checks under `NDEBUG`

`tests/task/test_ssa_async_frame_budget.c` no longer uses the standard C
`assert` macro for either state transitions or expectations. Its `TEST_CHECK`
always evaluates its condition, records failures, and jumps to the owning test's
cleanup. Wait-registry and compile-queue initialization calls run before their
results are checked; initialized flags guard `Deinit`, handle flags guard
`Release`, and the compile worker lease remains tracked until `Complete`
acknowledges it. Cleanup skips deinitialization while any tracked handle or
worker still owns state.

A safe pre-change canary guarded its cleanup with an initialization flag:
Debug printed `init_calls=1 initialized=1 deinit_calls=1`, whereas `-DNDEBUG`
printed `init_calls=0 initialized=0 deinit_calls=0`. It did not call `Deinit`
on an uninitialized object. After the test-only fix, strict GCC 4.8.3
standalone Debug and `-DNDEBUG` builds both pass the focused executable; this
does not claim a full CMake Release configuration. Temporary `NDEBUG` probes
also verified the failed-init and active-worker cleanup branches. Exact
commands and the root-owned MSVC evidence are recorded in
[`2026-09-29-ssa-async-frame-budget-ndebug-tests.md`](../../../../tests/acceptance/2026-09-29-ssa-async-frame-budget-ndebug-tests.md).
The final MSVC Debug target build and direct executable both passed; the direct
run invokes 11 source-level test functions. The registered
`ssa_async_frame_budget` CTest passed 1/1. These focused checks do not complete
the broader async frame-budget milestone, whose status remains `planned`.

**退出门禁：** p95/p99 与最长不可中断段可见；取消/超时/race 无泄漏或死锁，不能以仅更换 worker 线程声称帧预算已满足。

**失败恢复：** 关闭后台优化不影响基线执行；资源不足返回排队/取消状态，不阻塞帧线程等待。

**文档交付：** 新增 docs/core-runtime/frame-safe-scheduling.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrAsyncWaitRequest SZrAsyncWaitRequest;
typedef struct SZrCompileQueueRequest SZrCompileQueueRequest;
TZrBool ZrCore_Execution_BeginAsyncWait(const SZrAsyncWaitRequest *request,
    SZrDomainDiagnostic *diagnostic);
TZrBool ZrCore_Execution_QueueCompilation(const SZrCompileQueueRequest *request,
    SZrDomainDiagnostic *diagnostic);
/* SZrDomainDiagnostic 为 06.03 定义的 core 运行期诊断类型 */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| suspend eligibility | effect/borrow/state-map builder | scheduler 不在非法位置抢占 |
| waiter winner state | 原子 wait registry | wake/cancel/timeout 恰好一个胜者 |
| compile immutable input | 编译请求创建方 | 后台线程不碰可移动 AST/VM object |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 复用现有 execution_budget/session_checkpoint 协议，补状态而不新造重复 continuation。

- [ ] **批次 2：** 实现 waiter register/recheck/CAS 与取消清理；用可控调度覆盖丢唤醒窗口。

- [ ] **批次 3：** 接后台编译队列、预热、过期结果丢弃，测量 frame p99 及不可中断 native。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange wake between waiter registration and condition recheck
assert no lost wakeup
arrange cancel and wake concurrently
assert continuation resumes exactly once
arrange generation changes before compile completion
assert result disposed without entry publication
```

### 迁移结束检查

同步锁变 await 只能发生在已声明异步协议中，不能作为透明优化改变用户 API。编译 pending 与执行预算挂起使用不同状态码。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
