# SSA source cleanup CFG acceptance

This slice connects the source compiler to the cleanup-region representation
accepted by the SemanticIR-to-ExecIR builder.

## Accepted source shape

- The statement is `try/finally` with no catch clauses.
- The protected and `finally` bodies are completely preflighted before a graph
  is activated.
- The `finally` body contains only nested blocks and linear expression
  statements. The protected body may additionally contain one linear `return`
  or `throw` under statement-form conditionals, or one resolved direct call
  outside conditional control. The call may take no arguments or one exact,
  ownership/reference/GC-neutral `int` identifier by value.
- No enclosing ownership cleanup or active catch target is present.
- The protected block ends in an operand-free `BRANCH` over one cleanup edge to
  a `ZR_PARSER_CFG_BLOCK_CLEANUP`.
- The cleanup block ends in an operand-free `BRANCH` over one cleanup edge to a
  join, and following source statements continue from that join.
- For a terminal return or throw, the abrupt operand is captured before
  cleanup. Cleanup instead targets a dedicated RETURN or THROW block, and that
  block consumes the pre-cleanup ValueId without reloading a local changed by
  `finally`.
- When the same protected body can fall through, compiler-private selector and
  payload Places are created before its branch. The abrupt path stores its
  payload and `true`; the normal path retains `false`; both enter cleanup.
  Cleanup loads the selector and ends in `CLEANUP_DISPATCH`, with an ordered
  `SWITCH_CASE` to the abrupt block and `SWITCH_DEFAULT` to the normal join.
  The abrupt block reloads the preserved payload Place after cleanup.
- For the protected-call shape, the `INVOKE` exception edge reaches a dedicated
  landing block that defines `EXCEPTION_PAYLOAD`, stores it in the private
  payload Place, selects `true`, and enters cleanup. The normal edge retains
  the pre-call `false` selector and enters the same cleanup. Dispatch then
  rethrows the reloaded payload or reaches the normal join without using the
  interrupted call result.
- ExecIR construction preserves both adjacencies and marks the cleanup block
  with `ZR_EXEC_IR_BLOCK_FLAG_CLEANUP`.

`tests/parser/test_ssa_source_cleanup_cfg.c` covers this source-to-ExecIR path.

## Fail-closed boundary

The same test keeps nonlinear return/throw payloads, two abrupt sites, two
protected calls, and a combined catch-plus-finally statement on the legacy
path. Literal, converting, non-value, multiple, or conditional arguments,
conditional calls, declarations, nested nonlinear control flow, ownership
cleanup, and mixed explicit/exceptional completion are also outside this
slice. Those shapes must not publish a partial cleanup graph or restart a
detached CFG after rejection.

The source producer now uses the builder's `CLEANUP_DISPATCH` contract for one
normal-versus-return, normal-versus-throw, or normal-versus-call-exception
decision. A terminal abrupt shape still has only one post-cleanup destination
and therefore retains its direct cleanup edge. Multiple abrupt sites or kinds,
combined explicit/exceptional completion, and break/continue still require
later source milestones.

## Validation evidence (2026-09-19)

- The focused source cleanup suite passes 13/13 on Windows MSVC and WSL GCC and
  Clang; the 100-test pre-SemanticIR producer suite passes on all three
  toolchains.
- The adjacent 14-test SSA matrix passes 14/14 on all three toolchains.
- GCC ASan+UBSan passes the focused suite five consecutive times and the
  producer suite once (100/100), with leak detection and halt-on-error enabled.
- Wiki validation passes for 116 Markdown files, 115 manifest pages, and 644
  local links; the validator unit suite passes 5/5.
