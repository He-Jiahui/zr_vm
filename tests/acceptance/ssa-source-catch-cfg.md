# SSA source catch CFG acceptance

## Scope

This phase connects the source compiler to the handler-local exception payload
contract for one closed, auditable shape:

- one untyped catch-all parameter;
- no `finally` block;
- one resolved direct function call with no arguments or one unmarked
  positional `int` identifier that exactly matches one value parameter without
  conversion, ownership, reference, or GC-bridge work in the protected block;
  and
- either an empty catch body or one expression that reads the catch binding.

For the one-argument shape, the source local is loaded in the predecessor and
that ValueId becomes the call's explicit argument operand. The protected call
then lowers to `INVOKE`. Its normal successor branches to a join,
while its exceptional successor enters a dedicated handler that defines one
zero-operand `EXCEPTION_PAYLOAD`. The handler initializes a source-local Place
for the catch parameter from the payload, optionally loads that Place, and then
branches to the same join. The pre-try semantic slot snapshot is restored on
the handler path and at the join, which keeps the call result unavailable to
the handler and the catch binding local to the exceptional block. The active
handler target is cleared before the catch body is compiled, so a later call
receives a separate propagation sink.

The legacy exception bytecode remains in place. Typed and multiple catches,
catch bodies other than the single binding read, calls with multiple, named,
marked, generic, member, literal, or computed arguments, protected bodies
without the single resolved call, nested control, explicit handled throws, and
every `finally` shape retain the persistent conservative fallback barrier.
Declared callable bodies lower inside disposable SemanticIR
isolation instead of publishing into the entry sidecar. These boundaries do not
publish a partial source exception graph.

## Focused coverage

`tests/parser/test_pre_semantic_ir_exception_fallback.inc` verifies:

- direct source call-to-handler exceptional routing;
- a unique handler-local payload result with no operands;
- payload initialization of a source-local catch Place and a handler-local
  read from the same Place;
- one simple local argument loaded before the invoke block, retained as the
  call's second operand, and absent from every handler instruction's explicit
  value-operand array;
- normal and handler convergence through an ordinary branch join;
- successful SemanticIR-to-ExecIR construction for both empty and binding-read
  handlers, plus the exceptional block flag;
- clearing of the handler target before a trailing call;
- declared-child isolation;
- disposable catch-body isolation after a protected call's late semantic
  fallback; and
- fallback coverage for literal, multiple, nested/computed, and type-converting
  arguments, plus general nonempty bodies, typed, multiple, no-call, nested,
  and `finally` boundaries.

## Validation

- TDD started with the new source fixture failing `1/78`: the protected call
  had no exceptional successor targeting a source handler. Independent review
  then drove a `2/79` RED for declared-child pollution and nested argument
  calls escaping the bounded shape. Catch binding work started with a `1/80`
  RED because a nonempty handler fell back instead of consuming its payload;
  independent review then drove a `1/81` RED because late semantic-call
  fallback leaked the catch binding read into the persistent graph. Simple
  argument capture started with a `1/82` RED because every argument-bearing
  protected call still preflighted to the legacy graph. Independent review then
  reproduced a second `1/82` RED because an `int` argument converted to a
  `float` parameter was incorrectly admitted to the precise graph. The
  completed lowering passes all 82
  producer cases on Windows MSVC 19.44, WSL GCC 11.4, and WSL Clang 14.
- The same 11-test SSA adjacency/projection selection passes on all three
  toolchains: core/effects verification, builder CFG/dominance/control edges,
  iterator invokes, place eligibility/promotion, value validation, Oracle
  projections, and the scalar pass manager.
- WSL GCC 11.4 ASan+UBSan with leak detection enabled passes the producer
  82/82 without a sanitizer diagnostic.
- `python scripts/validate_wiki.py --root .` passes for 116 Markdown files,
  115 manifest pages, and 644 local links; the validator unit suite passes
  5/5.
