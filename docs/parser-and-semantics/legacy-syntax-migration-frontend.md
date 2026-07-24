---
related_code:
  - zr_vm_parser/include/zr_vm_parser/legacy_migration.h
  - zr_vm_parser/src/zr_vm_parser/migration/legacy_migration.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/legacy_migration.h
  - zr_vm_parser/src/zr_vm_parser/migration/legacy_migration.c
plan_sources:
  - docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md
  - docs/plans/syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes-implementation-plan.md
tests:
  - tests/parser/test_legacy_migration.c
  - tests/parser/test_property_consumer_contracts.c
  - tests/fixtures/syntax_migration_frontend/input/machine_forms.zr
doc_type: module-detail
---

# Legacy Syntax Migration Frontend

## Ownership

`ZrParser_LegacyMigration_PlanSource` is the only M2 producer of legacy migration candidates. It
returns ordered `SZrLegacyMigrationItem` facts carrying the exact source range, construct kind, target
kind, target-plan identifier, applicability, reason, optional related range, and an optional structured
edit. Consumers must not re-scan text, infer a candidate from a diagnostic message, or invent a
replacement from a member or type name.

The adapter has a small lexical state machine for code, line comments, block comments, quoted strings,
and backtick strings. It recognizes directives and legacy-looking constructor/callable forms only while
in code, so `%` modulo operators and text in comments or strings remain non-items. An unknown
`%identifier` is visible as `blocked`; it is never guessed into a rewrite.

## Applicability And Promotion

The report values are stable:

- `machineApplicable`: a token/structure proof and a current parser/compiler witness both exist;
- `maybeIncorrect`: a mechanically plausible edit lacks a complete proof;
- `requiresReview`: declaration, ownership, receiver, or type binding is required;
- `blocked`: no valid proposal can be emitted;
- `targetNotPromoted`: the target grammar or semantics belongs to a plan that has not passed its gate.

M2 deliberately publishes only `%owned -> resource` as a machine edit. The exact resource declaration
shell is verified by applying the plan, parsing the result, and compiling a `Unique<FileHandle>` plus
`own`/`drop` witness. `%module` is retained as `targetNotPromoted/06B` because the current parser still
accepts `%module` and rejects the target declaration spelling. Ownership calls, parameter passing
markers, callable arrows, `func`, `$Type(...)`, dynamic constructors, imports, and property forms are
reported without a write until a parser-owned semantic binding proves them safe.

The planner records the original FNV-1a source hash. `ZrParser_LegacyMigration_ApplyMachineEdits`
rejects a changed source or a plan marked as overlapping, applies non-overlapping edits from the right,
and returns a newly allocated output buffer. Replanning applied source retains non-machine facts but has
no machine edits, which is the relevant idempotence boundary before 06B.

## Property And LSP Boundary

Paired legacy property accessors remain owned by the existing parser property migration producer. The
adapter captures its `legacy_property_syntax` structured diagnostic and projects its exact fix when the
producer marks it machine-applicable; a scanner fallback is review-only only when no producer fact exists.
It never reconstructs or duplicates a property replacement.

06A does not inject adapter findings into normal LSP diagnostics. The formal parser migration diagnostic
is a 06B/M4 cutover responsibility; publishing it early would change diagnostics for currently accepted
repository source. Existing LSP structured diagnostic fixes, code actions, and revision-bound workspace
edit snapshots remain the future consumer path. They serialize a parser-provided machine fix and never
rebuild migration text in the language server.
