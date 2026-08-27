---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-method-call-mismatch-query-projection.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_type_mismatch_diagnostic_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/stdio_diagnostic_fix_smoke.js
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.24 Method Call Mismatch Query Projection

## Required Results

- Resolve receiver method candidates and argument compatibility in parser/type
  inference, not in the language server symbol table.
- Publish descriptor `2011`, exact argument range, declared parameter related
  range, canonical text, and one typed placeholder fix.
- Preserve specialized ownership mismatch diagnostics before ordinary type
  mismatch policy.
- Avoid emitting a rejected-candidate diagnostic while a later overload may
  still match.
- Remove the analyzer-owned method-name scan, parameter matcher, and direct
  `"Type mismatch in method call"` producer.
- Prove compiler-query/LSP structured parity and stdio serialization without a
  name, AST-text, or display-text fallback.

## TDD Evidence

The first parser run reported `62 Tests / 1 Failure` because the primary range
covered the parser wrapper instead of the exact argument token. The new LSP
case also failed against the analyzer-owned whole-call diagnostic. GDB then
showed that the active mismatch fallback was emitted from overload member
resolution, where candidate identity and parameter index had been discarded.

The GREEN implementation retains those structured values during candidate
scanning, delays publication until selection has failed, and maps the exact
member declaration and argument into the shared detailed diagnostic builder.
The existing method ownership regression remains GREEN through the
ownership-first producer.

## Final Evidence

On fixed HEAD `a66f001` plus twelve byte-identical code/test paths, GCC 11.4,
Clang 14.0.0, and MSVC 19.44.35228.0 each passed the same eleven direct checks.
Shared Unity totals were `62/62`, `11/11`, `15/15`, `30/30`, `124/124`, and
`127/127`; all four LSP focused suites and the dedicated stdio smoke exited
zero on every toolchain.

SHA-256 comparison reported `EXACT=12` and `MISMATCH=0` for the workspace and
fixed snapshots. The MSVC environment was `VSCMD_VER=17.14.38`, and its logs
contained zero failure markers.

## Acceptance Decision

Accepted as the receiver-method slice of Plan 03 Task 6. Parser/compiler is the
only semantic producer for exact receiver method argument incompatibility, and
LSP is a semantic-query projection. Task 6 remains active for other
analyzer-owned semantic rules.

## 状态与产出记录

- 完成时间：2026-08-27 18:25 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 11 项验收；Plan 03
  Task 6 继续进行。
- 完成项目：receiver method RED/GREEN、exact member/parameter identity、
  descriptor/range/related/fix contract、ownership-first 分类、delayed overload
  emission、analyzer duplicate producer 删除、golden parity、source contract、
  stdio transport、12-path SHA-256 与真实退出证据。
