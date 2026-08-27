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
  - tests/acceptance/2026-08-27-plan03-task06-assignment-compatibility-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.19: Assignment Compatibility Query Projection

## Scope

This submilestone moves ordinary assignment, explicit typed initializer, and
return ownership compatibility from LSP reconstruction to parser inference and
structured compiler diagnostics. It also makes the public ownership fact query
select the precise violating fact when broad and narrow facts overlap.

The fixed code/test overlay contains ten paths:

- parser compatibility diagnostics, semantic ownership-fact selection, and
  their focused tests;
- the LSP semantic analyzer and source-contract/analyzer tests;
- one dedicated stdio ownership diagnostic smoke.

Module, milestone, and acceptance documentation add three paths to the final
exact commit.

## TDD And Implementation

Parser RED first produced generic descriptor `2011` where initializer and
assignment ownership failures required descriptor `2008`. The LSP source
contract RED retained `semantic_emit_ownership_compatibility_diagnostic`,
proving that the analyzer still reconstructed policy and text. A lower semantic
fact RED then returned a broad non-violation COPY fact instead of the narrower
ERROR fact at the queried position.

Type inference now uses a generic structured ownership mismatch reporter after
canonical compatibility fails. The existing named-call adapter delegates to
the same reporter. Borrowed and loaned incompatibilities preserve their
dedicated escape classifications when earlier equality or legal-reborrow
checks have not accepted the operation; this restored the expanded
`Unique <- Loaned` regression contract.

The ownership fact query selects the narrowest containing range. For equal
width candidates, a violation wins over a non-violation; equal-priority facts
retain append order. The LSP assignment, explicit initializer, and return
paths now publish the current parser compiler diagnostic and consume query
output. They no longer classify ownership or construct its message, cause,
suggestion, range, help, or fix disposition.

Receiver method-call compatibility and existing borrow/loan return
related-range enrichment are not changed by this slice.

## Verification

The first expanded type-inference run exposed one lost Loaned escape case out
of 123 tests. Restoring the dedicated classification returned the suite to
`123/123` while preserving the new descriptor-2008 paths.

On fixed HEAD `e7f021a` plus a byte-exact ten-path code/test overlay, GCC 11.4,
Clang 14.0.0, and MSVC 19.44.35228.0 (`VSCMD_VER=17.14.38`) each directly
passed the same ten targets:

- compiler semantic-query diagnostics: `56/56`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `15/15`;
- semantic query: `30/30`;
- type inference: `123/123`;
- compiler integration: `127/127`;
- LSP semantic-query diagnostics;
- semantic analyzer regressions;
- LSP source contracts;
- union-pattern diagnostics.

Each toolchain also passed the dedicated stdio smoke with a real exit code of
zero. It requires exactly three descriptor-2008 `ownership_mismatch`
diagnostics: initializer at zero-based line 2 characters 35..41, assignment at
line 5 characters 13..19, and return at line 8 characters 11..17. Every
diagnostic requires canonical message/cause/suggestion, registered help URI,
`requires_user_decision`, and no fixes.

During validation HEAD advanced by one unrelated parser-test declaration
cleanup. That committed file was synchronized into all three snapshots, then
all eleven checks were rebuilt and rerun: GCC, Clang, and MSVC each reported
`RUN=11` and `EXIT_FAILURES=0`. SHA-256 auditing confirmed all ten code/test
paths were identical across the shared workspace and the three snapshots.
The complete stdio suite was not rerun for this submilestone.

## 状态与产出记录

- 完成时间：2026-08-27 09:51 +08:00。
- 状态：已完成 ordinary assignment、explicit typed initializer 与 return
  ownership compatibility 的 parser structured diagnostic 和 LSP query
  projection，并通过 GCC/Clang/MSVC focused、compiler integration 与独立
  stdio 验收；Plan 03 Task 6 继续进行。
- 完成项目：descriptor `2008`、exact source-expression range、ownership
  violation fact、narrowest/equal-width violation query selection、
  borrow/loan/reborrow 回归、LSP duplicate compatibility 删除、source
  contract、三工具链 byte-exact 与 transport 证据。
- 后续项目：等待 Syntax L8 释放 receiver method-call producer 后迁移 method
  call compatibility，并继续收敛剩余 analyzer-owned semantic diagnostics。
