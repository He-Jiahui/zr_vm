---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_cast.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_cast.h
tests:
  - tests/parser/test_cast_operand_facts.c
  - tests/language_server/test_lsp_cast_operand_facts.c
  - tests/language_server/stdio_smoke.js
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/2026-09-07-plan01-task06-sub06-cast-operand-semantic-facts.md
doc_type: module-detail
---

# Cast Operand Semantic Facts

## Analysis and Exactness

The cast expression inference path analyzes its operand before converting the
explicit target annotation to the result type. The operand keeps its own
expression TypeId, resolved call target, and external metadata identity. Nested
casts perform the same analysis at every level. A target type does not overwrite
the operand's return type or establish a missing call target.

An operand that cannot be inferred does not erase the explicit cast target type.
The inference path retains diagnostics and unresolved facts from the operand;
it does not clear compiler errors or fabricate a resolved symbol. This preserves
the existing cast result contract without adding conversion legality rules.
The syntax plan's pending surface decisions remain independent of this analysis
coverage fix.

## Ownership and Lifetime

The private cast helper initializes and frees one temporary inferred type for
the operand. Semantic facts, strings and canonical IDs are owned by the compiler
semantic context and remain available after that temporary is released. The
caller owns the cast result type. AST pointers and numeric IDs are valid only
within their semantic snapshot.

LSP consumers read the published facts. Definition requests do not reinfer the
cast, inspect its target type to guess a callable, or reconstruct external
identity from a name. External identity is compared inside one analyzer in the
regression tests; numeric SymbolIds are not compared across separate snapshots.

## Validation and Reference

Parser regressions cover distinct operand/result types, nested casts, a resolved
local call and an unknown call. LSP regressions compare an uncast call with a
cast call for binary-only and native providers, including their canonical
identity and exact definition URI/range. The original stdio smoke exercises a
binary export inside an integer cast.

The local Roslyn reference is `BindCast` in
`lua/roslyn/src/Compilers/CSharp/Portable/Binder/Binder_Expressions.cs`: it binds
the operand value before the target type and conversion. ZR uses that traversal
principle for its existing cast form; this change does not adopt C# conversion
rules.
