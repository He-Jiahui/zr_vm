---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_call_arguments.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_argument_mapping_cases.h
doc_type: module-detail
---

# Canonical Call Argument Mapping

## Purpose

Call consumers need an exact argument-to-parameter relation without re-reading source text or
reconstructing a signature from an AST. The parser semantic snapshot therefore stores mapping rows
beside the selected callable reference fact and exposes them through
`ZrParser_SemanticQuery_CallAt` as a borrowed view.

## Behavior Model

Each dense mapping row records the argument and parameter indexes, whether the argument was named,
the canonical argument and parameter `TypeId`, the parameter passing mode, the exact or implicit
conversion, and the source range of the argument expression. Rows are built only after overload
resolution has produced one `SZrResolvedCallSignature`.

Source call syntax keeps `ref` and `out` markers in `SZrFunctionCall.argumentMarkers` as
`SZrCallArgumentSyntax`. The mapping producer merges an explicit marker's structured
`markerLocation` with the argument AST range. Consequently `ref value` and `out value` are reported
as complete argument ranges. Named labels remain outside the expression range, matching positional
and named-call behavior. An `in` parameter requires no call-site marker, so its range remains the
argument expression itself while its passing mode comes from the resolved parameter contract.

## Data Flow

1. `parser_call_arguments.c` parses optional `ref`/`out` markers and stores their exact ranges.
2. Type inference resolves the selected callable and its closed parameter types and passing modes.
3. `type_inference_call_argument_semantic_facts.c` binds each argument to one parameter and emits a
   complete mapping row. Spread calls do not publish approximate one-to-one rows.
4. The reference fact owns a deep-copied mapping array in the semantic snapshot.
5. `CallAt` validates the rows against the selected canonical callable before returning a borrowed
   pointer. A malformed non-empty payload makes the query fail closed and clears its output.

## Design Rationale

Marker ranges are consumed from parser structures rather than recovered by scanning source bytes.
This preserves UTF and snapshot identity rules and keeps language syntax ownership below the LSP.
Passing mode is taken from the selected callable contract rather than inferred from the marker alone:
the marker describes the call-site form, while the contract defines parameter semantics.

The query never repairs a missing or contradictory row. Consumers that retain data beyond the
semantic snapshot lifetime must copy stable ids and ranges instead of retaining the borrowed array.

## Edge Cases

- Missing marker metadata falls back to the exact argument AST range, not source text inspection.
- Named argument labels are not part of `argumentRange`.
- Every published non-spread argument must have valid canonical type ids and a known conversion.
- Parameter bindings must be unique and agree with the selected callable's canonical contract.
- Receiver/member and binary/native callable parity remain separate Plan 03 work.

## Test Coverage

`test_semantic_query_call_argument_mapping_cases.h` covers named reorder, exact and implicit
conversion, malformed snapshot rejection, and source `in`/`ref`/`out` passing modes. The passing-mode
case also verifies that `hasCallInfo` is present and explicit marker ranges include the complete
`ref value` or `out value` syntax.

## Plan Sources

This module contract implements Plan 03 Task 4's structured argument-to-parameter mapping and the
Task 4.24 source passing-marker range correction.
