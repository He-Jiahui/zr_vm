---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic_type_prototypes.c
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
tests:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
  - tests/parser/test_semantic_display.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-31-plan03-task05-primitive-type-use-alias-producer.md
  - docs/plans/lsp/optimize/2026-08-31-plan03-task05-owner-variant-display-acceptance.md
doc_type: milestone-record
---

# Plan 03 Task 5: LSP Type Identity Display

## Goal

删除 LSP type prototype builder 中按显示名称反向推断基础类型的第二套语义。LSP 请求期
type prototype 只消费 parser 的 inferred type identity；source spelling alias 只作为展示
事实，不改变 `TypeId` 或解析结果。

## Contract

- `semantic_type_prototypes.c` 不维护 `int`/`i64`、`string`/`str` 等名称到
  `EZrValueType` 的映射表。
- 简单 identifier type 通过 `ZrParser_AstTypeToInferredType_Convert` 解析；同一 helper
  同时用于 declared-type resolution 和 inferred display builder。
- parser probe 暂时清空 `semanticContext`，因此不会发布重复 LSP semantic fact；失败时恢复
  compiler error state，保留既有 analyzer 诊断状态。
- canonical inferred type 已有 `typeName` 时直接使用；没有 stored name 时仅用
  `ZrParser_TypeNameString_Get` 得到 display text。该文本不参与 identity、lookup 或 fallback
  resolution。
- unsupported or unresolved types remain `ZR_VALUE_TYPE_OBJECT` recovery and are not made
  resolved by a name or text comparison.

## TDD

RED 在 LSP source-contract 中冻结三条约束：builder 必须调用 parser conversion，display helper
必须调用 parser type-name formatter，旧 `semantic_type_prototypes_base_type_from_name` 不得存在。
初始 source-contract 在 GCC 快照中报告两项预期 failure。GREEN 删除 name mapper，加入 parser
conversion probe、error-state isolation 和 formatter-only display fallback；修正 source test
的 function-section boundary 后，GCC source-contract 为 `0` failures。

## Verification

- GCC fresh snapshot：source-contract real exit 0；interface real exit 0。
- Clang fresh snapshot：source-contract real exit 0；interface real exit 0。
- MSVC static Debug snapshot (`VSCMD_VER=17.14.38`)：source-contract real exit 0；interface
  real exit 0。Windows checkout 的 tested source was LF-normalized in the isolated worktree so
  multiline source-contract matching has the same input bytes as the GCC/Clang snapshots; the
  main worktree was not normalized.
- The Task 4.29 GCC/Clang parser/query and callable project evidence remains valid; this slice does
  not claim a fresh full 16-target matrix, full project runner, or three stdio/CLI smokes.
- The project runner still contains 14 historical unrelated markers in the fresh GCC/Clang
  snapshot. They are retained as open baseline evidence and are not counted as Task 5 failures.

## 状态与产出记录

- 完成时间：2026-09-01 20:24 +08:00。
- 状态：Task 5 LSP type identity display GREEN；Plan 03 Task 6、Task 7、Task 8 仍进行中。
- 完成项目：删除 LSP primitive name-to-type mapper；parser canonical conversion probe；失败
  compiler-state isolation；display-only type-name formatter fallback；source-contract regression
  coverage；GCC/Clang/MSVC focused verification；主计划与 module documentation 更新。
- 未完成项目：结构化 diagnostics 唯一化、剩余 LSP consumer 迁移、完整16-target matrix、完整
  project runner、三套 stdio/CLI smoke，以及 Plan 03 Task 8 总门禁。
