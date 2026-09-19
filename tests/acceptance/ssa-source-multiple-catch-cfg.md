# SSA source multiple-catch CFG acceptance

## Scope

This phase expands the bounded source `try`/`catch` producer from one handler to
an ordered handler list. The protected region, catch annotations, catch bodies,
argument flow, abrupt-handler, and no-`finally` restrictions remain the same as
the single-catch checkpoint. Every nonterminal clause must have one simple,
already-resolvable canonical type annotation. The final clause may instead be
an untyped catch-all.

The compiler preflights the whole list before creating a partial handler graph,
then records one plan entry per source clause. Dispatch lowering:

1. routes the protected `INVOKE` exceptional edge to the first dispatch block;
2. defines one `EXCEPTION_PAYLOAD` for the complete handler list;
3. emits one `TYPE_TEST(payloadValueId, matchTypeId)` per typed clause in source
   order;
4. branches each true edge to that clause's handler and each false edge to the
   next dispatch;
5. routes the final false edge directly to a terminal catch-all when present;
   otherwise it emits one zero-successor `THROW` of the original payload; and
6. joins the normal continuation with every falling-through handler while
   keeping direct handler return/rethrow exits local to their selected clause.

Only the first dispatch block is exception-flagged. Later dispatch blocks,
matching handlers, a catch-all handler, and the unmatched propagation sink are
ordinary successors. Typed bindings use their resolved canonical TypeId; a
catch-all binding uses the payload TypeId. No source type spelling participates
in matching.

SemanticIR and ExecIR can represent and verify this graph. Oracle, ExecBC, and
AOT projections still reject `TYPE_TEST` transactionally, so runtime subtype
execution and repeated exception stress remain gated on the later backend
subtype phase.

## Conservative boundary

A catch-all followed by another clause remains on the persistent legacy path;
the later clauses are unreachable by source-order selection and are not
silently dropped from the semantic sidecar. Any unresolved or structurally rich
annotation, unsupported handler body, unsupported protected call, or `finally`
also causes whole-scope fallback before dispatch publication.

## Reference alignment

- CPython's `PyErr_GivenExceptionMatches` uses exception-class subtype identity
  rather than source name strings.
- HotSpot's `Method::fast_exception_handler_bci_for` scans exception-table
  entries sequentially, returns the first subtype match, and treats a zero
  catch token as catch-all.
- Mono scans clauses in table order and uses `mono_object_isinst_checked` for
  typed catch selection.

The local producer therefore preserves source order, canonical identity, and
first-match selection without introducing a spelling-based shortcut.

## Focused coverage

`tests/parser/test_pre_semantic_ir_multi_catch.inc` verifies:

- two typed clauses produce one payload, two ordered type tests, and one final
  rethrow that all reuse the same payload ValueId;
- the exact exceptional, true, false, handler-join, and unmatched edges;
- source-order match tokens and exception flags survive ExecIR construction;
- an arbitrary three-clause chain with a terminal catch-all has no unmatched
  throw and uses the final false edge as the catch-all entry;
- a direct rethrow in the first handler remains a local abrupt sink while a
  later handler and trailing invoke remain valid; and
- a nonterminal catch-all falls back transactionally to the legacy graph; and
- late protected-call fallback discards payload and catch-local reads from
  every handler in the list.

## Validation

- TDD began with the prior producer suite passing 95 tests and the first new
  multiple-catch fixture failing because no exception payload was emitted.
- Review then added a late protected-call fallback regression. The first run
  failed only that case at 99/100 because the second catch leaked one SemanticIR
  `LOAD`; retaining disposable isolation across the remaining handler list
  closed the transaction boundary.
- The final producer suite passes 100/100 with Windows MSVC 19.44, WSL GCC
  11.4, and WSL Clang 14.
- The 11-test SSA adjacency/projection selection passes on all three
  toolchains: core/effects verification, builder CFG/dominance/control edges,
  iterator invokes, Place eligibility/promotion, ValueId validation, Oracle
  projections, and the scalar pass manager.
- A fresh WSL GCC ASan+UBSan build passes 100/100 with leak detection and
  halt-on-error enabled.
- Wiki validation passes for 116 Markdown files, 115 manifest pages, and 644
  local links; the validator unit suite passes 5/5.
