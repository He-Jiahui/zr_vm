---
related_code:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server/wasm/wasm_diagnostic_json.cpp
  - zr_vm_language_server/wasm/wasm_diagnostic_json.h
implementation_files:
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - zr_vm_language_server/wasm/wasm_diagnostic_json.cpp
  - zr_vm_language_server/wasm/wasm_diagnostic_json.h
  - zr_vm_language_server/CMakeLists.txt
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/wasm_diagnostic_projection_smoke.js
  - tests/acceptance/2026-08-26-plan03-task06-wasm-diagnostic-projection.md
doc_type: milestone-record
---

# Plan 03 Task 6.10: WASM Diagnostic Projection

## Goal

Make WASM push, document-pull, and workspace-pull diagnostics serialize the
same canonical `SZrLspDiagnostic` projection as native LSP and stdio, without
reconstructing diagnostic policy from names, messages, or source text.

## Contract

- One WASM serializer owns severity, primary range, source, code, message,
  `codeDescription`, descriptor identity, related information, typed fixes,
  and explicit no-fix disposition.
- Typed-fix diagnostics include their structured edits and omit
  `noFixReason`; no-fix diagnostics include the canonical enum name and no
  synthetic fixes.
- Push, document-pull, and workspace-pull entry points all use the same
  serializer and preserve each diagnostic's document URI.
- Document-pull and workspace-pull functions are explicit Emscripten exports.

## Implementation

The diagnostic JSON projection was extracted from `wasm_exports.cpp` into a
cohesive WASM module. The push path now frees the complete projected diagnostic
array, while pull and workspace-pull wrap that same array in their protocol
report shapes. Export configuration publishes both pull entry points.

The runtime smoke exercises a typed `missing_return` fix and a canonical
`unsupported_fix` no-fix diagnostic through push, document-pull, and
workspace-pull APIs. Source contracts prevent reintroducing an inline WASM
diagnostic serializer or dropping the explicit exports.

## Verification

The pre-change WASM artifact failed the runtime smoke because the JSON payload
lacked canonical diagnostic source/disposition, and neither pull entry point
was exported. A fixed Emscripten 4.0.23 Release snapshot then built with real
exit zero; a direct no-work replay also exited zero. Node 22.16.0 loaded the
generated module, observed both pull exports as functions, and passed the
push/pull/workspace parity smoke.

WSL GCC 11.4, Clang 14, and Windows MSVC 19.44 each directly passed the updated
LSP source-contract executable. The isolated WASM snapshot additionally used
the current shared build-support projection for iteration and parser production
contract sources; those snapshot-only build inputs are not part of this change.

## 状态与产出记录

- 完成时间：2026-08-26 22:01 +08:00。
- 状态：已完成 WASM push/pull/workspace 诊断同构投影并通过运行时与三工具链
  source-contract 验收；不声明 Plan 03 Task 6 完成。
- 完成项目：共享 WASM serializer、relatedInformation、descriptorId、
  codeDescription、typed fixes、canonical no-fix reason、显式 pull exports、
  Emscripten Release 产物和 Node runtime smoke。
- 后续项目：删除剩余重复 semantic analyzer 诊断，并完成 Task 6 compiler/LSP
  golden parity 与最终门禁。
