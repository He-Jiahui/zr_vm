---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task06-function-call-mismatch-query-projection.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-method-call-mismatch-query-projection.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_type_mismatch_diagnostic_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/stdio_diagnostic_fix_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-method-call-mismatch-query-projection.md
doc_type: module-detail
---

# Canonical Receiver Call Diagnostics

## Purpose

Receiver method-call compatibility is a parser/type-inference rule. The
language server must not resolve a receiver type through its symbol table,
pair methods by name, infer argument types again, or construct a second
`type_mismatch` diagnostic. This module boundary keeps overload selection,
ownership compatibility, exact source identity, and diagnostic policy in the
parser/compiler layer, then exposes the resulting persistent fact through the
semantic diagnostic query.

## Behavior Model

Member resolution evaluates candidates without publishing diagnostics during
the scan. For the first structurally valid candidate whose value parameter is
incompatible, it records the exact `SZrTypeMemberInfo`, parameter index, and
canonical expected and actual inferred types. A diagnostic is emitted only
after no candidate can be selected. This prevents a rejected overload from
leaking a diagnostic when a later overload is valid.

The direct receiver-call validator follows the same policy. It first asks the
canonical ownership diagnostic producer to classify qualifier or passing-mode
failures. Only a plain value-type incompatibility proceeds to the detailed
argument mismatch producer. Generic binding, arity, unresolved member, and
other call failures retain their existing structured paths.

## Range And Fix Contract

The primary range is the exact argument expression. A parser-generated primary
wrapper with no member chain is unwrapped to its property range so punctuation
and wrapper coordinates do not widen the diagnostic. The related range is the
declared parameter type token from the exact class, struct, or interface method
declaration carried by `SZrTypeMemberInfo.declarationNode`.

The shared detailed type-error builder owns descriptor `2011`, code
`type_mismatch`, canonical message/cause/suggestion text, related information,
and the typed `<expected-type> <expression>` placeholder fix. The LSP layer
does not reconstruct any of these fields.

## Data Flow

1. Type inference resolves receiver and member identity and maps arguments to
   canonical parameter slots.
2. Candidate scanning records the first exact mismatch but does not emit it.
3. After selection fails, ownership mismatch policy runs before ordinary type
   mismatch policy.
4. Parser/compiler publishes the structured diagnostic as a persistent semantic
   query fact.
5. `semantic_analyzer_typecheck.c` invokes parser inference and projects the
   query result. It has no method-name scan, parameter matcher, or direct method
   `type_mismatch` producer.

If exact source declaration identity is unavailable, the detailed producer
returns unavailable and the existing parser fallback remains responsible for
the compiler error. LSP never substitutes a member-name, AST-text, or display-
text reconstruction.

## Edge Cases And Constraints

- Ownership failures must preserve their specific ownership descriptor and
  no-fix disposition; they cannot be downgraded to descriptor `2011`.
- Named and positional arguments use the parser's argument-to-parameter mapping.
- Overload candidate order may choose which rejected candidate is retained for
  the final error, but no candidate emits while another can still succeed.
- Binary/native declarations without exact source parameter ranges cannot gain
  fabricated related locations in LSP.
- Repeated semantic-query projection must not add a duplicate analyzer-owned
  diagnostic.

## Test Coverage

Parser tests freeze the exact argument and declared-parameter ranges, descriptor
`2011`, related information, and one typed placeholder fix. LSP tests verify the
same fields, source-contract tests prohibit the deleted method checker and
parameter matcher, golden parity compares compiler and LSP diagnostics from one
snapshot, and stdio coverage verifies protocol serialization. Existing method
ownership tests protect ownership-first classification.

## Follow-Up

Plan 03 Task 6 remains active for analyzer-owned rules outside receiver method
calls. New call diagnostics must extend parser/compiler facts rather than add a
language-server compatibility checker.
