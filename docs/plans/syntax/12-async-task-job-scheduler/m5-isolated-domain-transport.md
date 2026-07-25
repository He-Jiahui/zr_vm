# Syntax 12 M5: IsolatedDomain Transport

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 12:10 +08:00
- 完成时间：2026-07-25 14:46 +08:00
- 完成项目：
  - 已建立 M5 的实现计划：以 canonical `SZrOwnershipTransferEnvelope`
    接管 IsolatedDomain 的 request/result 传输，不沿用 legacy
    `Transfer.taken` 后再 decode 的路径。
  - 已确认 M5 依赖 Syntax 04 的 `DomainTransferKind`、quota 与
    `Prepare -> Publish -> Claim -> Commit/Abort` 生命周期，并在 provider
    bridge 中只消费该 canonical 生命周期。
  - 已完成 host-only provider policy：`AttachedDomain` 保持默认，
    `IsolatedDomain` 在 `ThreadScheduler` 构造时固化，不新增 ZR 源码
    关键字、构造器或类型分支。
  - 已完成 caller-domain Task provider bridge：prepared callable 与
    external completion 均只通过 `ZrLibraryTaskRuntimeWorkItem` 的 caller
    root 访问，不暴露 Job 私有字段给 worker。
  - 已完成第一段隔离执行链：worker 先公布 target `GcDomainIdentity`，
    caller 才 prepare/publish request envelope；worker reload callable
    artifact，并以独立 result envelope 回到 caller-side claim/commit。
  - GCC focused RED/GREEN：零捕获 Job、scalar closure capture，以及字符串
    `StructuredClone` capture/result、多请求 completion queue 和受限 quota
    faulted Task 均已通过；线程目标为 33 项测试、9 项既有
    TaskRunner/isolate 基线失败，未作为 M5 通过证据。
  - 已收紧跨域 envelope 失败闭环：worker 只在其 target domain abort 已
    claim 的 request/capture；未 claim 的 caller-origin envelope 由
    caller completion queue abort；result envelope 则依 claim 状态由
    caller target 或 worker source 进入唯一终态后释放。
  - 已新增 host-only transfer quota，并在 `ThreadScheduler` 构造时快照；
    1-byte StructuredClone quota 会在 source Job 已消费后令既有 caller
    `Task` fault，而不是让 `schedule` 丢失该 Task；host shutdown 后的
    后续 submission 同样返回 faulted Task。
  - 已完成受限 IsolatedDomain provider FIFO：`workerCount` 现在约束 live
    worker 数，尚未取得容量的 prepared Job 只保留 caller-domain work item
    与可重载 artifact；每项在 worker 发布目标 `GcDomainIdentity` 后才
    prepare/publish request/capture envelope。完成或 fault 的 caller
    acknowledgement 释放容量并驱动下一项，避免 worker-domain object 或
    envelope target 被缓存到 caller queue。
  - 已将单 worker 的 submission-order 用例切换到 `IsolatedDomain`，验证
    两个 Job 经受限 FIFO 依次完成；MSVC Debug focused build 成功，M5
    isolated 用例均通过。37-test 进程仍以相同 9 项既有
    TaskRunner/isolate 基线失败退出 9，未作为 M5 通过证据。
  - 已新增 host-only queued-shutdown 回归：第二个 Job 已进入受限
    IsolatedDomain provider FIFO 后，host shutdown 会使它的 caller-domain
    Task fault；该用例不新增任何 ZR 源码 API。MSVC Debug focused build
    成功，37-test target 的全部 M5 用例通过，进程仍只因相同 9 项既有
    TaskRunner/isolate 基线失败而退出 9，未将其计为里程碑通过证据。
  - 已新增 forbidden resource capture 回归：`Unique<Counter>` 闭包可以
    形成既有 `Job<int>`，但 IsolatedDomain schedule 在缺少 canonical
    ResourceMove provider projection 时由 envelope contract fault 该 Task。
    该行为不以 runtime value class 假装 ResourceMove，并保持 Job 的既有
    调用即消费规则。
  - 已新增 shared provider queue 回归：两个 worker 对四个独立 Job 从同一
    FIFO 取件，前两个立即启动，后两个在 caller acknowledgement 释放容量后
    继续完成；队列不含 worker-domain object、GC root 或 target envelope。
  - 已将 IsolatedDomain 实现从 `runtime_workers.c` 提取到
    `runtime_isolated_domain.c`；AttachedDomain worker 与可复用 artifact
    helper 留在原模块，隔离 request/result envelope、FIFO、完成确认与
    shutdown 都在独立模块，避免单一 runtime worker 文件继续膨胀。
  - 已在同一 `HEAD + M5 overlay` 隔离快照重建并重放线程目标：GCC、Clang
    与 MSVC 都是 `37 tests / 9 known baseline failures`，所有 10 个
    IsolatedDomain M5 断言通过。三个真实进程退出码均为 `9`；该非零退出
    不被当作全目标通过，原因是三套工具链均保留同一组 9 个既有
    TaskRunner/isolate 测试失败。
  - 已在同一快照重放 canonical transfer 下层回归：GCC、Clang 与 MSVC 的
    `zr_vm_resource_cross_domain_transfer_test` 均为 24/24，
    `zr_vm_resource_cross_domain_transfer_race_test` 均为 5/5，真实进程
    退出码都是 `0`。这些用例覆盖 provider prepare/commit/decode/allocation
    failure 的唯一终态和 queued/claimed claim-abort 单赢家；M5 只消费该
    lifecycle，不在 thread runtime 重建它。
  - 已确认 ResourceMove/ImmutableHandle provider 仍从 canonical type-layout
    contract 提供。当前 scheduler runtime 只接收 `SZrTypeValue`，没有可解析
    provider 的 layout metadata 时一律发布 `FORBIDDEN`，令已消费 Job 的
    caller Task fault；它不会按 runtime value 类别、类型名或裸指针猜测
    provider。下层 24/24 provider 证据覆盖该 contract 的资源与 immutable
    分支，线程层覆盖该 metadata 不可用时的安全拒绝边界。
  - 完整验收、精确日志位置与 9 项基线失败名称见
    `tests/acceptance/2026-07-25-syntax-12-m5-isolated-domain-transport.md`。

## 产出位置

- 实施计划：`docs/plans/syntax/12-async-task-job-scheduler/m5-isolated-domain-transport-implementation-plan.md`
