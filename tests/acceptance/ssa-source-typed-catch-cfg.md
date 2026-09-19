# SSA source typed-catch CFG acceptance

## Scope

This phase connects one bounded source typed catch to canonical type identity.
The admitted form keeps all existing single-catch restrictions: no `finally`,
one resolved direct protected call, and only the already-modeled catch bodies.
Its annotation must be a simple primitive or already-resolved named type with
no qualification, generic arguments, array shape, ownership qualifier, or
reference qualifier.

The protected call's exceptional edge enters a dispatch block. That block:

1. defines the active exception once with `EXCEPTION_PAYLOAD`;
2. emits `TYPE_TEST(payloadValueId, matchTypeId)` with a boolean result TypeId;
3. branches true to the matching catch body; and
4. branches false to a zero-successor `THROW` that consumes the original
   payload ValueId.

The matching handler initializes the catch Place with the resolved annotation
TypeId and then lowers the supported body. Only the dispatch block is an
exception block; the matching handler and unmatched propagation sink are
ordinary successors. No source spelling is retained for comparison.

The producer emits valid SemanticIR and ExecIR, but the graph is not yet an
executable backend contract: Oracle, ExecBC, and AOT projections continue to
reject `TYPE_TEST` transactionally until canonical runtime subtype evaluation
is connected.

## Conservative boundary

Multiple catches, `finally`, unresolved annotations, and structurally richer
annotations remain on the persistent legacy CFG path. Unsupported handler
bodies and protected-call shapes also retain their existing fallback. The
preflight checks resolvability before canonical conversion, so an unsupported
annotation cannot leave a speculative diagnostic or partial dispatch graph.

## Focused coverage

`tests/parser/test_pre_semantic_ir_typed_catch.inc` verifies:

- one payload definition, one canonical type test, and one unmatched throw;
- distinct payload, boolean-result, and match TypeIds;
- payload dominance and exact ValueId reuse by both the test and rethrow;
- exception-to-dispatch, true-to-handler, and false-to-propagation edges;
- a typed catch-local initialization on the matching path;
- exception flags only on the dispatch block;
- direct preservation of `matchTypeId` as ExecIR `matchTypeToken`;
- an independent exceptional successor on a trailing invoke, proving the
  local unmatched rethrow does not poison function-level reachability; and
- dispatch for both a primitive annotation and an already-resolved user type.

`tests/parser/test_pre_semantic_ir_exception_fallback.inc` keeps an unsupported
typed handler, an unresolved type annotation, and an array annotation on the
legacy path.

## Validation

- TDD began with the producer at 93/94: the typed fixture contained no
  `EXCEPTION_PAYLOAD` because all typed catches still fell back.
- The implemented dispatch passes all 95 producer tests with MSVC on Windows
  and with GCC and Clang under WSL.
- The WSL GCC ASan+UBSan build also passes all 95 tests with leak detection and
  halt-on-error enabled.
