---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.22: Canonical Import-Origin Definition

## Scope

Move native import-alias definition navigation from request-time import binding
and alias-name matching to the parser relation graph plus metadata projection.
The alias must remain navigable when its analyzer AST is unavailable, while an
ambiguous or inconsistent external origin must fail closed.

## TDD And Implementation

The runtime RED removed `analyzer->ast` after a complete semantic snapshot and
queried the use of a native import alias. The old consumer returned no
definition even though `SymbolAt` retained the exact SymbolId and TypeId. The
source-contract RED also failed because no relation adapter existed.

The producer lifecycle was the first support defect. LSP analysis built source
scope facts during `SemanticCalls_PublishSource`, after the earlier compiler
relation publication pass, so no import-origin relation existed. The analyzer
now publishes `SemanticRelations_PublishImportOrigins` immediately after source
facts and before the snapshot is consumed.

`SymbolAt`, `VisibleSymbols`, and `DeclaredSymbols` now classify imports and
project the borrowed external origin from the same visible-symbol fact.
`lsp_semantic_relation_query.c` queries `RelationsOfSymbol`, requires exactly
one external `IMPORT_EXPORT_ORIGIN` whose source SymbolId and source/target
TypeIds match that canonical symbol, and passes the origin identity to
`ResolveImportedModuleEntry`. The metadata provider owns source, binary, and
native URI selection. A relation-supplied virtual URI must equal that result.

The adapter returns not-applicable, resolved, or invalid. Only a canonical
non-import symbol is not applicable. A classified import with no projected
origin, no relation, ambiguous origins, a missing metadata entry, or a URI mismatch is invalid and
cannot fall through to AST import-chain or alias-name resolution. The negative
matrix keeps the AST available while removing the symbol origin, removing the
relation, changing the origin to missing metadata, injecting a mismatched
virtual URI, and appending a second origin. The adapter contains no
import-binding collection, alias/member-name
comparison, source scan, or virtual URI formatting.

## Verification

The same isolated source snapshot was built and run from:

- source: `/home/hejiahui/.cache/zr-lsp-inline-417-src`;
- GCC: `/home/hejiahui/.cache/zr-lsp-inline-417-gcc`;
- Clang: `/home/hejiahui/.cache/zr-lsp-inline-417-clang`.

Both toolchains completed with real exit 0:

- symbols `24/24`, query `30/30`, calls `30/30`, relations `28/28`;
- query contract `6/6`, canonical consumers `21/21`, semantic facts `17/17`;
- type inference `124/124`;
- semantic-query parity and all source-contract checks;
- the complete LSP interface executable, including AST-independent native
  alias definition and ambiguous-origin rejection.

An attempted concurrent GCC/Clang type-inference run was discarded after the
two processes collided in shared binary-import temporary artifacts and each
reported one different failure. The accepted evidence reruns both toolchains
globally serially at 124/124.

MSVC, the complete repository target matrix, and stdio smoke were not run for
this focused submilestone.

## 状态与产出记录

- 完成时间：2026-09-01 15:46 +08:00。
- 状态：Task 3.22 canonical import-origin definition consumer子里程碑已完成；
  Plan 03 Task 3与Task 8继续进行。
- 完成项目：LSP snapshot import-origin producer lifecycle；parser公共symbol
  query的`isImport`/external origin投影；三态canonical relation adapter；
  metadata-owned source/binary/native module-entry URI；无AST native alias
  definition；missing symbol origin/relation/metadata、URI mismatch及ambiguous origin
  fail-closed矩阵；source-contract；GCC/Clang focused门禁。
- 后续项目：import literal/chain relation consumer；parser-owned无source
  virtual declaration URI producer；真实multi-provider nonzero generation；
  binary/native sourceless relation matrix；MSVC、完整矩阵与stdio smoke。
