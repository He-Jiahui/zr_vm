---
related_code:
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_fragment.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_fragment.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/parser/test_expression_fragment_parser.c
  - docs/plans/lsp/04-debug-and-repl/2026-07-28-e2a-formal-expression-fragment-parser.md
doc_type: module-detail
---

# Formal Expression Fragment Parser

## Contract

`ZrParser_ParseExpressionWithState` parses exactly one expression using an
already initialized `SZrParserState`. The result is an owned `SZrAstNode` that
the caller releases through `ZrParser_Ast_Free`. It succeeds only when the
formal parser consumes the complete input and the lexer is at `ZR_TK_EOS`.

The entry point delegates directly to the existing internal `parse_expression`
precedence pipeline. Conditional expressions, calls, member access, literals,
and all other expression forms therefore keep the same AST and diagnostics as
normal source parsing. It does not copy or fork lexer, grammar, or AST rules.

## Diagnostics And Failure

Malformed operands keep the existing structured parser diagnostic path. For
example, `value +` invokes the existing missing-right-operand diagnostic and
the configured `structuredErrorCallback` observes an error. A syntactically
valid prefix followed by another token, such as `one two`, is rejected with the
existing parser error callback rather than silently accepting a prefix.

On any parser error or trailing token, the API releases an intermediate AST and
returns `ZR_NULL`; callers inspect the configured parser callbacks or
`SZrParserState.hasError`. There is no permissive partial-expression result.

## Debug Boundary

This module is the parser half of LSP 04 E2. It deliberately does not bind
debug locals, inject a receiver, resolve a `Place`, evaluate an expression, or
weaken type checking. E2b must consume this API and the canonical debug
evaluation context rather than extending `zr_vm_lib_debug` with another
recursive-descent grammar.

## Validation

`test_expression_fragment_parser.c` covers a full conditional expression,
structured diagnostics for a missing right operand, rejection of trailing
tokens, and the separate canonical external-binding identity projection. The
last case is documented in [Canonical external binding
injection](canonical-binding-injection.md). On 2026-07-28 GCC, Clang, and MSVC
each ran the target with `4 Tests`, `0 Failures`, and a real exit code of zero.
