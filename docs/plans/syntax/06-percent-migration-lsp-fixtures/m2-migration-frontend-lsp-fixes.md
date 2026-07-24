# 06A-M2 Migration Frontend + LSP Fixes 里程碑记录

对应设计：`docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md` 的
`06A / M2 Migration frontend + LSP fixes`。

执行计划：
`docs/plans/syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes-implementation-plan.md`。

## 状态与产出记录

- 状态：completed
- 完成时间：2026-07-24 18:09 +08:00
- 完成项目：
  - 提供 parser-owned `SZrLegacyMigrationPlan` API：token-aware lexical boundary、稳定排序、source hash、
    overlap rejection，以及从右至左的 idempotent machine-edit application。
  - 交付 `zr_vm_cli migrate syntax <path> --check|--write --format json|text`。它递归处理 `.zr` 文件、默认
    排除 `bin`/`golden`/`generated`/`.codex`，`--include-generated` 才允许后者，并在 write 前做 source
    re-read、parser/compiler validation 与 atomic replace。
  - 冻结 M2 safety gate：只有 `%owned -> resource` 具备 current parser/compiler witness 并可写入；`%module`
    报告为 `targetNotPromoted/06B`；ownership call、passing mode、callable、constructor、import 与 property
    family均保持不可写的结构化事实。
  - 保持 LSP non-cutover boundary：不向当前 document 注入 migration diagnostic；现有 structured-fix、
    revision-guarded code-action 和 workspace-edit snapshot consumer 通过回归。为 Windows DLL consumer 补齐
    document-aware range/position helper export，恢复同一 interface test 的 MSVC 链接。
  - 修复 Windows directory walker 对 `generated` 等排除目录的单反斜杠识别，并使 CLI migration CTest 在
    Windows 使用显式 `cmd.exe` quoting 执行带引号的 CLI 路径。
  - 新增 parser/CLI/module docs/acceptance evidence；GCC 11.4、Clang 14.0.0、MSVC 19.44 同一 focused
    matrix 均通过。

## 当前实现边界

- M2 只提供 legacy migration adapter、语义 edit plan、`zr migrate syntax` 的 JSON/text report 与
  idempotent machine edit。现阶段唯一 current-parser/compiler witness 是 `%owned -> resource`；其余
  legacy family 保持完整 report 但不可写入。
- M2 不切换正式 compiler/parser 的接受语法、不改写全仓 source、fixture、golden、artifact 或
  documentation snippet；这些属于 M3/06B。
- 旧 property accessor 继续由已提交的 parser property migration producer 输出同一
  `SZrStructuredDiagnosticFix` 合同，不能被新 scanner 产生重复 diagnostic。
- 06A 不向普通当前 document 注入 migration diagnostic：正式 parser diagnostic 与其 LSP projection
  是 06B/M4 cutover 责任。现有 LSP code-action/workspace-edit snapshot 回归仅证明未来会序列化
  parser-owned structured fix，而不重建文本。

## 验收结论

- 接受。三套隔离构建均通过 `legacy_migration`、`property_consumer_contracts`、`cli_args` 与
  `cli_syntax_migration` CTest，直接 LSP interface test、CLI migration smoke 及 LSP stdio smoke 均为
  real exit 0。详细命令、RED 证据和边界见
  `tests/acceptance/2026-07-24-syntax-06a-m2-migration-frontend-lsp-fixes.md`。
