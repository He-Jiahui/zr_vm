---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.42: Imported Reference Canonical Identity

## Goal

让 imported-member references 和 document highlights 只消费 parser canonical SymbolId
与 exact declaration identity。LSP 不再按 module/member name 扫描项目文件，也不在
parser producer 缺少 declaration range 时猜测同名目标。

## Contract

- `ResolveAtPosition` 在 import-chain 提前返回之前查询 parser
  `ZrParser_SemanticQuery_SymbolAt`，只保存 resolved canonical SymbolId、reference range
  和 declaration range。
- imported consumer 仅在 canonical declaration URI/range 与 structured metadata
  declaration URI/range 完全一致时可用；缺少 source、无效范围或 identity mismatch
  均 fail closed。
- references/highlights 把 query 投影到现有
  `ZrLanguageServer_LspSemanticReferenceQuery`，最终只调用 parser
  `DeclarationOf/ReferencesOf(SymbolId)`。
- 删除 imported-member 的 project/module/member name aggregation helper，不提供 AST、
  token、member spelling 或 type-text fallback。
- Syntax05 当前 exact-owned `type_inference_import_metadata.c` 不在本任务写集；producer
  尚未发布 exact imported declaration identity 时，LSP 明确返回 unavailable。

## RED/GREEN

RED 由 source-contract 回归固定：旧 `AppendReferences` 和 document highlights 分支仍调用
imported member name aggregation helpers，且 import-chain 会在 parser `SymbolAt` 前提前
返回。GREEN 将 canonical query 前移，在 consumer 入口校验 SymbolId 与两侧 exact
declaration identity，并删除旧 helper。回归同时禁止旧 references/highlights helper
重新进入这两个函数。

## Verification

- WSL GCC 与 Clang 对 `lsp_semantic_query.c` 的 `-fsyntax-only` 均真实 exit 0。
- 当前工作树重新编译并运行 `test_lsp_source_contracts.c`，真实 exit 0，包含新增
  imported canonical identity contract。
- `git diff --check` 通过；production source 中旧 imported references/highlights name
  aggregation helper 与 `AppendMatchingLocationsForUri` 扫描均已移除。
- focused Ninja 构建此前在 CMake glob verification 阶段 184 秒超时，未取得有效 runtime
  exit；本任务不复用旧 binary，也不把超时计为 GREEN。完整三工具链 16-target matrix
  和三套 stdio smoke 尚未重跑。

## 状态与产出记录

- 完成时间：2026-08-30 13:55 +08:00。
- 状态：Task 7.42 focused GREEN；Plan 03 Task 7/Task 8 总门禁仍进行中。
- 完成项目：imported-member canonical SymbolId/declaration identity acquisition；metadata
  declaration identity exact cross-check；references/highlights canonical relation projection；
  name-based imported aggregation helper 删除；source-contract RED/GREEN 与 GCC/Clang
  production syntax verification。
- 未完成项目：Syntax05 imported declaration identity producer、focused interface runtime、
  三工具链完整 16-target matrix、三套 stdio smoke，以及 binary/native cross-project
  relation parity；这些项目不得由 LSP 名称、类型文本或 AST fallback 替代。
