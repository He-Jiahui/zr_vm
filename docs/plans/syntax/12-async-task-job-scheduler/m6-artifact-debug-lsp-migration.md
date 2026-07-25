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
  - M6.1b 尚未开始，仍负责唯一的 source -> compiler -> binary artifact
    producer 链路；当前 M6 总状态不因 M6.1a 完成而提前关闭。

## 产出位置

- 实施计划：`docs/plans/syntax/12-async-task-job-scheduler/m6-artifact-debug-lsp-migration-implementation-plan.md`
