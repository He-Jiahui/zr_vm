---
related_code:
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/migration/migration.h
  - zr_vm_cli/src/zr_vm_cli/migration/migration.c
  - zr_vm_parser/include/zr_vm_parser/legacy_migration.h
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/migration/migration.c
plan_sources:
  - docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md
  - docs/plans/syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes-implementation-plan.md
tests:
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_syntax_migration.c
  - tests/cli/syntax_migration_smoke.js
  - tests/fixtures/syntax_migration_frontend/expected/machine_forms.json
doc_type: command-reference
---

# Syntax Migration Command

## Invocation

```text
zr_vm_cli migrate syntax <path> --check --format json
zr_vm_cli migrate syntax <path> --write --format text
```

Exactly one of `--check` and `--write` is required. Formats are `json` and `text`; JSON is the default.
`--language-from legacy --language-to current` is accepted only as the fixed migration direction.
`--include-generated` opts into otherwise excluded generated roots. Run, compile, debug, profiling,
coverage, project-module, and output modifiers are rejected for this standalone mode.

## Reports

JSON reports use schema version 1. Each item contains `diagnosticCode`, `file`, byte `range`, old and
target construct kinds, old binding kind, optional resolved type identity, applicability, target plan and
promotion gate, `edits`, related declarations, and a stable reason. Paths are emitted with the supplied
or normalized source path; a directory invocation prints one deterministic report per eligible `.zr`
file in library-directory order.

`--check` reads and reports only. `--write` considers only `machineApplicable` items. In M2 that is the
verified `%owned -> resource` declaration-shell edit; report-only items stay byte-identical. The golden
fixture freezes a `%module` target gate beside that safe resource edit.

## Write Guard

For each eligible `.zr` file, the runner records the parser plan, rereads the source immediately before
writing, rejects byte changes, applies the parser-owned plan, parses and compiles the candidate with the
standard modules registered, then writes a temporary sibling and atomically replaces the original. A
failed revalidation, parser/compiler check, or replacement leaves that source unchanged.

Directory traversal is recursive and deterministic. By default, paths below `bin`, `golden`,
`generated`, and `.codex` are skipped. `--include-generated` permits generated text sources, but it
does not bypass the same hash, overlap, parser/compiler, or machine-applicability checks. Non-`.zr`
files are never selected.
