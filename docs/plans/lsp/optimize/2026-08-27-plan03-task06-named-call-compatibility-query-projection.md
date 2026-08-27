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
  - tests/acceptance/2026-08-27-plan03-task06-named-call-compatibility-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.18: Named-Call Compatibility Query Projection

## Scope

This submilestone moves named function-call compatibility ownership from the
LSP analyzer to parser inference and structured compiler diagnostics. It
closes the source-to-stdio path for canonical ownership mismatch and weak-wake
diagnostics without changing receiver method-call production.

The fixed code/test overlay contains eight paths:

- parser overload scoring and call diagnostic implementation/header;
- parser compiler semantic-query diagnostic coverage and ownership regression
  coverage;
- the LSP semantic analyzer and source-contract test;
- one dedicated stdio diagnostic smoke.

## TDD And Implementation

The source-contract RED first failed five checks: the parser-inference entry
point was absent and the LSP analyzer still referenced named function lookup,
overload resolution, compatibility checking, and analyzer-owned mismatch text.
The parser RED then showed a named `Shared<Resource>` to `Unique<Resource>`
call producing generic descriptor `2011` instead of ownership descriptor
`2008`.

The parser now classifies ownership compatibility from the resolved signature,
passing mode, inferred argument type, and mapped argument node. It builds the
existing canonical ownership diagnostics, appends an exact-range ownership
violation fact, and leaves valid owner reborrow behavior unchanged. Borrow,
loan, weak, and owner-to-plain text contracts remain distinct rather than being
collapsed into one generic ownership message.

For named calls, the LSP analyzer invokes parser inference on the complete
primary expression and consumes the compiler/query diagnostic path. Source
contracts reject `ZrParser_TypeEnvironment_LookupFunction`,
`ZrParser_FunctionCallOverload_Resolve`,
`ZrParser_FunctionCallCompatibility_Check`, and LSP-owned function-call type
mismatch text in the analyzer implementation.

Receiver method calls are deliberately not claimed. Their detailed member-call
producer intersects the active Syntax L8 write set, so the existing method
consumer remains until that parser path is released.

## Verification

The first expanded GCC run exposed four ownership regressions: legal
`Unique -> borrowed` was rejected, and weak/borrow/loan cases lost their
specific error-message contracts. After routing each case through its existing
canonical predicate/builder, type inference returned to `123/123` while the
new structured diagnostics remained GREEN.

On fixed HEAD `1f7b9cd` plus a byte-exact eight-path code/test overlay, GCC
11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same ten targets:

- compiler semantic-query diagnostics: `54/54`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `14/14`;
- semantic query: `30/30`;
- type inference: `123/123`;
- LSP semantic-query diagnostics;
- semantic analyzer regressions;
- LSP source contracts;
- union-pattern diagnostics;
- compiler integration: `127/127`.

Each toolchain also passed the dedicated stdio smoke with a real exit code of
zero. It requires exactly one `ownership_mismatch` diagnostic on zero-based
line 3 characters 12..20, descriptor `2008`, canonical message/cause/
suggestion, registered help URI, `requires_user_decision`, and no fixes. The
complete stdio suite was not rerun for this submilestone.

SHA-256 auditing confirmed that all eight code/test paths were identical in
the shared workspace and the GCC, Clang, and MSVC acceptance snapshots.

## 状态与产出记录

- 完成时间：2026-08-27 08:21 +08:00。
- 状态：已完成 named function-call compatibility 的 parser structured
  diagnostic 与 LSP query projection，并通过 GCC/Clang/MSVC focused、
  compiler integration 与独立 stdio 验收；Plan 03 Task 6 继续进行。
- 完成项目：canonical overload ownership classification、descriptor
  `2008/4004`、exact argument range、ownership violation fact、reborrow 与
  borrow/loan/weak 回归、LSP named-call duplicate semantics 删除、source
  contract、三工具链 stdio transport 证据。
- 后续项目：等待 Syntax L8 释放 receiver member-call producer 后迁移 method
  call compatibility，并继续收敛剩余 analyzer-owned semantic diagnostics。
