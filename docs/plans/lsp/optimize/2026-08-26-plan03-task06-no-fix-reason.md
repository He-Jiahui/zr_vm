---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fixes.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_copy.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fixes.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_copy.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/acceptance/2026-08-26-plan03-task06-no-fix-reason.md
doc_type: milestone-record
---

# Plan 03 Task 6.1: Explicit No-Fix Reason

## Goal

Make the absence of a diagnostic machine edit an explicit parser-owned fact,
so an LSP consumer never has to infer producer intent from an empty fix array,
message text, or diagnostic code.

## Contract

- `SZrStructuredDiagnostic.noFixReason` is appended after existing fields and
  uses `UNSPECIFIED` only for a producer that has not classified fixability.
- `ZrParser_StructuredDiagnostic_SetNoFixReason` accepts a defined nonzero
  reason, is idempotent for the same value, and rejects conflicting values.
- A diagnostic cannot contain both typed fixes and a no-fix reason. Both
  mutation directions enforce the invariant.
- `ZrParser_StructuredDiagnostic_Copy` preserves the reason and rejects an
  inconsistent source instead of silently selecting one disposition.
- Persistent semantic facts and `SemanticQuery_Diagnostics` expose the same
  copied value; no parser query or LSP reconstruction is added.

## Implementation

The public enum distinguishes not-applicable, insufficient-context,
requires-user-decision, and unsafe-edit cases. The setter lives beside the
existing fix mutation API in the small diagnostic fixes module. The field is
at the end of the public structure to preserve existing member offsets, while
normal initialization keeps `UNSPECIFIED` as the zero state.

The dedicated parser test publishes a diagnostic with a no-fix reason into a
semantic context, releases the producer object, explicitly materializes query
diagnostics, and verifies the snapshot-owned query copy. A second test freezes
the bidirectional fix/no-fix exclusion and direct-copy behavior.

## Verification

On the same `da9b86c654dc + 5 code/test/CMake overlays` source snapshot, WSL
GCC 11.4 and Clang 14 directly report 2/2 for the new target, 4/4 for semantic
query contract, and 46/46 for compiler semantic-query diagnostics. A fresh
MSVC 19.44 static build reports the same 2/2, 4/4, and 46/46. Every direct test
sequence exits zero.

## 状态与产出记录

- 完成时间：2026-08-26 16:47 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收；不声明 Plan 03 Task 6
  完成。
- 完成项目：typed no-fix reason、fix/no-fix 双向互斥、persistent fact
  deep-copy、query materialization preservation 和独立 contract tests。
- 后续项目：为全部 compiler producers 分类 fix disposition、LSP 纯协议投影、
  compiler/LSP golden parity 与重复 analyzer 删除。
