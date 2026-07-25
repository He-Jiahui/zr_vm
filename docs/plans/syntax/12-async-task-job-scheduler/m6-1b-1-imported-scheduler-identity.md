# Syntax 12 M6.1b.1: Imported Scheduler Identity

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 15:55 +08:00
- 完成时间：2026-07-25 16:23 +08:00
- 完成项目：
  - 将 scheduler artifact contract 的 provider key 从仅 local `TypeDef`
    放宽为严格的 nominal `TypeDef` 或 imported `TypeRef`；`TypeSpec` 和
    member token 保持非法。
  - `ZrCore_CanonicalConsumer_ResolveSchedulerContract` 先通过 canonical
    type table 解析 key，再返回 scheduler row；缺少 TypeRef row 不会退化为
    名称、source spelling、AST 或 runtime value 推断。
  - TDD RED：旧实现的 GCC artifact schema 为 15 tests / 1 failure，旧
    canonical consumers 为 17 tests / 4 failures，均因 imported TypeRef
    scheduler identity 返回 `ILLEGAL_TOKEN`。
  - GREEN：隔离 GCC、Clang、MSVC 缓存均运行
    `zr_vm_artifact_schema_test` 15/15 和
    `zr_vm_canonical_consumers_test` 17/17，全部真实 process exit 为 0。
  - 未完成项：本记录不创建 compiler scheduler fact，不增加生产
    `ZrCore_Artifact_Write` caller，也不将 legacy `.zrb` VM stream 视为
    source-produced artifact；这些仍由 M6.1b.2 负责。

## 验收证据

| Toolchain | Artifact schema | Canonical consumers | Process exit |
|---|---:|---:|---:|
| GCC 11.4 | 15/15 | 17/17 | 0 / 0 |
| Clang 14 | 15/15 | 17/17 | 0 / 0 |
| MSVC 19.44 | 15/15 | 17/17 | 0 / 0 |

日志位于 `.codex/logs/s12m6b1-*-{artifact,canonical}.log`；各构建目录均为
独立的 `.codex/build-s12m6b1-*`，没有消费共享 build cache。
