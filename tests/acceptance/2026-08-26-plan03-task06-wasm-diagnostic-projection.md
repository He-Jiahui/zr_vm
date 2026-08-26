---
related_code:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server/wasm/wasm_diagnostic_json.cpp
  - zr_vm_language_server/wasm/wasm_diagnostic_json.h
tests:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/wasm_diagnostic_projection_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.10 WASM Diagnostic Projection

## Required Results

- WASM push, document-pull, and workspace-pull diagnostics consume one
  canonical `SZrLspDiagnostic` JSON projection.
- Standard `codeDescription.href`, descriptor identity, related information,
  and typed edits survive serialization.
- Typed-fix diagnostics omit no-fix disposition; no-fix diagnostics publish the
  canonical reason and no synthetic edits.
- Pull APIs are present in the generated Emscripten module exports.
- WASM code does not infer diagnostic policy from code, message, member name,
  or source text.

## Evidence

The old generated module provided the RED evidence: runtime smoke exited one on
missing canonical diagnostic source, and both pull exports were undefined. The
fixed Emscripten 4.0.23 Release target and its no-work replay exited zero. The
generated JavaScript and WASM files were loaded by Node 22.16.0, which reported
`PASS: WASM diagnostic JSON matches canonical LSP projection` with exit zero.

The updated source-contract executable directly exited zero under GCC 11.4,
Clang 14, and MSVC 19.44. Fixed GCC/Clang and Windows archive snapshots consumed
the same source-contract test and explicit export configuration. Snapshot-only
iteration/parser build-support synchronization is excluded from the committed
scope.

## Acceptance Decision

Accepted for WASM diagnostic push/pull/workspace projection. Removal of the
remaining duplicate analyzer diagnostics and full compiler/LSP golden parity
remain outside this record.

## 状态与产出记录

- 完成时间：2026-08-26 22:01 +08:00。
- 状态：已完成并通过 Emscripten runtime 与 GCC/Clang/MSVC source-contract 验收。
- 完成项目：统一 serializer、完整 diagnostic payload、显式 pull exports、
  push/pull/workspace runtime parity 与 direct source-contract evidence。
