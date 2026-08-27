---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_facts.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_assignment_ownership_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-assignment-compatibility-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.19 Assignment Compatibility Query Projection

## Required Results

- Parser compatibility owns ordinary assignment, explicit typed initializer,
  and return ownership diagnostics.
- Ownership failures publish descriptor `2008` and an exact violating
  ownership fact while legal reborrows and dedicated borrow/loan failures keep
  their canonical behavior.
- Ownership queries prefer the narrowest containing fact and, at equal width,
  a violation over a non-violation.
- LSP projects parser diagnostics without reconstructing ownership policy,
  messages, ranges, help, or fixes.
- GCC, Clang, and MSVC pass the same focused targets and dedicated stdio smoke
  from byte-identical code/test paths.

## Evidence

Parser RED produced generic descriptor `2011` instead of ownership descriptor
`2008`. Source-contract RED found the retained analyzer-owned ownership helper.
The lower fact RED selected broad COPY fact kind `4` rather than precise ERROR
fact kind `6`. Expanded type inference then caught one lost Loaned escape case
before final GREEN.

The acceptance baseline is HEAD `e7f021a` plus ten code/test paths. GCC 11.4,
Clang 14.0.0, and MSVC 19.44.35228.0 with `VSCMD_VER=17.14.38` returned real
exit zero for the same ten targets. Unity totals include compiler diagnostics
`56/56`, query disposition `11/11`, semantic facts `15/15`, semantic query
`30/30`, type inference `123/123`, and compiler integration `127/127`; all four
LSP focused suites also passed.

The dedicated stdio fixture freezes exactly three descriptor-2008
`ownership_mismatch` diagnostics: initializer at zero-based line 2 characters
35..41, assignment at line 5 characters 13..19, and return at line 8
characters 11..17. Each requires canonical full text, registered help URI,
`requires_user_decision`, and an empty fixes payload. All three toolchain
servers passed with real exit zero.

After an unrelated one-file HEAD advance, the committed baseline file was
synchronized into every snapshot and all eleven checks were rebuilt and rerun.
GCC, Clang, and MSVC each reported `RUN=11` and `EXIT_FAILURES=0`. SHA-256
values match across the shared workspace and all three snapshots for every
code/test path. The complete stdio suite was not rerun.

Receiver method-call compatibility and borrow/loan return related-range
enrichment are outside this acceptance slice.

## Acceptance Decision

Accepted for assignment, explicit initializer, and return ownership diagnostic
projection. Plan 03 Task 6 remains in progress; receiver method-call
compatibility remains the next explicit producer boundary after Syntax L8.

## 状态与产出记录

- 完成时间：2026-08-27 09:51 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC focused、compiler integration
  与独立 stdio 验收；Task 6 继续进行。
- 完成项目：parser-owned assignment compatibility、structured ownership
  diagnostics、exact violation facts、deterministic overlap selection、LSP
  duplicate semantics 删除、borrow/loan/reborrow 回归与三工具链证据。
