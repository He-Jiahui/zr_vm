---
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/iterator_semantic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_body.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_body.h
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_child_function_relocation.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_child_function_relocation.h
  - tests/iterator/yield_public_compile_graph_cases.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/compiler/iterator_semantic.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_body.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_body.h
  - zr_vm_parser/src/zr_vm_parser/compiler/declaration/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_child_function_relocation.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_child_function_relocation.h
plan_sources:
  - docs/plans/syntax/2026-07-20-13-iterator-enumerator-yield-design.md
  - docs/plans/syntax/13-iterator-enumerator-yield/m2-yield-syntax-semir-implementation-plan.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
tests:
  - tests/iterator/test_yield_syntax.c
  - tests/iterator/test_yield_semantic.c
  - tests/iterator/yield_function_body_cases.inc
  - tests/iterator/yield_public_compile_graph_cases.inc
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

## Public Compile Graph Probe

Function declaration lowering now delegates its body, out-parameter definite
assignment check and implicit iterator completion to a private function-body
module. The declaration prepares and restores the same parameter, type and IR
scopes as before. Its child IR is disposed when that isolated scope ends;
reading the restored parent IR cannot establish the child's yield facts.

The scoped semantic tests use a real parsed declaration and parameter type
environment, invoke this same body entry and copy fact counts and source
offsets before disposal. Nested facts belong to the actual inner declaration.
Public compile graph checks remain separate, including the ordinary outer
return and zero executable instructions in the generator child. Native
validation of the scoped test correction is pending.

The MSVC public compile probe reports seven tests, four failures and zero
ignored. The new three-yield case fails the public child-graph validator:
`CREATE_CLOSURE target outside childFunctions graph`. The copied child is a
generator with zero executable instructions; its constant still points at the
detached source function. Three original pre-SemIR assertions also fail and
remain separate from this graph defect. The log is
`m1-msvc-yield-graph-red-r1-zr_vm_yield_semantic_test.log`, SHA256
`AC27DBE98C27B313D3937F9B98B4E096A0DA85831C4AE967650E80E569DEBC0E`,
5601 bytes.

The assembly candidate uses the actual source-child pointer table to relocate
owned raw-function constant slots after all children are copied and before the
source buffers are detached. It skips borrowed nodes and shared closure objects;
the existing identity rebinding and graph validator remain in place. The
new public compile case now passes. The complete MSVC suite reports seven
tests, three failures and zero ignored; the three original pre-SemIR failures
remain unchanged. Module 11 and pipeline 18 companion cases all pass. The new
yield log is `m1-msvc-yield-graph-green-r1-zr_vm_yield_semantic_test.log`, SHA256
`6142C99C4F87BDFDDD24D810914DB7530E64F5874B82B0C18796230B476CE053`,
4427 bytes. This validates child-graph preservation and establishes no iterator
runtime behavior or milestone acceptance.

## Scoped Body And Public Graph Replay (2026-10-14)

The bounded MSVC lower-contract replay reports seven tests, zero failures and
zero ignored. The shared function-body scope tests preserve all four yield
facts and their source ranges by copying observations before the child IR is
disposed; nested facts remain attached to the inner declaration. The public
compile case also passes with a zero-instruction generator child and the
ordinary outer return intact. This validates these focused compiler contracts,
not iterator runtime execution or M1 acceptance. The log is
`m1-msvc-m1-lower-contracts-r1-zr_vm_yield_semantic_test.log`, SHA256
`E2357CED55C36C0C888443DF30415AAC1EB426893758685C856A786B8A7E8936`,
5225 bytes.
