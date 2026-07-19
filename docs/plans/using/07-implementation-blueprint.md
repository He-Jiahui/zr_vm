# Using 07：实施路线

## Milestones

1. **U1 foundation**：Canonical owner/ref TypeRef、Place/CFG availability/loan/drop facts和artifact schema。
2. **U2 resource/owner**：resource class、own、Unique/Shared/Weak、Drop与GC bridge。
3. **U3 union/pattern**：variant layout、if let/switch refinement与exhaustiveness。
4. **U4 loader split**：loadModule/loadPlugin result、ModuleIdentity与capability。
5. **U5 Close protocol**：metadata、cleanup CFG、异常组合；此后才冻结UsingStatementSyntax。
6. **U6 consumers**：VM/AOT/LSP/reflection/debug/artifact parity。
7. **U7 migration**：AST-aware edits、fixture/doc/formatter收口并删除旧执行路径。

## 验收顺序

每个milestone先跑canonical type/dataflow leaf tests，再跑parser/core/module，再跑project四backend与LSP golden。异常、nested scope、branch/loop、partial construction和GC compaction必须是第一等测试，不作为后续补充。

## 完成记录规则

已有runtime/metadata/union/plugin证据分别位于`01-ownership/`、`03-metadata/`、`04-unions/`、`05-migration/`。新工作每完成一个可独立复用contract，在对应目录新增`YYYY-MM-DD-<detail>.md`，记录输入版本、命令、结果和仍open的边界；禁止把每日微步骤继续堆进本文件。

## 依赖与晋级账本

| Milestone | 硬依赖 | 产物 | Promotion gate |
|---|---|---|---|
| U1 foundation | syntax 01-03 | owner/ref TypeIds、Place/CFG facts、artifact schema | parser/dataflow/query roundtrip |
| U2 resource/owner | U1 + syntax 04 | own/Unique/Shared/Weak/Drop/GC bridge | normal+abrupt+GC+thread matrix |
| U3 pattern | U1 + union layout | if-let/switch SemIR与exhaustiveness | payload move/drop与四backend |
| U4 loader | syntax 10 ModuleIdentity | loadModule/loadPlugin result/capability | package/export/version/failure matrix |
| U5 Close | U1/U2 + protocol metadata | cleanup CFG与exception policy | 所有退出边；surface仍可pending |
| U6 consumers | U2-U5 | VM/AOT/LSP/reflection/debug parity | source/binary/artifact golden |
| U7 migration | U6 + syntax 06 | AST edits、docs/fixtures、legacy removal | idempotence与allowlist scan |

每个U阶段必须先给出RED证据、最小共享实现、leaf→parent→project命令与结果、未运行范围和明确non-claim。U5 semantic contract完成不自动意味着UsingStatementSyntax已经冻结。
