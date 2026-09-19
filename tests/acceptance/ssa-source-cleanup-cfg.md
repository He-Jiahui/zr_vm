# SSA source cleanup CFG acceptance

This slice connects the source compiler to the cleanup-region representation
accepted by the SemanticIR-to-ExecIR builder.

## Accepted source shape

- The statement is `try/finally` with no catch clauses.
- The protected and `finally` bodies are completely preflighted before a graph
  is activated.
- Each body contains only nested blocks and linear expression statements.
- No enclosing ownership cleanup or active catch target is present.
- The protected block ends in an operand-free `BRANCH` over one cleanup edge to
  a `ZR_PARSER_CFG_BLOCK_CLEANUP`.
- The cleanup block ends in an operand-free `BRANCH` over one cleanup edge to a
  join, and following source statements continue from that join.
- ExecIR construction preserves both adjacencies and marks the cleanup block
  with `ZR_EXEC_IR_BLOCK_FLAG_CLEANUP`.

`tests/parser/test_ssa_source_cleanup_cfg.c` covers this source-to-ExecIR path.

## Fail-closed boundary

The same test keeps `return` inside the protected body and a combined
catch-plus-finally statement on the legacy path. Calls, throws, declarations,
nested nonlinear control flow, ownership cleanup, and pending completion state
are also outside this slice. Those shapes must not publish a partial cleanup
graph or restart a detached CFG after rejection.

The next cleanup milestone must introduce explicit pending completion state
before accepting exceptional entry, return/throw propagation, break/continue,
or `CLEANUP_DISPATCH`.
