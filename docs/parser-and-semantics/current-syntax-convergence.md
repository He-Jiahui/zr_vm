---
related_code:
  - README.md
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_function_syntax.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/migration/legacy_migration.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_public_contract.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_common/include/zr_vm_common/zr_ast_constants.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_regions.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_function_syntax.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/migration/legacy_migration.c
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_public_contract.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_common/include/zr_vm_common/zr_ast_constants.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
plan_sources:
  - user: 2026-07-29 README.md is the current standard syntax
  - user: 2026-07-30 strictly perform a one-shot breaking syntax cutover
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
  - docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_percent_syntax_cutover.c
  - tests/parser/test_legacy_migration.c
  - tests/cli/test_cli_syntax_migration.c
  - tests/parser/test_property_ref_return.c
  - tests/parser/test_type_inference.c
  - tests/library/test_native_binding_direct_call.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-07-26-syntax-55-status-records-review.md
  - tests/acceptance/2026-08-03-syntax-08-m1-reflection-provider-contract.md
  - tests/acceptance/2026-08-05-syntax-06b-repository-promotion.md
  - tests/acceptance/2026-08-05-syntax-07b-current-reference.md
doc_type: module-detail
---

# Current Syntax Convergence

## Authority

`README.md` is the user-facing authority, while
`docs/zr_language_specification.md` records the production grammar boundary
and `tests/fixtures/projects/syntax_reference_v1` is its executable catalog.
Historical percent forms belong only to migration diagnostics, inventory,
negative fixtures, or historical design documents. The production parser does
not accept them as a compatibility surface.

The relevant current forms are:

```zr
module examples.hello;
let system = import("zr.system");

fn inspect(value: ref readonly Data): int { return value.count(); }
fn update(value: ref Data): void { value.reset(); }

var data = init Data();
update(ref data);
```

## One-Shot Cutover Boundary

The current frontend has one production grammar. In particular, it does not
accept `%module`, `%import`, `%extern`, `%compileTime`, `%test`, `%owned`,
`%borrow`, `%loan`, `%unique`, `%shared`, `%weak`, `%func`, `%in`, `%out`,
`%ref`, or `%using` as language keywords. Their replacements are ordinary
tokens and typed contracts:

- module declarations use `module path;`;
- static imports use `import("path")`;
- native declarations use `native extern("library") ...`;
- build-time execution uses `comptime` and `zr.compile` typed descriptors;
- ownership uses `Unique<T>`, `Shared<T>`, `Weak<T>`, `ref`, direct owner
  operations, and statement/block `using`;
- definitions use `fn(args): ReturnType`, while callable types use
  `fn(Args) -> ReturnType`.

There is no parser fallback from a rejected percent form to an old AST or
compiler lowering path. The explicit migration frontend may still recognize
legacy input to produce diagnostics and edits, but it never turns that input
into a production compilation unit. Ordinary `%` remains the modulo operator.
An unknown `%identifier` is an ordinary syntax error rather than an implicit
migration directive.

Known removed spellings deliberately remain as parser token comparisons so a
user receives the directed fatal `legacy_syntax_removed` diagnostic. Every
such branch returns no AST. Their presence in production source is therefore
a rejection contract, not a compatibility grammar.

The final 06B audit found a distinct compatibility route for user-authored
`intermediate ... % ...` input. Its parser helpers, five source AST kinds and
payloads, project-import projection, syntax writer, semantic branch, and stale
CLI compiler consumer were deleted. Their numeric AST slots remain gaps so
later values do not move, and public-contract wire value 13 is rejected
explicitly rather than deserialized as a source declaration.

The obsolete standalone `$` prototype-reference parser entry has been removed.
Canonical `init/new/own` still use their internal typed construct-target node;
that implementation detail is not a legacy source grammar path.

Removed qualified names are not retained as hidden type-inference aliases.
`zr.system.reflect.Type` and `zr.system.reflect.CallableType` fail through
ordinary module resolution; production type inference no longer maps either
name to `zr.builtin.TypeInfo`. Migration tooling may explain historical input,
but it does not install a compatibility type binding.

## Implemented Frontend Surface

- `module path;` reaches the normalized module-declaration parser and becomes
  `script.moduleName`.
- `import("module.path")` is parsed as a static import expression; it no
  longer receives a legacy-syntax diagnostic.
- `ref place` and `ref existingRef` are parsed as reference construct
  expressions. `ref ref place` is rejected with the directed reborrow form.
- Parameter display in semantic hover and completion follows the source form:
  `value: ref T`, `value: ref readonly T`, `value: in T`, and `value: out T`.
- `return ref place;` remains a dedicated return-statement contract. The AST
  records `isReferenceReturn` and its source range, which the property return
  compiler already consumes; it must not be lowered indiscriminately into an
  ordinary reference expression.

The parser preserves the declared target TypeRef: `ref readonly T` is read-only
and `ref T` is writable. Borrow/loan, escape, suspension, owner receiver, and
artifact consumers are covered by their dedicated lower-layer suites. Future
work must extend those typed contracts; it must not restore a second source
spelling.

## Validation Status

The breaking switch is directly covered by `percent_syntax_cutover`,
`cli_syntax_migration`, and `legacy_migration`: old source is rejected by the
production path while migration diagnostics and edits remain available. A
static audit permits listed legacy literals only in rejection diagnostics,
migration tooling, explicit negative inputs, and historical records. The
2026-07-30 WSL GCC isolated matrix built the full tree and passed all 123 then
registered CTest tests, including `language_pipeline`, projects,
language-server stdio, VM/AOT, debug, and migration consumers.

This closes the dual-parser compatibility question, not the complete Syntax
redesign. The upper-gate ledger remains open until every owner gate has direct
coverage.

The 2026-08-03 Gate 08 M1 replay revalidated parser 74/74 and percent cutover
6/6 under GCC, Clang, and MSVC. The broader identical matrix was 395/395 per
toolchain. It also found and migrated five stale native-direct-call fixtures
that still declared functions without `fn`; no keywordless-function parser
fallback was restored.

The final 2026-08-05 replay reports 6/6 strict cutover, parser 74/74,
semantic-query 27/27, and current-reference 13/13. Inventory scanner v3 scans
the current language specification and reports zero findings, zero unknown,
14 stable allowlisted negative/migration inputs, and 598 separately reviewed
current call/new candidates. The canonical status selector remains
`TOTAL=55 MISSING=0`; 06B and 07B have independent root-promotion acceptance
records rather than being inferred from those leaf statuses.
