# Syntax 11 M5 formatter cutover acceptance

Date: 2026-08-02

Scope: formatter output behavior for the one-time syntax cutover. This record
does not claim external CompileTool execution, persistent incremental cache, or
Gate 11 M5 promotion.

## Accepted contract

- Full-document and range formatting invoke the existing structured legacy
  migration planner before producing an edit.
- A document with any registered migration item returns successfully with an
  empty edit set. The formatter never rewrites or re-emits `%compileTime`,
  `%func`, or another removed surface; migration code actions and the CLI own
  those edits.
- Canonical `#zr.compile.declarationTransform#`, `comptime fn`, and
  `import("@derive")` source remains formatable.
- `%` and `%=` remain ordinary current arithmetic operators. Spaced and
  adjacent forms such as `remainder % value`, `remainder%value`, and
  `remainder %value` do not create migration items and continue through the
  conservative indentation formatter.
- Both full and range formatting inspect the complete opened-document snapshot,
  so formatting a selected range cannot leak removed syntax through a partial
  scan.
- Migration planning failure is fail-closed: formatting returns failure rather
  than generating unchecked text.

## TDD evidence

The initial WSL GCC test replay had exactly one failure:

```text
LSP formatting does not emit removed syntax
formatter produced an edit containing removed syntax
```

The companion canonical test already passed, proving the failure was confined
to removed-syntax output. After the formatter reused
`ZrParser_LegacyMigration_PlanSource`, the complete advanced-editor executable
passed with zero failures. The canonical case includes `%`, `%=`, and
identifier-adjacent remainder forms to lock the operator/removed-keyword
boundary.

## Files and validation

- Production: `zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c`
- Focused cases: `tests/language_server/test_lsp_current_syntax_formatting_cases.h`
- Runner: `tests/language_server/test_lsp_advanced_editor_features.c`
- WSL GCC 11.4 direct exact-object/shared-library replay: zero failures.

This closes the scoped Gate 11 formatter consumer. Gate 11 M5 remains
`indirect` until ordinary external CompileTool activation, actual transitive
artifact verification, persistent cache integration, and remaining consumers
have direct evidence.
