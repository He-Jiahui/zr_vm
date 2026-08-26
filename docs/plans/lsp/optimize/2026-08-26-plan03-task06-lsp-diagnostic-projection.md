---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_diagnostic_projection.c
  - zr_vm_language_server/stdio/stdio_diagnostic_json.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/diagnostics/lsp_diagnostic_store.c
  - zr_vm_language_server/stdio/stdio_diagnostic_json.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_type_mismatch_diagnostic_cases.h
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/acceptance/2026-08-26-plan03-task06-lsp-diagnostic-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.9: LSP Diagnostic Projection

## Goal

Project the complete parser/compiler diagnostic disposition into native LSP and
stdio protocol values without reconstructing policy from names, messages, or
source text.

## Contract

- Analyzer diagnostics retain the registry descriptor id/help URI, related
  ranges, typed fixes, and explicit no-fix reason from the structured fact.
- LSP diagnostics expose `codeDescriptionHref` and `noFixReason` as direct
  projections.
- stdio emits the help URI as `Diagnostic.codeDescription.href` and explicit
  no-fix disposition as `Diagnostic.data.noFixReason`.
- A diagnostic with a typed fix omits `data.noFixReason`; a no-fix diagnostic
  emits its canonical reason and no synthetic fixes.
- Diagnostic result ids include the help URI and no-fix reason.

## Implementation

The structured-diagnostic copy was extracted from the oversized semantic
analyzer into a cohesive projection module. A separate protocol module owns the
stable no-fix enum names. The stdio serializer only serializes those projected
fields, and the diagnostic store hashes them with the rest of the payload.

The focused type-mismatch parity fixture also corrected its stale return-type
related range from columns `12..15` to the exact declaration range `10..13` for
`fn bad(): int`.

## Verification

On the byte-equivalent `57b6f8d + 12 code/test overlays` snapshot, WSL GCC 11.4,
Clang 14, and Windows MSVC 19.44 each directly report 14/14 focused LSP semantic
query diagnostic tests. The stdio diagnostic-fix smoke directly exits zero on
all three toolchains. MSVC used an isolated archive snapshot and
`VSCMD_VER=17.14.38`; all snapshot overlays were SHA-256 matched.

## 状态与产出记录

- 完成时间：2026-08-26 18:42 +08:00。
- 状态：已完成 native LSP/stdIO 诊断 disposition 投影并通过
  GCC/Clang/MSVC 验收；不声明 Plan 03 Task 6 完成。
- 完成项目：registry help URI、explicit no-fix reason、typed-fix/no-fix
  protocol branching、resultId payload identity、focused compiler/LSP parity
  和 stdio JSON smoke。
- 后续项目：WASM diagnostic JSON 同构投影、剩余重复 semantic analyzer
  诊断删除、Task 6 全量 golden parity 与最终门禁。
