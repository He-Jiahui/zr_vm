# Syntax 12 M6: Artifact, Debug, LSP, and Migration

## 状态与产出记录

- 状态：`in_progress`
- 开始时间：2026-07-25 15:00 +08:00
- 完成时间：待定
- 完成项目：
  - 已建立 M6 实施计划，按 artifact contract、legacy runtime migration、
    debug projection 和 LSP projection 四个可独立验收、独立提交的子阶段
    推进。
  - 已确认 `SZrArtifactDomainTransferRow` 与
    `ZrCore_CanonicalConsumer_ResolveDomainTransfer` 已存在，但当前只由
    fixture 覆盖；M6.1 将把它接到真实 source -> compiler -> binary artifact
    -> canonical import 链路。
  - 已冻结规则：缺少 canonical provider/type-layout/module identity 时必须
    以 `FORBIDDEN` 或明确失败收口，不能用 Task 名称、runtime value 类别、
    裸指针、display text 或 AST 文本推断 ResourceMove、ImmutableHandle、
    Send、Sync 或 scheduler policy。
  - 已将 LSP 修改延后到 M6.4；M6.1-M6.3 不触碰 LSP 源码、测试或文档路径，
    先发布可由 LSP 消费的 canonical artifact/runtime facts。
  - M6.1a 已完成：发布 schema-v3 的固定 48-byte scheduler contract row，
    按 scheduler TypeDef token 解析，并对 policy、Send/Sync requirement、ABI、
    transport hash 和 scheduler hash 不匹配发布结构化拒绝；没有名称、文本或
    runtime value fallback。GCC、Clang、MSVC 上的 artifact schema 15/15 与
    canonical consumers 17/17 均为真实 exit 0。
  - M6.1b.1 已完成：scheduler provider identity 现严格接受 local TypeDef
    或 imported TypeRef，并在返回 scheduler contract 前通过对应 type table
    解析；TypeSpec/member token 仍被拒绝。GCC、Clang、MSVC 上的 artifact
    schema 15/15 与 canonical consumers 17/17 均为真实 exit 0。该阶段只收口
    imported identity 前置条件，不声明 source artifact writer 已完成。
  - M6.1b.2 仍负责唯一的 source -> compiler -> binary artifact producer
    链路；当前 M6 总状态不因 M6.1a/M6.1b.1 完成而提前关闭。
  - M6.1b.2 已按事实依赖拆分：M6.1b.2a 先持久化真实 source 中已解析的
    scheduler receiver-call fact，M6.1b.2b 仅在该 fact 已关联 exact TypeDef/
    TypeRef 后才写入 `.zri`/`.zro` artifact。两者均不能将 legacy `.zrb`
    stream 或 hand-built row 作为 producer 证据。
  - M6.1b preflight 已确认：生产 `writer_binary.c` 仍只写旧 `.zrb` VM
    函数流，仓库中 `ZrCore_Artifact_Write` 的生产调用者为零。下一步先补
    compiler-owned type/provider artifact identity，再接入实际 artifact 文件
    writer，不能把旧 writer 或测试 fixture 当作 producer。
  - M6.1b.2a 已完成：真实 `zr.thread.ThreadScheduler.schedule` receiver
    call 现在在编译函数上发布去重的 canonical scheduler source fact，包含
    scheduler `TypeId`、精确 member/signature token、signature hash、protocol
    mask 和 contract role。native descriptor 的类型方法也获得确定性的 member
    identity；身份不完整或非 scheduler call 仍保持 unavailable。该阶段不写
    artifact 文件，不把 `.zrb` 或 fixture 当作 source-produced artifact。
    GCC、Clang、MSVC 的 artifact schema 均为 18/18，canonical consumers
    均为 17/17，所有测试进程真实 exit 0。
  - M6.1b.2b 已完成：`ZrParser_Writer_WriteSchedulerArtifactFile` 从完整的
    compiler-owned Scheduler/Task/Job provider facts 投影真实 `.zro`
    TypeRef/TypeDef、scheduler contract 和 Job ResourceMove/DropOnFailure
    domain-transfer section，再由 `ZrCore_CanonicalConsumer` 导入。writer 不
    使用 legacy `.zrb`、metadata/name/text fallback；source/import contract
    hash 一致，ABI、policy、requirement、transport、contract 与 provider
    token 不匹配均被拒绝，unavailable provider 也明确拒绝。固定
    `e04719a + M6.1b.2b overlay` 的 GCC、Clang、MSVC 各通过 artifact schema
    21/21 和 canonical consumers 17/17，六个进程真实 exit 0；完成时间为
    2026-07-25 21:55 +08:00。
  - M6.2 baseline（仅定位，不作为通过证据）：GCC
    `zr_vm_task_runtime_test` 当前为 54 tests / 6 failures / raw exit 6；
    失败包含 TaskRunner/start/pump/defaultScheduler 旧表面，以及两项既有
    borrowed-value/import 回归，后续必须按 root cause 分片处理。

## 产出位置

- 实施计划：`docs/plans/syntax/12-async-task-job-scheduler/m6-artifact-debug-lsp-migration-implementation-plan.md`
