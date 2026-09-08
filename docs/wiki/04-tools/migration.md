---
related_code:
  - zr_vm_cli/src/zr_vm_cli/migration/migration.h
  - zr_vm_cli/src/zr_vm_cli/migration/migration.c
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c
  - zr_vm_parser/src/zr_vm_parser/migration
  - docs/zr_language_specification.md
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/migration/migration.c
  - zr_vm_cli/src/zr_vm_cli/metadata/zrp_metadata_dump.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/06-percent-migration-lsp-fixtures/m1-migration-inventory.md
tests:
  - tests/cli/test_cli_syntax_migration.c
  - tests/cli/syntax_migration_smoke.js
  - tests/parser/test_legacy_migration.c
  - tests/migration/test_percent_test_migration.c
doc_type: module-detail
---

# 语法迁移与项目元数据

## 迁移命令

```text
zr_vm_cli migrate syntax path.zr --check [--format json|text]
zr_vm_cli migrate syntax path.zr --write [--format json|text]
```

`--check` 只生成迁移计划并报告，不修改文件；`--write` 仅应用
`machineApplicable` edit，写入前重新读取并校验 source hash，应用后再用当前 parser 重解析，
校验通过才原子替换文件。两种模式都必须明确指定其一，不能同时使用。默认输出 JSON；
`--format text` 适合人工查看，`--format json` 适合 CI。迁移器只根据 lexer token、AST 节点
和结构化 diagnostic 生成 edit，不用正则替换。

可选参数如下：

| 选项 | 约束与作用 |
| --- | --- |
| `--include-generated` | 将路径中 `bin/`、`golden/`、`generated/` 和 `.codex/` 下的 `.zr` 纳入扫描；默认跳过。 |
| `--language-from legacy` | 显式确认输入语言为 legacy；其它值被拒绝。 |
| `--language-to current` | 显式确认目标语言为 current；其它值被拒绝。 |
| `--format json\|text` | 选择结构化或文本报告；JSON 默认包含 `schemaVersion`、`sourceHash`、`items`、`edits` 和适用性。 |

无法无歧义转换的 `%module`、`%import`、`%owned`、旧 `func`、`$Type` 和旧 scheduler
surface 会生成 `requiresReview`/`blocked` 等负向诊断，不能静默改写。生产 parser 仍拒绝
legacy token。目录输入会递归扫描 `.zr` 文件；单文件输入必须以 `.zr` 结尾。

## ZRP metadata 工具

`--dump-zrp-metadata` 输出 manifest version、module graph、source/binary/intermediate hash、
provider phase、exports、AOT options 和 preserve rules；`--diff-zrp-metadata before after`
按稳定 key 排序显示新增/删除/变更；`--check-zrp-metadata-version` 只验证 schema/版本，不编译。
这些工具可在 CI 中运行而无需启动 VM。
