---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/project_imports.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_public_contract.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_common/include/zr_vm_common/zr_ast_constants.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/project_imports.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_public_contract.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_common/include/zr_vm_common/zr_ast_constants.h
plan_sources:
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/05-property-unified-ast/m1-unified-ast-symbol-implementation-plan.md
  - docs/plans/syntax/05-property-unified-ast/m2-explicit-field-init-implementation-plan.md
tests:
  - tests/parser/test_property_unified_ast.c
  - tests/parser/test_property_explicit_field_init.c
  - tests/parser/test_parser.c
doc_type: module-detail
---

# AST And Syntax Contracts

## Unified Property Grammar

Class, struct, resource class, and interface members use one contextual property grammar:

```zr
pub property value: int {
    get { return this._value; }
    pri set { this._value = value; }
}
```

`property`, `get`, `set`, and `init` remain contextual identifiers. The lexer does not reserve them
globally. A property owns its decorators, visibility, static/modifier flags, name, type, and ordered
accessor array. Each accessor records its kind, optional visibility override, keyword range, body
kind, and body node.

Accessor bodies have exactly three forms:

- bodyless contract: `get;`
- expression body: `get => expression;`
- block body: `get { ... }`

Concrete containers require accessor bodies. Interface accessors are bodyless in M1. `set` and
`init` bodies receive compiler-bound contextual `value`; outside those bodies, `value` remains an
ordinary identifier.

## Container-Neutral AST

The parser emits `ZR_AST_PROPERTY_DECLARATION` with ordered
`ZR_AST_PROPERTY_ACCESSOR` children for every supported container. Container selection is parser
validation context and does not select a different property AST shape. The property declaration and
accessor enum values are appended, preserving existing serialized AST numbers.

AST free, import projection, and syntax-tree writer traverse the declaration and accessor children
directly. They do not pair names or synthesize getter/setter nodes.

The legacy `ZR_AST_CLASS_PROPERTY`, `ZR_AST_INTERFACE_PROPERTY_SIGNATURE`,
`ZR_AST_PROPERTY_GET`, and `ZR_AST_PROPERTY_SET` layouts remain numerically available for old
readers and explicit migration tests. Production parsing does not emit them, and the compiler rejects
manually supplied legacy property nodes as semantic sources.

## Explicit Bindings And Fields

`let` and `var` are the canonical binding surface for locals, `for`/`foreach` bindings, and explicit
class, struct, resource-class, and interface fields. `let` projects to the existing immutable binding
fact; `var` projects to mutable storage. The final 06B cutover rejects `var const` in local, class, and
struct positions with a directed migration diagnostic. Bare module `const` remains the distinct
current module-constant contract. Writer/debug output reports the canonical binding kind from the AST
fact rather than preserving removed source spelling.

Object/array destructuring and foreach destructuring register every bound identifier with the same
binding kind; `let {x}`, `let [x]`, and `for (let {x} in values)` therefore cannot be reassigned.
Variable and field declaration ranges start at the exact `let`/`var` compatibility keyword after
visibility/decorator modifiers. The syntax writer traverses class/struct/interface members and prints
field names from their containing AST nodes, never by casting an embedded `SZrIdentifier` to a node.

Only an explicit field declaration creates field identity or storage. A property declaration and its
accessor children are callable contracts; the parser never synthesizes a backing field, pairs a field
by name, or changes a field declaration into a property node. `let` is appended at the lexer token
enum tail, preserving all pre-existing token ids.

## Recovery Boundary

Property recovery is scoped to the declaration body and accessor terminators. A half-written
accessor may report an exact property/accessor range while preserving later members when a stable
boundary exists. Recovery never converts an unrelated identifier named `property`, `get`, `set`,
`init`, or `value` into a declaration.

## Removed Source-Intermediate Contract

User-authored `intermediate ... % ...` declarations are removed syntax. The final 06B cutover deleted
their five AST enum names and payloads, parser helpers, import projection, writer labels, semantic
branch, and CLI compiler consumer. The enum numbers remain unassigned gaps so later serialized AST
values do not move. Public-contract wire value 13 has a named removed-value constant and is rejected
explicitly; it must never be treated as a current declaration or accepted to preserve an old artifact.

## M2 Boundary

M2 adds explicit binding and initialization contracts without adding implicit storage. Full typed
accessor-call lowering, virtual/interface dispatch, compound evaluation ordering, ref-return Place
projection, and LSP reconstruction remain M3 through M5 work. Those stages must consume the same
property declaration and explicit field identities rather than add another syntax representation.
