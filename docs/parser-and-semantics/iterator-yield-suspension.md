---
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/iterator_semantic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/compiler/iterator_semantic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
plan_sources:
  - docs/plans/syntax/2026-07-20-13-iterator-enumerator-yield-design.md
  - docs/plans/syntax/13-iterator-enumerator-yield/m2-yield-syntax-semir-implementation-plan.md
tests:
  - tests/iterator/test_yield_syntax.c
  - tests/iterator/test_yield_semantic.c
  - tests/iterator/test_iterator_semantic_ir.c
doc_type: module
---

# Iterator Yield Suspension

Syntax 13 M2 adds `yield expression;` as a normal statement. It is not an
iterator-function modifier and does not create a second function declaration
form. `iterator fn` is rejected explicitly; the function remains an ordinary
`fn` whose declared return carrier supplies the iterator contract.

## Carrier And Return Contract

The containing function must declare a canonical
`zr.iteration.Iterator<T>` return TypeId. The compiler resolves that TypeId and
its element TypeId through the type environment and protocol metadata; it never
accepts source spelling, formatter text, member names, `Iterable<T>`, or a raw
generic argument as a substitute.

`yield value;` must be compatible with the resolved `T`. It is rejected at
top level, in accessors, in non-Iterator nested functions, and in async
functions. A function that contains its own `yield` accepts `return;` as its
completion form but rejects `return expression;`. A nested callable is a
boundary: its `yield` neither turns the enclosing function into an iterator nor
changes the enclosing function's return rules.

## Pre-SemIR Contract

Each validated yield records these canonical facts, in order:

1. `YIELD_VALUE` with the resolved element TypeId and a fresh semantic ValueId.
2. `YIELD_SUSPEND` carrying that same ValueId.
3. `YIELD_RESUME` carrying that same ValueId.

Function completion records one `ITERATOR_COMPLETE` fact. All yield facts use
the `YieldStatement` range. Carrying the ValueId through suspend and resume
lets existing LoanId liveness retain an active borrow across the suspension
edge without a text-based exception.

M2 intentionally emits no executable iterator bytecode, allocates no iterator
frame, and adds no runtime dispatch, async lowering, artifact ABI, public
runtime helper, LSP behavior, or legacy generator migration. Those facilities
remain later milestones.
