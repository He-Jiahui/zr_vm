---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_imports.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.23: Canonical Import Literal Definition

## Scope

Move direct and destructured import-literal navigation from request-time AST
module-path reconstruction to a parser-owned position query over exact visible
facts and import-origin relations. Import chains are deliberately outside this
slice.

## TDD And Implementation

The parser RED required a structured `ImportOriginAt` result for the exact
`"zr.math"` literal shared by two destructured bindings, plus not-applicable and
conflicting-relation cases. The LSP RED removed `analyzer->ast` after snapshot
construction; the old literal consumer could no longer resolve `zr.system`.

Visible import facts now retain the exact module string-literal range. The new
read-only query validates that every binding at the literal has exactly one
matching `IMPORT_EXPORT_ORIGIN` relation with the same SymbolId, TypeId,
declaration range, external origin, and optional virtual URI. It returns
not-applicable, resolved, or invalid and clears output on every non-resolved
path. It does not manufacture a module SymbolId or inspect module spelling.

The LSP relation adapter invokes the query before the AST gate and delegates
module-entry URI selection to the existing metadata provider. A resolved query
uses the exact parser literal range. An invalid query cannot fall through to
AST import bindings. The legacy import-chain branch now rejects a module-only
hit, while retaining member-chain behavior for the next slice.

The negative runtime case preserves the complete AST, removes relation facts,
and requires resolution failure. Source-contract coverage freezes the
parser-query call, AST-before/after ordering, metadata-provider delegation, and
the module-only fail-closed branch.

## Verification

The same isolated source snapshot was built and run from:

- source: `/home/hejiahui/.cache/zr-lsp-inline-417-src`;
- GCC: `/home/hejiahui/.cache/zr-lsp-inline-417-gcc`;
- Clang: `/home/hejiahui/.cache/zr-lsp-inline-417-clang`.

Both toolchains completed with real exit 0:

- symbols `24/24`, query `30/30`, calls `30/30`, relations `29/29`;
- query contract `6/6`, canonical consumers `21/21`, semantic facts `17/17`;
- type inference `124/124`;
- semantic-query parity, all LSP source-contract checks, and the complete LSP
  interface executable.

The first Clang build command was terminated by its outer 120-second timeout
and is not evidence. The resumed incremental build completed with exit 0 before
the accepted serial test matrix. MSVC, the complete repository target matrix,
and stdio smoke were not run for this focused submilestone.

## 状态与产出记录

- 完成时间：2026-09-01 16:33 +08:00。
- 状态：Task 3.23 canonical import literal definition consumer子里程碑已完成；
  Plan 03 Task 3与Task 8继续进行。
- 完成项目：exact import literal range producer；三态`ImportOriginAt`公共查询；
  direct/destructured binding relation一致性与冲突门禁；AST-independent native
  literal definition；relation缺失时AST-present fail-closed；metadata-owned URI；
  source-contract；GCC/Clang focused与完整interface门禁。
- 后续项目：import-chain relation consumer；parser-owned无source virtual
  declaration URI producer；真实multi-provider nonzero generation；binary/native
  sourceless relation matrix；MSVC、完整矩阵与stdio smoke。
