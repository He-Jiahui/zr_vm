---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_semantic_query_public_contract_cases.h
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/acceptance/2026-08-26-plan03-task06-diagnostic-completeness-gate.md
doc_type: milestone-record
---

# Plan 03 Task 6.8: Diagnostic Completeness Gate

## Goal

Prevent an incomplete fix disposition from crossing the persistent semantic
fact boundary and close the final conditional diagnostic-builder branches.

## Contract

- `AppendDiagnostic` rejects a diagnostic with no fixes and an unspecified
  no-fix reason.
- A diagnostic with at least one typed fix remains accepted.
- A diagnostic with a defined no-fix reason remains accepted.
- Legacy property migration without an exact replacement publishes
  `REQUIRES_USER_DECISION`.
- Removed ownership member syntax without a canonical replacement publishes
  `INSUFFICIENT_CONTEXT`; exact replacements retain machine fixes.

## Implementation

The semantic fact append gate validates disposition before duplicate lookup or
deep copy, so incomplete values cannot enter snapshot-owned storage. Existing
copy invariants continue to reject fix/reason conflicts.

The two conditional builders now use explicit fix and no-fix branches with
cleanup on either failure. Contract tests prove incomplete rejection followed
by accepted typed reason, and producer tests exercise both null and exact
replacement branches.

## Verification

On the byte-equivalent `1634858 + 5 code/test overlays` snapshot, WSL GCC 11.4
and Clang 14 directly report 4/4 semantic-query contract, 30/30 semantic query,
and 8/8 diagnostic tests. MSVC 19.44 reports the same results. Every direct
sequence exits zero.

## 状态与产出记录

- 完成时间：2026-08-26 17:53 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收；不声明 Plan 03 Task 6
  完成。
- 完成项目：persistent fact completeness gate、两个条件 migration builder
  的 fix/no-fix 分支、public-contract fixture 合规和三工具链回归。
- 后续项目：LSP 纯协议投影、compiler/LSP golden parity、重复 semantic
  analyzer 删除与 Task 6 最终门禁。
