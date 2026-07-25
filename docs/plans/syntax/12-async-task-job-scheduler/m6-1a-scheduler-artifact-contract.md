# Syntax 12 M6.1a: Scheduler Artifact Contract

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 15:00 +08:00
- 完成时间：2026-07-25 15:44 +08:00
- 完成项目：
  - 将 artifact schema 升级到 v3，新增固定 48-byte
    `SZrArtifactSchedulerContractRow`。该行使用 canonical scheduler/task/job
    tokens、policy support mask、per-policy Send/Sync requirements、ABI version、
    transport contract hash 与 scheduler contract hash，不编码 host 当前选中的
    AttachedDomain/IsolatedDomain 状态。
  - 为 scheduler contract table 完成 input、decoded artifact、section ordering
    与字段合法性校验；非法 token、未知 bit、零 ABI/hash、保留位和无策略的
    requirement 均会被拒绝。
  - 发布 `ZrCore_CanonicalConsumer_ResolveSchedulerContract` 与
    `ZrCore_CanonicalConsumer_ValidateSchedulerContract`。consumer 只按
    scheduler TypeDef token 和结构化 expectation 工作，缺 section、policy、
    requirement、ABI、transport/scheduler hash 不匹配都返回稳定 artifact status，
    不按名称、显示文本、AST 或 runtime value 推断。
  - 新增 schema round-trip 和 canonical consumer 覆盖，并修正 optional
    DomainTransfer fixture 删除逻辑，使其在新增 scheduler section 后仍准确移除
    DomainTransfer section。
  - 验收：独立 GCC、Clang、MSVC 构建均执行
    `zr_vm_artifact_schema_test` 15/15、
    `zr_vm_canonical_consumers_test` 17/17；六个测试进程真实 exit 均为 0。

## 边界

M6.1a 只建立 schema 和 importer contract。真实 source -> compiler -> binary
artifact producer 由 M6.1b 完成；在那之前，这一 row 不得被描述为 source-produced
metadata。
