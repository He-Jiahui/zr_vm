# SSA source cleanup CFG acceptance

This slice connects the source compiler to the cleanup-region representation
accepted by the SemanticIR-to-ExecIR builder.

## Accepted source shape

- The statement is `try/finally` with no catch clauses.
- The protected and `finally` bodies are completely preflighted before a graph
  is activated.
- The `finally` body contains only nested blocks and linear expression
  statements. The protected body may additionally contain one or more
  same-kind linear `return` or `throw` sites under statement-form conditionals,
  or one or more resolved direct calls in linear statement-form conditional
  control. Each call may take no arguments or up to two exact,
  ownership/reference/GC-neutral `int` identifiers or integer literals by value.
  When directly nested in a supported `while`,
  linear statement-form `for`, or statically typed `foreach`, it may instead
  contain one or more operand-free `break` sites or one or more operand-free
  `continue` sites, provided every transfer has the same loop target and
  transfer kind.
- No enclosing ownership cleanup or active catch target is present.
- The protected block ends in an operand-free `BRANCH` over one cleanup edge to
  a `ZR_PARSER_CFG_BLOCK_CLEANUP`.
- The cleanup block ends in an operand-free `BRANCH` over one cleanup edge to a
  join, and following source statements continue from that join.
- For a terminal return or throw, the abrupt operand is captured before
  cleanup. Cleanup instead targets a dedicated RETURN or THROW block, and that
  block consumes the pre-cleanup ValueId without reloading a local changed by
  `finally`. Multiple same-kind return or throw sites reuse the same payload
  merge and completion block, whether or not a normal sibling remains.
- When the same protected body can fall through, compiler-private selector and
  payload Places are created before its branch. The abrupt path stores its
  payload and `true`; the normal path retains `false`; both enter cleanup.
  Cleanup loads the selector and ends in `CLEANUP_DISPATCH`, with an ordered
  `SWITCH_CASE` to the abrupt block and `SWITCH_DEFAULT` to the normal join.
  The abrupt block reloads the preserved payload Place after cleanup.
- For the protected-call shape, each `INVOKE` exception edge reaches the same
  dedicated landing block. That block defines one `EXCEPTION_PAYLOAD`, stores
  it in the private payload Place, selects `true`, and enters cleanup. Normal
  edges retain the pre-call `false` selector and enter the same cleanup.
  Dispatch then rethrows the reloaded payload or reaches the normal join
  without using an interrupted call result. A conditional call branch and a
  linear non-call sibling both join this same cleanup path. One or more explicit
  `throw` sites may share that path when every explicit payload is an exact
  non-null, ownership-neutral `object`; the explicit payloads and exceptional
  payload then use the same pending `THROW` completion.
- For the loop-transfer shape, the pending destination is the existing loop
  join for `break`, the `while` condition block for a `while` `continue`, the
  `for` step block for a `for` `continue`, or the foreach move-next block for a
  `foreach` `continue`. One or more transfers of that same kind go from
  cleanup through one completion block and then follow a normal edge to that
  target. A conditional transfer uses a selector-only private Place and
  cleanup dispatch; it does not allocate a meaningless payload Place.
- ExecIR construction preserves both adjacencies and marks the cleanup block
  with `ZR_EXEC_IR_BLOCK_FLAG_CLEANUP`.

`tests/parser/test_ssa_source_cleanup_cfg.c` covers this source-to-ExecIR path;
its focused exceptional-call cases live in
`tests/parser/test_ssa_source_cleanup_cfg_exceptional.inc` so the shared test
harness and individual case family remain bounded. Focused loop-completion
cases live in `tests/parser/test_ssa_source_cleanup_cfg_loop.inc`.

## Fail-closed boundary

The same test keeps nonlinear return/throw payloads, mixed return/throw sites,
and a combined catch-plus-finally statement on the legacy path. Converting,
non-value, multiple, or conditional arguments, dynamic or unresolved calls,
declarations, nested nonlinear control flow, ownership cleanup, and mixed
explicit/exceptional completion other than all-exact `object` throws are also
outside this slice. Integer or otherwise incompatible explicit throws and
other mixed completion kinds must not publish a partial cleanup graph or
restart a detached CFG after rejection.
Mixed `break`/`continue` sites,
dynamic/unresolved `foreach` iteration, binding cleanup, unsupported loop
cleanup, and a loop transfer combined with another completion kind remain on
that same fail-closed path.

The source producer now uses the builder's `CLEANUP_DISPATCH` contract for one
normal-versus-return, normal-versus-throw, or normal-versus-call-exception
decision. A terminal abrupt shape still has only one post-cleanup destination
and therefore retains its direct cleanup edge. Same-kind return/throw sites use
the same payload merge; when a normal sibling remains, they additionally use
the pending selector. One or more same-kind,
operand-free `break`/`continue` transfers from a supported
`while`/`for`/`foreach` are dispatched to their existing loop target after
cleanup; mixed transfer kinds and combined explicit/exceptional completion
outside the all-exact object-throw case still require later source milestones.

## Validation evidence (2026-09-20)

- The focused source cleanup suite passes 43/43 on Windows MSVC and WSL GCC and
  Clang. It includes repeated and conditional protected invokes sharing one
  exception landing, linear sibling cleanup joins, dynamic-call fail-closed
  guards, terminal and conditional
  `break`/`continue`, repeated same-kind transfers across
  `while`/`for`/`foreach`, and explicit mixed-kind fail-closed cases.
- The adjacent 14-test SSA matrix passes 14/14 on all three toolchains.
- GCC ASan+UBSan passes the focused suite 43/43 five consecutive times, with
  leak detection and halt-on-error enabled.
- Wiki validation passes for 116 Markdown files, 115 manifest pages, and 644
  local links; the validator unit suite passes 5/5.
