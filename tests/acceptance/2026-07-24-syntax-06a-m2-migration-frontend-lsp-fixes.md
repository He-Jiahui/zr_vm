# Syntax 06A M2 Migration Frontend + LSP Fixes Acceptance

- Date: 2026-07-24 18:09 +08:00
- Plan: `docs/plans/syntax/06-percent-migration-lsp-fixtures/m2-migration-frontend-lsp-fixes-implementation-plan.md`
- Baseline: `e5b3b7d` (`feat(syntax): inventory legacy migration forms`)
- Decision: accepted

## Delivered Contract

- `ZrParser_LegacyMigration_PlanSource` produces ordered, token-aware migration facts and masks line
  comments, block comments, quoted strings, backtick strings, and modulo uses.
- The only M2 machine edit is `%owned -> resource`; apply revalidates source hash, rejects overlap,
  applies right-to-left, reparses/typechecks, and is idempotent. `%module` is
  `targetNotPromoted/06B`; all other inventory families retain a non-writable applicability fact.
- `zr_vm_cli migrate syntax <path> --check|--write --format json|text` emits deterministic JSON/text,
  keeps `--check` read-only, and validates before atomic writes. Recursive migration excludes `bin`,
  `golden`, `generated`, and `.codex` unless `--include-generated` is passed.
- No migration diagnostic is injected into normal LSP documents in 06A. Existing parser-owned structured
  fix, code-action, and stale workspace-edit snapshot contracts remain the future 06B projection path.

## RED To GREEN Evidence

- Parser RED began with no public `legacy_migration` API; production implementation then satisfied eight
  focused Unity cases: deterministic classification, lexical masking, inventory applicability, edit
  idempotence, current-parser/compiler witness, and stale/overlap rejection.
- CLI RED began with no `migrate syntax` parser/runner. The completed CLI tests cover invalid argument
  combinations, read-only check mode, deterministic JSON, machine-only write, and a second no-edit run.
- An initial speculative LSP migration-diagnostic projection failed current-source interface contracts;
  it was removed. This fixes the non-cutover boundary rather than adding an LSP-only rewrite.
- Review added direct-file exclusion, collision-safe exclusive temporary creation, non-shell `%owned`,
  canonical Windows path, and paired-property producer regressions. `--write` now rejects an excluded
  direct file, preserves a pre-existing legacy temporary name, and imports the parser producer's exact
  property edit rather than reconstructing it.
- MSVC reproduced two platform defects during the final matrix: absent `ZR_LANGUAGE_SERVER_API` export
  declarations for document-aware conversion helpers and double-backslash directory exclusion matching.
  The existing LSP interface executable and recursive/direct generated-directory smoke passed after the
  narrow fixes. The Windows CTest runner now wraps a quoted CLI executable with explicit `cmd.exe /d /s /c`.

## Final Matrix

Each isolated build executed these targets:

```text
zr_vm_legacy_migration_test
zr_vm_property_consumer_contracts_test
zr_vm_cli_args_test
zr_vm_cli_syntax_migration_test
zr_vm_cli_executable
zr_vm_language_server_lsp_interface_test
zr_vm_language_server_stdio
zr_vm_descriptor_plugin_fixture_int
zr_vm_descriptor_plugin_fixture_float
```

Each toolchain then ran CTest regex
`^(legacy_migration|property_consumer_contracts|cli_args|cli_syntax_migration)$`, the direct LSP interface
executable, `tests/cli/syntax_migration_smoke.js`, and `tests/language_server/stdio_smoke.js`.

| Toolchain | Isolated build | Result |
| --- | --- | --- |
| GCC 11.4 | `.codex/build-syntax06a-m2-gcc` | CTest 4/4, LSP interface, CLI smoke, and stdio smoke: exit 0 |
| Clang 14.0.0 | `.codex/build-syntax06a-m2-clang` | CTest 4/4, LSP interface, CLI smoke, and stdio smoke: exit 0 |
| MSVC 19.44.35228 | `.codex/build-syntax06a-m2-msvc` | CTest 4/4, LSP interface, CLI smoke, and stdio smoke: exit 0 |

The final MSVC environment was loaded through `VsDevCmd`; `where cl` resolved
`E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`. Node 22.13.1 emitted only its
existing `fs.rmdir(..., { recursive: true })` deprecation warning; both smoke processes returned zero.

## Residual Boundary

No 06B grammar cutover, formal parser migration diagnostic, compiler fallback, whole-repository rewrite,
fixture/golden rewrite, artifact migration, or LSP text-based candidate reconstruction is included in M2.
