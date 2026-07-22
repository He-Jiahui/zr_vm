---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/project_imports.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/project_imports.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c
plan_sources:
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/05-property-unified-ast/m1-unified-ast-symbol-implementation-plan.md
tests:
  - tests/parser/test_property_unified_ast.c
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

## Recovery Boundary

Property recovery is scoped to the declaration body and accessor terminators. A half-written
accessor may report an exact property/accessor range while preserving later members when a stable
boundary exists. Recovery never converts an unrelated identifier named `property`, `get`, `set`,
`init`, or `value` into a declaration.

## Deferred Work

M1 does not add implicit storage, field initialization rules, typed accessor-call lowering,
ref-return properties, or LSP reconstruction. Those remain M2 through M5 contracts and must consume
the same property declaration identity rather than add another syntax representation.
