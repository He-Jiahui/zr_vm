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

After the one-shot cutover, production parsing rejects every removed percent directive and `$`
constructor form with a fatal `legacy_syntax_removed` diagnostic. Migration remains a separate,
explicit frontend. Structurally proven `%module`, `%owned`, `%type`, and other promoted forms may carry
machine edits; semantic ownership, receiver, callable, and static-constructor choices remain
`requiresReview` or `blocked` until their binding is proven.

The dynamic `$(target)(arguments)` adapter consumes the complete balanced target and argument ranges.
When both are structurally complete it proposes
`reflection.requireConstructible(target).createInstance(...[arguments])` as a
`ZR_DIAGNOSTIC_FIX_MAYBE_INCORRECT` edit. The ordinary machine-edit applicator never applies it. The
reviewer must confirm that `target` is `zr.reflection.Type` and that argument boxing is correct. Missing
or unbalanced target/argument groups are `blocked`. The migration frontend does not create a legacy
prototype-reference AST and never rewrites a runtime type expression into `init`.

The planner records the original FNV-1a source hash. `ZrParser_LegacyMigration_ApplyMachineEdits`
rejects a changed source or a plan marked as overlapping, applies non-overlapping edits from the right,
and returns a newly allocated output buffer. Replanning applied source retains non-machine facts but has
no machine edits, which is the relevant idempotence boundary before 06B.

## Property And LSP Boundary

Paired legacy property accessors remain owned by the existing parser property migration producer. The
adapter captures its `legacy_property_syntax` structured diagnostic and projects its exact fix when the
producer marks it machine-applicable; a scanner fallback is review-only only when no producer fact exists.
It never reconstructs or duplicates a property replacement.

Normal parsing owns the fatal removed-syntax diagnostic; the migration command owns review candidates.
Existing LSP structured diagnostic fixes, code actions, and revision-bound workspace-edit snapshots
serialize parser-provided edits and never rebuild migration text in the language server. This split
keeps removed syntax out of production AST/lowering while preserving an explicit migration workflow.
