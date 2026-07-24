# Syntax 13 M2 Yield Syntax And SemIR Implementation Plan

**Goal:** Add `yield expression;` as a normal statement inside an existing
`FunctionDefinition`, validate its explicit `zr.iteration.Iterator<T>` carrier
contract, and project suspension facts into pre-SemIR. This milestone does not
execute an iterator frame.

**Architecture:** `yield` gets a single AST node and a single lexer keyword.
The parser neither creates an iterator-function declaration nor reuses the old
`out` statement. Compiler validation resolves the declared carrier through the
canonical type environment and records `YieldValue`, `YieldSuspend`, and
`YieldResume` facts. Runtime frame allocation, bytecode dispatch, async
iteration, artifact serialization, and legacy generator migration remain later
milestones.

## Exact Initial Surface

- `zr_vm_parser/include/zr_vm_parser/lexer.h`
- `zr_vm_parser/src/zr_vm_parser/lexer.c`
- `zr_vm_parser/include/zr_vm_parser/ast.h`
- `zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h`
- `zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c`
- `zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c`
- `zr_vm_parser/src/zr_vm_parser/parser/parser_ast_free.c`
- `zr_vm_parser/src/zr_vm_parser/writer/writer_syntax_tree.c`
- `zr_vm_parser/include/zr_vm_parser/semantic_ir.h`
- `zr_vm_parser/src/zr_vm_parser/semantic_ir.c`
- `zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c`
- `zr_vm_parser/src/zr_vm_parser/compiler/iterator_semantic.c`
- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h`
- `zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c`
- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c`
- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c`
- `tests/CMakeLists.txt`
- `tests/iterator/test_yield_syntax.c`
- `tests/iterator/test_yield_semantic.c`
- `tests/iterator/test_iterator_semantic_ir.c`
- `docs/parser-and-semantics/iterator-yield-suspension.md`
- `docs/parser-and-semantics/index.md`
- `docs/plans/syntax/13-iterator-enumerator-yield/m2-yield-syntax-semir-implementation-plan.md`
- `docs/plans/syntax/13-iterator-enumerator-yield/m2-yield-syntax-semir.md`
- `tests/acceptance/2026-07-25-syntax-13-m2-yield-syntax-semir.md`

## Task 1: Lexer And AST RED

- [x] Add `tests/iterator/test_yield_syntax.c` and its CMake target.
- [x] Add RED cases for `yield value;`, a missing expression, a missing
  semicolon, `yield` outside a callable, a nested callable boundary, and the
  rejection of `iterator fn`.
- [x] Build and run the focused GCC target. The initial failure must be the
  absence of the `yield` token/AST contract, not an unrelated parser failure.

## Task 2: Parse A Normal Statement

- [x] Add one `ZR_AST_YIELD_STATEMENT` with exactly an expression and source
  range; do not add an iterator function AST, modifier, or function kind.
- [x] Register `yield` in the lexer and implement parsing in
  `parser_yield.c`; use the existing structured missing-semicolon diagnostic
  path.
- [x] Wire freeing and syntax-tree display, preserving existing `out` and
  `GENERATOR_EXPRESSION` behavior for the later migration milestone.
- [x] Make the parser tests green for valid syntax and recovery.

## Task 3: Canonical Carrier Validation

- [x] Add `iterator_semantic.c` as a narrow compiler helper. It must resolve
  the enclosing declared return TypeRef through the canonical type environment,
  require the resolved `zr.iteration.Iterator<T>` TypeId, and derive the
  element TypeId from that resolved carrier.
- [x] Reject `yield` in top-level code, property accessors, comptime/native/
  bodyless declarations, and nested functions whose own explicit carrier does
  not match. Do not compare source spelling, member names, or formatter text.
- [x] Reject `return expression;` in a function containing `yield`; permit only
  `return;` completion. Reject a yielded expression incompatible with the
  carrier element TypeId.
- [x] Keep ordinary non-yield function return checking unchanged.

## Task 4: Pre-SemIR Suspension Facts

- [x] Add `YIELD_VALUE`, `YIELD_SUSPEND`, `YIELD_RESUME`, and
  `ITERATOR_COMPLETE` semantic IR opcodes plus formatter/validation support.
- [x] Project every validated yield as value then suspend, with a resume target
  and canonical source ranges. `return;` in the same function projects exactly
  one completion fact.
- [x] Make the pre-SemIR test cover ordering and source ranges; add a borrow
  regression proving an active loan is visible across the suspension edge.
- [x] Do not emit executable iterator bytecode, allocate a frame, add a public
  helper, or change the normal callable signature in this milestone.

## Task 5: Evidence And Commit

- [x] Add the module document and index entry with YAML front matter, exact
  code/test paths, M2 boundary, and explicit no-runtime statement.
- [x] Run focused parser/semantic-IR targets in independent GCC, Clang, and
  MSVC directories. Record real exit codes, Unity totals, and baseline markers.
- [x] Update `m2-yield-syntax-semir.md` under `## 状态与产出记录` with status,
  start/finish time, completed work, contract, and evidence.
- [x] Stage only M2 exact paths through
  `GIT_INDEX_FILE=.git/index-syntax13-m2-stage` and commit one milestone.

## Non-Negotiable Boundaries

- [ ] No `iterator fn`, `generator` declaration, implicit `T -> Iterator<T>`
  return rewrite, `yield break`, `yield from`, `AsyncIterable`, or second
  function AST.
- [ ] No raw source-name/type-name/member-name fallback for carrier or element
  resolution.
- [ ] No iterator frame, runtime dispatch, async lowering, artifact ABI, LSP
  behavior, or legacy generator migration in M2.
