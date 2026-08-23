---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/incremental_parser.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_declaration_index.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_syntax_reparse.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_declaration_index.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental/incremental_syntax_reparse.c
  - zr_vm_language_server/src/zr_vm_language_server/project/lsp_project.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
plan_sources:
  - docs/plans/lsp/optimize/02-snapshots-workspaces-and-diagnostics.md
tests:
  - tests/language_server/test_incremental_parser.c
  - tests/language_server/test_lsp_incremental_equivalence.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_semantic_snapshot.c
doc_type: milestone-record
---

# Plan 02 Task 5: Incremental Declaration Reparse

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
| --- | --- | --- | --- |
| 2026-08-23 13:27 +08:00 | 已完成 | 将非 token-equivalent 更新从无条件 full reparse 收敛为已验证的 declaration reparse 或显式 full fallback；将 declaration AST identity 变化纳入项目语义缓存失效。 | GCC Debug shared 四项直接测试均 exit 0，输出未出现 `Fail -` 或 `FAIL:`。 |

## Delivered Contract

- `SZrFileVersion.lastParseMode` 明确标记 `full_reparse`、`token_equivalent`
  和 `declaration_reparse`，因此 telemetry/capability 不会把保守路径表述为
  内部增量。
- Declaration index 仅选择完整包含 edit 的唯一顶层声明。新旧文本长度、edit
  长度、edit 起点、行结束符布局、lexer token boundary、AST kind 和完整 range
  必须全部一致；任一条件不满足时运行完整解析。
- 成功路径保留 script root 与未触及兄弟节点，替换节点使用新 source/range；旧
  节点仅在 replacement 验证后释放。
- declaration replacement 必然清理该文档的 semantic cache，避免结构型 AST hash
  漏掉新 declaration identity。之后只由 parser canonical public-contract hash
  决定是否保留或重算 importer；没有 name/text fallback。
- Differential test 将同一编辑序列的增量结果与 clean full parse 比较 AST、
  diagnostics、symbols、TypeId/SymbolId relations 和公开 LSP JSON。

## Validation

```text
ninja -C .codex/build-lsp-snapshot-gcc \
  zr_vm_language_server_incremental_parser_test \
  zr_vm_language_server_lsp_incremental_equivalence_test \
  zr_vm_language_server_lsp_semantic_snapshot_test \
  zr_vm_language_server_lsp_project_features_test
```

- `zr_vm_language_server_incremental_parser_test`: direct exit 0, no failure
  marker.
- `zr_vm_language_server_lsp_incremental_equivalence_test`: direct exit 0,
  `0 failure(s)`.
- `zr_vm_language_server_lsp_semantic_snapshot_test`: direct exit 0,
  `0 failure(s)`.
- `zr_vm_language_server_lsp_project_features_test`: direct exit 0, including
  body-only public update preservation, inferred-body invalidation, and public
  signature invalidation.

## Scope Boundary

This record completes only Plan 02 Task 5's GCC focused gate. Task 6
diagnostic aggregation and Task 7's GCC/Clang/MSVC acceptance matrix remain
open.
