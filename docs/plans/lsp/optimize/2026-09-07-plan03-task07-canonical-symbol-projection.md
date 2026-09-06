---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/astra/lsp/review.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_symbol_projection_cases.h
doc_type: milestone-record
---

# Plan 03 Task 7 Astra P1: Canonical Symbol Projection Fail-Closed

## Goal

让公开的 LSP 位置符号投影只接受 parser semantic snapshot 的 canonical
`SymbolAt` 结果和稳定 `SymbolId`，避免 canonical 查询失败后由 LSP symbol
table 的范围或 retained reference ranges 猜测出语义身份。

## Contract

- `ZrLanguageServer_Lsp_FindSymbolAtUsageOrDefinition` 保留 exact property
  contract 入口，再把普通 symbol 交给
  `ZrLanguageServer_SemanticAnalyzer_GetSymbolAt`。
- `SemanticAnalyzer_GetSymbolAt` 的 canonical query 失败、semantic context
  缺失或 `SymbolId` 无效时返回 `ZR_NULL`。
- 位置查询不遍历 `SZrSymbolTable::allScopes`，不按 declaration range、reference
  range、name 或显示文本恢复 canonical identity。
- 返回的 LSP symbol 只能通过 `FindBySemanticId` 从已投影 symbol table 取得；
  raw symbol 的生命周期继续绑定当前 analyzer/context，仅同步请求期借用。

## Implementation

- 删除 `file_position_is_in_range` 以及 `Lsp_FindSymbolAtUsageOrDefinition` 中的
  all-scopes/reference-range fallback。
- 保留属性 contract 的 exact source identity 处理和 canonical `SymbolAt` bridge，
  不改变 property bookkeeping 使用的 reference storage。
- 增加两个 parity cases：detach semantic context 后拒绝位置查询；篡改 LSP
  symbol 的 semantic id 后拒绝位置查询。

## Verification

- GCC WSL build：
  `cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --target zr_vm_language_server_semantic_query_parity_test --parallel 4`
  完成并链接成功。
- Clang ASan/UBSan WSL build：
  `cmake --build /mnt/e/Git/zr_vm/.codex/lsp-optimize-validation/clang-asan-current --target zr_vm_language_server_semantic_query_parity_test --parallel 4`
  完成并链接成功。
- 两个新用例在 GCC 与 Clang ASan/UBSan parity executable 中均真实 PASS。
- 整套 parity executable 仍以 exit 1 结束，失败名称与
  `2026-09-06-plan00-task01-sub04-current-gcc-failures.json` 的基线一致：source
  snapshot parity、local reference consumers、missing-canonical fixture 和
  detached-analyzer source hover；Clang 另报告既有 544-byte LSan 泄漏。该基线差异
  未归因于本子里程碑，也未宣称完整 parity 或 Task 8 通过。

## 状态与产出记录

- 完成时间：2026-09-07 04:50 +08:00。
- 状态：Plan 03 Task 7 Astra P1 focused 子里程碑完成；`Task 7.63 ResolveTypeAtPosition`
  与 Plan 03 Task 7/8 其余门禁仍进行中。
- 完成项目：删除公开 symbol position lookup 的 LSP scope/range fallback；加入
  canonical identity 缺失与不一致的 fail-closed 回归；保留 property exact contract。
- 未完成项目：其余 consumer 迁移、source/binary/native sourceless relation、virtual
  declaration URI、真实 multi-provider generation、完整 16-target matrix、三套
  stdio/CLI smoke，以及 Task 8 总门禁。
