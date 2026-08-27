---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_type_inference.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_named_call_ownership_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-named-call-compatibility-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.18 Named-Call Compatibility Query Projection

## Required Results

- Parser overload scoring owns named-call type and ownership compatibility.
- Ownership failures publish registered structured diagnostics and an exact
  mapped-argument ownership violation fact.
- Legal owner reborrows remain accepted; weak, borrow, loan, and
  owner-to-plain failures preserve their canonical contracts.
- LSP named-call analysis triggers parser inference and does not reconstruct
  lookup, overload, compatibility, type text, or ownership policy.
- GCC, Clang, and MSVC pass the same focused targets and dedicated stdio smoke
  from byte-identical code/test paths.

## Evidence

Source-contract RED produced five failures for the absent parser-inference
entry and four forbidden LSP semantics. Parser RED produced descriptor `2011`
where the canonical `Shared<Resource> -> Unique<Resource>` mismatch required
descriptor `2008`. Expanded type-inference RED then caught one legal reborrow
rejection and three lost ownership message contracts before final GREEN.

The acceptance baseline is HEAD `1f7b9cd` plus eight code/test paths. GCC 11.4,
Clang 14.0.0, and MSVC 19.44.35228.0 with `VSCMD_VER=17.14.38` returned real
exit zero for the same ten targets. Unity totals include compiler diagnostics
`54/54`, query disposition `11/11`, semantic facts `14/14`, semantic query
`30/30`, type inference `123/123`, and compiler integration `127/127`; all four
LSP focused suites also passed.

The dedicated stdio fixture freezes exactly one descriptor-2008
`ownership_mismatch` diagnostic at zero-based line 3 characters 12..20. It
requires canonical full text, the registered help URI,
`requires_user_decision`, and an empty fixes payload. All three toolchain
servers passed the contract with real exit zero.

SHA-256 values match across the shared workspace and all three acceptance
snapshots for every code/test path. The complete stdio suite was not rerun.

Method-call compatibility is not accepted by this slice. Its detailed parser
producer remains under Syntax L8 ownership, so this decision covers named
function calls only.

## Acceptance Decision

Accepted for named function-call compatibility and LSP query projection. Plan
03 Task 6 remains in progress; receiver method-call compatibility is the next
explicit producer boundary.

## 状态与产出记录

- 完成时间：2026-08-27 08:21 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC focused、compiler integration
  与独立 stdio 验收；Task 6 继续进行。
- 完成项目：parser-owned named-call compatibility、structured ownership
  diagnostics、exact argument facts、canonical no-fix disposition、LSP
  duplicate semantics 删除、reborrow/weak/borrow/loan 回归与三工具链证据。
