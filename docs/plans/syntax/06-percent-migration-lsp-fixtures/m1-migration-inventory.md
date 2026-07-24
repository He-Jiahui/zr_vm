# 06A-M1 Migration Inventory 里程碑记录

对应设计：`docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md` 的
`06A / M1 Migration inventory`。

执行计划：
`docs/plans/syntax/06-percent-migration-lsp-fixtures/m1-migration-inventory-implementation-plan.md`。

## 状态与产出记录

- 完成时间：2026-07-24 12:18 +08:00
- 状态：completed
- 完成项目：
  - 已确认 Syntax 01-05 的全部 33 个前置里程碑记录为完成，满足 06A 的下游启动前提。
  - 已建立 M1 的只读 inventory 边界：分类所有 current ZR source、project fixture、embedded
    ZR test source 和 current documentation snippet；对历史、迁移负例、生成、二进制和第三方目录
    显式记录 exclusion，禁止裸文本替换或 source rewrite。
  - 已为 `machineApplicable`、`maybeIncorrect`、`requiresReview`、`blocked` 与
    `targetNotPromoted` 设定稳定 report 合同；08、10、11、12、13、14 未晋级目标只能记录为
    `targetNotPromoted`，不得被计作可应用 edit。
  - 已完成 synthetic fixture 的词法与分类 matrix：`python tests/scripts/test_syntax_migration_inventory.py`
    当前 5/5 通过；golden 固定 35 个 legacy form contract，未识别 `%` directive 必须显式为
    `blocked`。
  - 已完成全仓 baseline：811 个 scanned、336 个 explicit exclusion、770 个 finding，
    `unknownCount=0`、`blocked=0`；target-not-promoted 项保持不可发布。
  - 已完成 Windows Python 3.14、WSL GCC 11.4/Python 3.10 与 WSL Clang 14.0/Python 3.10
    的 focused validation，三份 raw JSON SHA-256 均为
    `23397b06e154a009e2db276f7f2ae8917b4c088e75066e17851e8e4a106657e2`。
  - 已完成 exact-path audit：14 个 M1 paths，`git diff --cached --check` 和 working-tree
    `git diff --check` 均通过；三份 existing Syntax design drafts、generated fixture binary 和
    `.codex` 临时证据均未被暂存。

## 当前实现边界

- M1 只构建 inventory/report/schema/fixture/documentation，不调用或替换正式 parser/compiler/CLI
  路径，不生成 source edit，也不发布 LSP action。
- 注释、字符串、`%` 取模、普通 `$`、历史 plan、migration/legacy negative fixture 与生成/二进制
  输入必须可解释地排除；无法识别的 legacy-like syntax 记录为 `blocked`，而不是 `unknown`。
- M2 才实现 edit planner、JSON/text migration report 的 edit 载荷与 LSP workspace edit；M3 才运行
  全仓 dry-run；06B 仍受 08、10-14 的 promotion gate 约束。

## 验收结论

- 已完成 06A M1 Migration Inventory。M2 的 edit planner、JSON/text edit payload 和 LSP workspace
  edit 尚未开始；M3 的 `--check` dry run 也尚未开始；06B 仍受 08、10、11、12、13、14 的 promotion gate
  约束。本里程碑没有修改任何被盘点的 ZR source，也没有批准 source rewrite。
