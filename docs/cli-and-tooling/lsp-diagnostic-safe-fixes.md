---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_code_actions.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/stdio/stdio_lsp_memory.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_code_actions.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/stdio/stdio_lsp_memory.c
plan_sources:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_parser_diagnostics.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/test_lsp_diagnostic_safe_fix_cases.h
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_diagnostic_fix_smoke.js
doc_type: module-contract
---

# LSP Diagnostic Safe Fixes

## Ownership

Parser/compiler diagnostics own fix applicability, title, edit text and source range. LSP maps these facts to protocol actions and must not infer a fix from English messages, action titles, statement names or raw source prefixes.

`ZrParser_DiagnosticBuilder_BuildMissingStatementSemicolon` accepts a primary diagnostic location and a separate fix location. The primary range can identify the next token or EOF while the edit remains a zero-width insertion at the previous complete token end. `parser_diagnostics.c` derives that end by replaying the same lexer over the current source; whitespace and comments are therefore handled by token identity rather than text trimming.

Ordinary variable declarations now report `missing_statement_semicolon` at EOF as well as before a following statement. For-header declarations keep their existing `reportMissingSemicolon=false` boundary.

`ZrParser_DiagnosticBuilder_BuildMissingConditionClose` preserves the parser's primary range on the block opener and publishes a separate zero-width `)` insertion at that range's start. The producer marks the edit machine-applicable; the LSP does not infer the token from `missing_condition_close`, the statement kind or the English suggestion.

`ZrParser_DiagnosticBuilder_BuildMissingIndexClose` likewise keeps the opening `[` as the primary diagnostic range while accepting the current-token location as a separate fix range. The builder collapses that second range to its start and publishes a machine-applicable `]` insertion, so recovery at `value[0;` edits immediately before `;` without replacing either token.

`ZrParser_DiagnosticBuilder_BuildMissingParameterListClose` receives the unexpected token range after a declaration parameter list. It preserves that range as the primary diagnostic and publishes a zero-width machine-applicable `)` insertion at its start. The function, method, interface, extern and related declaration producers already share this reporter, so none of their parser branches infer or rebuild the edit.

`ZrParser_DiagnosticBuilder_BuildMissingCallClose` accepts the opening `(` primary range and a separate current-token fix range. The parser reporter supplies both ranges, and the builder publishes a zero-width machine-applicable `)` at the second range's start. This keeps navigation/diagnostic selection anchored on the call opener while `pick(1 + 2;` is repaired immediately before `;`.

`ZrParser_DiagnosticBuilder_BuildMissingGroupClose` follows the same dual-range contract for grouped expressions. The primary range remains the exact opening `(`, while the parser's current-token range supplies the zero-width `)` insertion point. Failed lambda lookahead restores the complete parser cursor before grouped-expression fallback, including token-start offsets and lines, so the primary range cannot inherit speculative lookahead coordinates.

`ZrParser_DiagnosticBuilder_BuildMissingArrayClose` preserves the exact opening `[` token range and accepts the current token as a separate fix range. `parse_array_literal` captures its opener with `get_current_token_location`, rather than the lexer's post-token current cursor, and the builder publishes a zero-width machine-applicable `]` at the fix range start. `return [1, 2` therefore keeps characters 7..8 as the primary range and inserts at EOF character 12.

## LSP Projection

`lsp_code_actions.c` calls `ZrLanguageServer_Lsp_GetDiagnostics` and publishes only fixes with `ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE`. It copies the structured title, range and text into a preferred quickfix. Placeholder and maybe-incorrect fixes remain diagnostic data and are not promoted to automatic code actions.

A caret request selects structured diagnostics/fixes on the same requested line or with an intersecting range. This preserves line-start code-action requests without reading source text. A block comment containing statement-like text has no parser fact and therefore produces no semicolon action. Missing condition, index, declaration parameter-list, call-close, group-close and array-close actions copy the parser-owned delimiter edits at their exact zero-width ranges.

`ZrLanguageServer_Lsp_FreeDiagnostics` is the public owner cleanup for arrays returned by `GetDiagnostics`; stdio and in-process code-action consumers share it.

## Snapshot Boundary

The safe fix uses the existing code-action workspace-edit fingerprint. The initial action carries the captured open document version/generation/open-state/length/hash. `codeAction/resolve` preserves the edit only while that fingerprint remains current; a stale action loses its edit and receives the standard disabled reason. Resolve never reconstructs the edit from title, code, message or current source.

## Validation

- Parser builder tests separate the primary range from the exact edit range and assert machine applicability.
- Parser/LSP diagnostics include an EOF variable declaration case.
- Advanced-editor tests cover EOF, line-comment insertion, block-comment absence, structured machine fix consumption and placeholder rejection.
- Main stdio smoke checks captured versions 1/2, exact `{line: 0, character: 15}` insertion, stale resolve rejection and version 3 apply/rebind cleanup.
- Diagnostic-fix smoke checks JSON `Diagnostic.data.fixes` for placeholder, semicolon, condition-close, index-close, parameter-list-close, call-close, group-close and array-close machine fixes, including each version 2 diagnostic-clear boundary.

Detailed three-toolchain acceptance evidence is recorded in `docs/plans/lsp/02-diagnostics/2026-07-21-semicolon-safe-fix-convergence.md`, `docs/plans/lsp/02-diagnostics/2026-07-21-condition-close-safe-fix-convergence.md`, `docs/plans/lsp/02-diagnostics/2026-07-21-index-close-safe-fix-convergence.md`, `docs/plans/lsp/02-diagnostics/2026-07-21-parameter-list-close-safe-fix-convergence.md`, `docs/plans/lsp/02-diagnostics/2026-07-21-call-close-safe-fix-convergence.md`, `docs/plans/lsp/02-diagnostics/2026-07-21-group-close-safe-fix-convergence.md` and `docs/plans/lsp/02-diagnostics/2026-07-21-array-close-safe-fix-convergence.md`.

## Open Boundaries

This contract covers local semicolon insertion plus missing control-condition `)`, index `]`, declaration parameter-list `)`, call `)`, grouped-expression `)` and array-literal `]` insertions. Other delimiter families, delimiter replacement, migration edits, multi-document fixes, registry-wide applicability audits, cancellation/race stress and performance/peak-memory budgets remain open.
