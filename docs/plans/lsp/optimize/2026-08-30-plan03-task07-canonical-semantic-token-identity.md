---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c
  - zr_vm_language_server/stdio/stdio_semantic_tokens_json.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
doc_type: milestone-record
---

# Plan 03 Task 7.41: Canonical Semantic Token Identity

## Goal

让 source-local semantic tokens 的声明、引用和类型分类消费 parser canonical
SymbolId、reference role、声明范围和声明节点，不再由 LSP symbol table、请求时类型
推断或名称 lookup 重建本地语义。声明 modifier 与 stdio legend 保持同一 canonical
role 投影。

## Contract

- 本地 declaration token 由 `ZrParser_SemanticQuery_DeclaredSymbols` 投影，只接受
  有效 SymbolId、canonical role 和声明范围；不遍历 `SZrSymbolScope`。
- 本地 identifier、member reference 和 import-chain segment 优先由同一范围的
  `ZrParser_SemanticQuery_SymbolAt` 投影；缺少 resolved canonical fact 时保持未知，
  不回退到 `ZrLanguageServer_LspSemanticQuery_ResolveAtPosition`、symbol-table
  name lookup 或 parameter name lookup。
- token type 优先由 canonical declaration AST kind 与 semantic symbol kind 决定，
  `DECLARATION` role 投影为 modifier bit 0；重复 token 合并时按位保留 modifier。
- 外部 imported metadata chain 仍使用既有 structured metadata adapter，仅作为跨模块
  producer 尚未发布完整 canonical reference relation 的受限边界；该 adapter 不参与
  source-local canonical identity fallback。

## RED/GREEN

RED 由 source-contract 审计固定：旧 semantic-token consumer 遍历 analyzer
symbol table，并在 identifier/member classification 中调用 LSP request-time semantic
query 和 name-based parameter lookup；owner type 失败时还会调用 analyzer 类型推断。
GREEN 删除这些 source-local fallback，统一建立 canonical SymbolAt helper，并将声明
enumeration 改为 DeclaredSymbols。新增 source-contract regression 禁止旧调用和旧
遍历路径；semantic-token legend 同步声明 modifier，runtime fixture 继续覆盖 canonical
ownership type、shadowed local、unresolved member 和 import-chain adapter。

## Verification

- WSL Clang `-fsyntax-only` 对 `lsp_semantic_tokens.c`、`stdio_semantic_tokens_json.c`、
  `test_lsp_source_contracts.c` 和 `test_lsp_interface.c` 真实 exit 0；interface test
  仅保留一条既有 `providerPhase` missing-field initializer warning。
- WSL GCC `-fsyntax-only` 对 semantic-token production source 真实 exit 0；
  `git diff --check` 通过。
- 重新编译并运行的 source-contract executable 真实 exit 0，包含
  `PASS: Semantic tokens use canonical symbol queries` 与
  `PASSED: LSP source contract tests`。
- focused Ninja interface/source-contract build 在 `/mnt/e` 构建目录的 CMake glob
  校验阶段等待 184 秒后超时，未取得有效 runtime test exit；不将旧可执行文件、该
  超时或任何既有 marker 计为本阶段 GREEN。三工具链完整矩阵与三套 stdio smoke
  仍待独立 fresh build 后重跑。

## 状态与产出记录

- 完成时间：2026-08-30 13:10 +08:00。
- 状态：Task 7.41 canonical semantic-token identity focused GREEN；Plan 03
  Task 7/Task 8 总门禁仍进行中。
- 完成项目：source-local declaration enumeration 已迁移到
  `ZrParser_SemanticQuery_DeclaredSymbols`；identifier/member classification 已迁移
  到 `ZrParser_SemanticQuery_SymbolAt`；request-time semantic analyzer、symbol-table
  parameter lookup 和 source-local name fallback 已删除；声明 role modifier 已接入
  token merge 与 stdio legend；source-contract 与 test source regression 已加入。
- 未完成项目：当前接口 runtime 的新 semantic-token 断言尚未因 build timeout 取得
  有效执行证据；cross-project imported relation producer、binary/native metadata
  parity、Plan 03 三工具链 16-target matrix 与三套 stdio smoke 仍未完成。
