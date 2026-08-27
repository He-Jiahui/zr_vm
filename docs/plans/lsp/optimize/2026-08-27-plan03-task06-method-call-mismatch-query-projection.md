---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task06-function-call-mismatch-query-projection.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_type_mismatch_diagnostic_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-method-call-mismatch-query-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.24: Method Call Mismatch Query Projection

## Scope

This submilestone closes the receiver-method boundary left open by the earlier
free-function call slice. Parser/type inference now owns exact method candidate
identity, parameter mapping, ownership classification, and the detailed
`type_mismatch` diagnostic. LSP invokes parser inference and projects the
persistent semantic query fact; it no longer walks class symbols by method name
or compares argument types independently.

The contract covers source class, struct, and interface method declarations
whose exact declaration AST identity is available. Binary/native call sites
remain fail-closed for source related ranges when no exact declaration range is
published; LSP must not fabricate one.

## TDD And Root Cause

The parser RED reported `62 Tests / 1 Failure`: the existing method mismatch
used the widened primary wrapper range instead of the exact argument token. The
LSP RED also showed that the analyzer still emitted its own whole-call
`"Type mismatch in method call"` diagnostic.

GDB located the active compiler fallback in
`type_inference_member_resolution.c`, not only the direct native/member
validator. Candidate scanning retained expected and actual types but discarded
the selected member identity and parameter index, so the detailed producer
could not recover the declaration parameter range. The repair carries those
structured values through the scan and emits only after all candidates fail.

## Implementation

`type_inference_call_diagnostics` now accepts exact member declaration identity
and supports class, struct, and interface method parameter lists. It maps the
parameter index back to the source argument, unwraps a memberless primary node
to its exact property range, and delegates descriptor `2011`, code
`type_mismatch`, canonical text, related parameter range, and typed placeholder
fix to `ZrParser_TypeError_ReportDetailed`.

Both direct receiver validation and overload member resolution try the
canonical ownership reporter before ordinary type mismatch reporting. This
preserves specialized ownership codes and no-fix policy. Candidate scanning
does not publish diagnostics; the first exact mismatch is retained only for the
final no-match result, preventing false errors when a later overload succeeds.

`semantic_analyzer_typecheck.c` no longer contains
`semantic_check_method_call`, `semantic_call_matches_parameters`, its direct
method `type_mismatch` producer, or the associated name/type-text ownership
classifier. Receiver calls use the same parser inference/compiler-error query
bridge as other canonical semantic diagnostics.

## Verification

The fixed source baseline is HEAD
`a66f001` plus twelve byte-identical code/test paths. SHA-256 comparison between
the workspace, WSL snapshot, and Windows snapshot reported `EXACT=12` and
`MISMATCH=0`. Later shared HEAD commits did not touch these twelve paths.

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same eleven checks with real
process exits:

- compiler semantic-query diagnostics: `62/62`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `15/15`;
- semantic query and registry/message coverage: `30/30`;
- type inference: `124/124`;
- compiler integration: `127/127`;
- LSP semantic-query diagnostics;
- semantic analyzer, including method ownership and compiler/LSP golden parity;
- fixed-snapshot LSP source contracts;
- union-pattern diagnostic regression;
- receiver-method stdio diagnostic/fix smoke.

MSVC build/run logs contained zero failure markers. Full repository GREEN is
not claimed by this focused Task 6 slice.

## 状态与产出记录

- 完成时间：2026-08-27 18:25 +08:00。
- 状态：已完成 receiver method call 参数失配的 parser/compiler 单一生产、
  semantic query 与 LSP/stdIO 投影，并通过 GCC/Clang/MSVC 同基线 11 项验收；
  Plan 03 Task 6 继续进行。
- 完成项目：method candidate/member identity 与 parameter index 保留、class/
  struct/interface declaration parameter range、exact argument range、descriptor
  `2011`、related information、typed placeholder fix、ownership-first 分类、
  overload scan 延迟发布、analyzer method checker/parameter matcher 删除、
  compiler/LSP golden parity、source contract、stdio smoke、12-path SHA-256 与
  真实退出证据。
- 后续项目：继续 support-first 迁移剩余 analyzer-owned semantic rules；不得在
  LSP 按 member name、symbol-table AST、source/display text 或本地参数匹配重建
  receiver call compatibility。
