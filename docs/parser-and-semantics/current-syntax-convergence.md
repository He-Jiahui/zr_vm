---
related_code:
  - README.md
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_regions.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
plan_sources:
  - user: 2026-07-29 README.md is the current standard syntax
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
  - docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_property_ref_return.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/language_server/stdio_smoke.js
doc_type: module-detail
---

# Current Syntax Convergence

## Authority

`README.md` is the sole current-language source for examples and editor-facing
syntax. Historical percent forms belong to migration diagnostics, inventory, or
negative fixtures; they are not the target spelling for parser, LSP, extension,
or test updates.

The relevant current forms are:

```zr
module examples.hello;
let system = import("zr.system");

fn inspect(value: ref readonly Data): int { return value.count(); }
fn update(value: ref Data): void { value.reset(); }

var data = init Data();
update(ref data);
```

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

## Ownership Boundary Still Open

The parser preserves the declared target TypeRef: `ref readonly T` is read-only
and `ref T` is writable. The initial `ref place` expression currently enters
the ownership pipeline as a borrow construct, however, so a writable target
annotation has not yet been used to normalize that construct into a loan fact.
That is why the remaining work belongs in ownership/dataflow and escape
analysis, rather than in a second `ref` spelling or a return-parser rewrite.

The next semantic slice must derive the ownership fact from the enclosing
binding or return TypeRef, cover reborrows, and retain the dedicated
`return ref` provenance. It must then prove borrow/loan conflict and escape
diagnostics with the canonical forms above.

## Validation Status

`tests/parser/test_parser.c` covers current module syntax and the README
reference declaration forms. `tests/language_server/test_semantic_analyzer.c`
covers current parameter signature rendering, and the focused stdio diagnostic
smoke passes. The full stdio smoke is currently blocked before LSP analysis by
the shared native-registry builtin-registration failure; a VSIX built from that
dependency closure is therefore not an acceptance artifact.
