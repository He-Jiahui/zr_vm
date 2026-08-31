---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.41: Interface Const Publisher Cutover

## Scope

Complete the consumer half of Task 6.35. Interface const-field diagnostics must
be produced by the parser-owned persistent-fact publisher. The LSP symbols
analyzer may trigger that publisher for its semantic snapshot, but it must not
enumerate violations, build diagnostics, or append facts itself.

## TDD And Implementation

The source-contract RED required
`ZrParser_InterfaceContract_PublishConstFieldDiagnostics` and rejected the
three lower-level producer APIs in the LSP module. Against the old analyzer it
failed exactly four assertions: the publisher was absent and the violation,
builder, and append APIs were all present.

The analyzer now calls the parser publisher once after registering a source
class. The parser owns stable descriptor `2014`, exact primary and related
ranges, severity, message fields, and the `requires_user_decision` no-fix
disposition. The LSP path neither reconstructs those fields nor falls back to
member names or AST pairing. Missing compiler/semantic context retains the
previous no-op behavior; publisher failure stops analysis without exposing a
partial local fact set.

## Verification

On the isolated fixed source snapshot, GCC and Clang each returned real exit
zero for:

- LSP source contracts `70/70`;
- LSP semantic-query diagnostics `19/19`;
- parser interface const-field publisher `1/1`.

The existing semantic-analyzer interface const-field case also passed on GCC:
the mutable implementation and missing implementation still produce exactly
two descriptor-2014 diagnostics with exact primary/related ranges and explicit
no-fix disposition. The full semantic-analyzer executable retains unrelated
known failures and is not counted as a green suite. The GCC and Clang interface
targets retain the same fixed eight producer failures, delta zero. MSVC, the
full 16-target matrix, and stdio smoke were not run for this narrow cutover.

## 状态与产出记录

- 完成时间：2026-08-31 11:04 +08:00。
- 状态：Task 6.41 子里程碑已完成；Plan 03 Task 6 继续进行。
- 完成项目：删除 LSP violation/builder/append producer loop；切换到 parser
  persistent-fact publisher；source-contract RED/GREEN；GCC/Clang `70/19`
  与 parser publisher `1/1` focused 门禁；interface const-field 两诊断端到端
  保持；interface fixed8、delta 0。
- 后续项目：继续迁移其余 analyzer-owned semantic diagnostic producer，完成
  compiler/LSP golden parity 总门禁及 MSVC/完整矩阵/stdio 验收。
