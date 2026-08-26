---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_diagnostic_projection.c
  - zr_vm_language_server/src/zr_vm_language_server/diagnostics/lsp_diagnostic_store.c
  - zr_vm_language_server/stdio/stdio_diagnostic_json.c
tests:
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_type_mismatch_diagnostic_cases.h
  - tests/language_server/stdio_diagnostic_fix_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.9 LSP Diagnostic Projection

## Required Results

- Structured descriptor identity and help URI survive analyzer and LSP copies.
- Typed fixes remain typed fixes without an explicit no-fix reason.
- No-fix diagnostics retain their canonical reason and publish no synthetic fix.
- stdio uses standard `codeDescription.href` and stable `data.noFixReason`.
- Result ids change when help URI or no-fix disposition changes.
- No protocol field is reconstructed from diagnostic code, message, or source.

## Evidence

GCC 11.4, Clang 14, and MSVC 19.44 directly execute the focused LSP semantic
query diagnostics target at 14/14 with zero failures. Each toolchain also
directly executes `stdio_diagnostic_fix_smoke.js` with exit zero. GCC and Clang
share a byte-matched fixed ext4 source snapshot with separate build directories;
MSVC uses a separate archive snapshot with the same 12 SHA-256-matched overlays.

## Acceptance Decision

Accepted for native LSP and stdio diagnostic disposition projection. WASM JSON
parity and removal of remaining duplicate analyzer diagnostics remain outside
this record.

## 状态与产出记录

- 完成时间：2026-08-26 18:42 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：codeDescription、no-fix data、typed-fix branch、resultId identity、
  exact related range 和三工具链 direct smoke evidence。
