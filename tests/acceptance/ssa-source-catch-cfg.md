# SSA source catch CFG acceptance

## Scope

This phase connects the source compiler to the handler-local exception payload
contract for one closed, auditable shape:

- one untyped catch-all parameter;
- no `finally` block;
- one resolved zero-argument direct function call in the protected block; and
- an empty catch body.

The protected call lowers to `INVOKE`. Its normal successor branches to a join,
while its exceptional successor enters a dedicated handler that defines one
zero-operand `EXCEPTION_PAYLOAD` and then branches to the same join. The
pre-try semantic slot snapshot is restored on the handler path and at the join,
which keeps the call result unavailable to the handler and the payload local to
the exceptional block. The active handler target is cleared before source
compilation continues, so a later call receives a separate propagation sink.

The legacy exception bytecode remains in place. Typed and multiple catches,
nonempty catch bodies, calls with arguments, protected bodies without the
single resolved call, nested control, explicit handled throws, and every
`finally` shape retain the persistent conservative fallback barrier. Declared
callable bodies lower inside disposable SemanticIR isolation instead of
publishing into the entry sidecar. These boundaries do not publish a partial
source exception graph.

## Focused coverage

`tests/parser/test_pre_semantic_ir_exception_fallback.inc` verifies:

- direct source call-to-handler exceptional routing;
- a unique handler-local payload result with no operands;
- normal and handler convergence through an ordinary branch join;
- successful SemanticIR-to-ExecIR construction and the exceptional block flag;
- clearing of the handler target before a trailing call;
- declared-child isolation; and
- fallback for argument-bearing calls, nonempty, typed, multiple, no-call,
  nested, and `finally` boundaries.

## Validation

- TDD started with the new source fixture failing `1/78`: the protected call
  had no exceptional successor targeting a source handler. Independent review
  then drove a `2/79` RED for declared-child pollution and nested argument
  calls escaping the bounded shape. The completed lowering passes all 79
  producer cases on Windows MSVC 19.44, WSL GCC 11.4, and WSL Clang 14.
- The same 11-test SSA adjacency/projection selection passes on all three
  toolchains: core/effects verification, builder CFG/dominance/control edges,
  iterator invokes, place eligibility/promotion, value validation, Oracle
  projections, and the scalar pass manager.
- WSL GCC 11.4 ASan+UBSan with leak detection enabled passes the producer
  79/79 without a sanitizer diagnostic.
- `python scripts/validate_wiki.py --root .` passes for 116 Markdown files,
  115 manifest pages, and 644 local links; the validator unit suite passes
  5/5.
