---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_core/src/zr_vm_core/reflection.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_property.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_receiver_effect.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_core/src/zr_vm_core/reflection.c
plan_sources:
  - docs/plans/syntax/2026-07-18-05-property-unified-ast-design.md
  - docs/plans/syntax/05-property-unified-ast/m1-unified-ast-symbol-implementation-plan.md
tests:
  - tests/parser/test_property_unified_ast.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_semantic_query.c
doc_type: module-detail
---

# Type Inference Contracts

## Canonical Property Identity

M1 binds one visible property member and zero or more callable accessor members. The visible member
uses `ZR_SEMANTIC_SYMBOL_KIND_PROPERTY` and owns:

- `propertySymbolId` and `propertyIdentity`
- canonical `propertyValueTypeId`
- optional getter, setter, and initializer accessor SymbolIds
- source name, access, static/modifier flags, declaration node, and value type

Accessor members retain their callable `TypeId`, role, property SymbolId, and property identity.
Hidden runtime names are dispatch payload only. Semantic lookup first resolves the visible property,
then selects the linked accessor by exact SymbolId/role. Name-derived lookup is limited to importing
legacy/native artifacts that do not yet publish the M1 links.

## Validation And Receiver Effects

Binding rejects duplicate accessor kinds, `set` plus `init`, missing accessors, wider accessor
visibility, bodyless concrete accessors, and interface accessor bodies. Getter callable contracts use
a readonly receiver, setter and initializer contracts use a mutable/initializing boundary, and static
properties have no receiver effect. Setter/initializer expose one readonly compiler-bound `value`
parameter whose type is the property's canonical value type.

Read/write type inference consumes the visible property's structured value type. A read requires the
linked getter; a write requires the linked setter or initializer contract. It does not infer a pair by
matching `__get_` and `__set_` names.

## Serialization And Reflection

The serialized visible member carries `memberType`, source name, property identity, decorator
metadata, and property type name. Core reflection recognizes the shared appended AST constant and
creates the canonical `property` entry before accessor payload entries. Runtime decorators target
that visible property identity. Hidden accessor parsing remains a compatibility reader for older
artifacts, not a source-language semantic path.

## M1 Boundary

M1 establishes identity and callable contracts but does not claim the full M3 lowering matrix,
compound assignment ordering, ref-return Place projection, field/init-phase rules, or source/binary
LSP parity. Those stages must reuse these SymbolIds and TypeIds and may not reconstruct contracts from
member text.
