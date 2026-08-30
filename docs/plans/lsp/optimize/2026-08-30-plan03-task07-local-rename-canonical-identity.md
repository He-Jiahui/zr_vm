---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
doc_type: milestone-record
---

# Plan 03 Task 7.40: Local Rename Canonical Identity

## Goal

让 local rename 的声明、引用和 placeholder 只依赖 parser 发布的 canonical SymbolId
与 relation/reference facts，避免 LSP 的 `SZrSymbol` lookup range、name 或不一致的 raw
symbol identity 改变重命名目标集合。

## Contract

- local rename 仅在 `hasCanonicalSymbol` 且 `canonicalSymbol.symbolId` 有效时继续。
- declaration 与 references 统一由
  `ZrLanguageServer_LspSemanticReferenceQuery_AppendReferences` 投影，包含 declaration
  时使用 parser `DeclarationOf`/`ReferencesOf` 的精确 range。
- canonical SymbolId 与仍存在的 raw LSP symbol 不一致时返回 unavailable；不得退回
  raw symbol id、lookup range 或 symbol name。
- rename placeholder 仅使用 canonical symbol display name；缺失 canonical display
  name 时不从其他 LSP/AST/name source 猜测。
- imported-member 与 external metadata rename 仍等待其 producer 发布跨项目 relation
  identity，本记录不以 local rename 的 canonical path 覆盖该缺口。

## RED/GREEN

RED 由 source-contract 审计固定：旧 local rename 直接添加
`GetSymbolLookupRange(query->symbol)`，并把 `query->symbol->name` 作为 placeholder；
reference projector 在 canonical/raw SymbolId 不一致时还会返回 raw id。GREEN 删除这
两类 fallback，local rename 改走 canonical reference query，并在 identity 缺失或冲突时
fail closed。

## Verification

- WSL GCC 11.4 对 `lsp_semantic_reference_query.c`、`lsp_interface.c` 和
  `test_lsp_source_contracts.c` 的等价 include/define 集合 `-fsyntax-only` 均真实 exit 0。
- 重新编译并链接的 source-contract executable 真实 exit 0；输出包含
  `PASS: Local rename uses canonical SymbolId and reference queries` 与
  `PASSED: LSP source contract tests`。
- `git diff --check` 对本子里程碑三个 code/test 文件通过。此前 Ninja 增量构建在
  `/mnt/e` 上 180 秒无输出而超时，未将其作为通过证据；本记录不声明 full interface、
  16-target matrix 或三套 stdio smoke GREEN。

## 状态与产出记录

- 完成时间：2026-08-30 12:26 +08:00。
- 状态：Task 7.40 local rename canonical identity focused GREEN；Plan 03 Task 7/Task 8
  总门禁仍进行中。
- 完成项目：local rename declaration/reference 投影统一到 canonical SymbolId 与 parser
  relation query；raw LSP SymbolId mismatch、lookup range/name fallback 已移除；source
  contract regression 已加入并通过真实 process exit。
- 未完成项目：cross-project imported function relation producer、binary/native external
  navigation、semantic-token producer ownership、三工具链完整 16-target 与三套 stdio
  smoke；这些不在本子里程碑中伪造或改写。
