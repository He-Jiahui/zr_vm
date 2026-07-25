# Syntax 12 M6.1b.2b：Scheduler Artifact Writer Integration

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 17:40 +08:00
- 完成时间：2026-07-25 21:55 +08:00
- 完成项目：
  - 已以真实 `zr.thread.ThreadScheduler.schedule` source 编译用例固定
    source -> `.zro` writer -> canonical import 的 RED；writer API 缺失时 GCC
    在链接阶段明确失败，不以手工 fixture 或旧 `.zrb` 作为 producer。
  - compiler source fact 现保留 Scheduler、Task、Job 的精确 provider
    TypeDef/signature/layout/module identity。native descriptor provider 由
    canonical import module/prototype 身份导出，closed generic provider 保留
    open prototype 的 module provenance；writer 不再查询或猜测 metadata
    record。
  - 新增 `ZrParser_Writer_WriteSchedulerArtifactFile`，以
    `ZrCore_Artifact_Write` 写入真实 `.zro` 的 TypeRef/TypeDef、scheduler
    contract 与 Job `RESOURCE_MOVE`/`DROP_ON_FAILURE` domain-transfer section；
    legacy `.zrb` writer 未被调用或重命名。
  - source roundtrip 测试证明 source/import scheduler contract hash 相等，且
    ABI、policy、requirements、transport hash、scheduler contract hash 和
    provider token 不匹配均由 canonical consumer 拒绝；无 scheduler source
    fact 或不完整 provider identity 时 writer 返回明确失败。
  - RED：writer API 缺失时真实 source roundtrip 在 GCC 链接阶段因
    `ZrParser_Writer_WriteSchedulerArtifactFile` 未定义而失败。
  - GREEN：固定 `e04719a + M6.1b.2b overlay` 快照中，GCC、Clang、MSVC
    分别全新构建并实际运行 `zr_vm_artifact_schema_test` 21/21 与
    `zr_vm_canonical_consumers_test` 17/17，六个测试进程全部 exit 0。日志：
    `.codex/logs/s12m6b2b-final-{gcc,clang,msvc}-*-run.log`。

## 不变边界

- 仅输出真实 `.zri`/`.zro` canonical artifact；`writer_binary.c` 的 legacy
  `.zrb` VM stream 不参与本阶段 producer 证据。
- writer 不按类型/成员名称、display text、AST 文本、runtime value 分类或
  function metadata row 回退；provider identity、layout、module hash 任一
  不完整时返回明确失败。
- Scheduler policy、Send/Sync requirement、transport hash 和 domain-transfer
  provider contract 都从 compiler-owned source fact 投影。
