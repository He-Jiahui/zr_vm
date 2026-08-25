---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2k: Direct Import Alias Visibility Facts

## Goal

Publish direct source module bindings as opt-in `VisibleSymbols` import/alias
candidates. The fact must retain the compiler-registered declaration symbol;
the parser query and LSP must not infer import status from an identifier or
module-path string.

## Implementation

- The source-scope producer recognizes only a variable declaration whose
  identifier pattern is initialized by `ZR_AST_IMPORT_EXPRESSION`.
- It reuses the existing resolved variable declaration fact and marks the
  visible-symbol candidate `isImport` and `isAlias`.
- The existing `includeImports` query option controls whether that candidate is
  visible. No query code, global symbol search, module lookup, or LSP code was
  changed.

## Contract

For `var math = import("zr.math");`, the `math` candidate has the exact
variable declaration `SymbolId` and declaration range. It is absent from the
default visible-symbol query and present exactly once when `includeImports` is
enabled. The module path is not used as an identity or fallback key. Destructured
imports and non-module type aliases are outside this child.

## Verification

- RED: the direct import declaration was published as an ordinary local, so
  the default visible-symbol query incorrectly returned `math`.
- GREEN: the source scope fact is marked import/alias from AST structure, so
  the default query excludes it and `includeImports` returns the original
  declaration `SymbolId`.
- MSVC static: symbols 16/16, semantic query 29/29, query contract 3/3,
  compiler diagnostics 46/46, and compiler integration 127/127 passed with
  zero failures and real process exit zero.

## 状态与产出记录

- 完成时间：2026-08-25 16:39:39 +08:00
- 状态：MSVC 子里程碑完成；本次未重跑 GCC/Clang executable gate，跨工具链
  acceptance 保持开放且未做通过声明。
- 完成项目：direct import canonical declaration identity、import/alias fact
  classification、default exclusion、`includeImports` opt-in projection、RED/GREEN、
  MSVC focused query/diagnostic/compiler-integration 回归、模块文档与验收记录。
- 后续项目：destructured import、type-value alias、binary/native import producers，
  以及 Task 2 LSP consumer migration。
