# SSA source cleanup CFG acceptance

This slice connects the source compiler to the cleanup-region representation
accepted by the SemanticIR-to-ExecIR builder.

## Accepted source shape

- The statement is `try/finally` with no catch clauses.
- The protected and `finally` bodies are completely preflighted before a graph
  is activated.
- The `finally` body contains only nested blocks and linear expression
  statements. The protected body may additionally end in one linear `return`
  or `throw`.
- No enclosing ownership cleanup or active catch target is present.
- The protected block ends in an operand-free `BRANCH` over one cleanup edge to
  a `ZR_PARSER_CFG_BLOCK_CLEANUP`.
- The cleanup block ends in an operand-free `BRANCH` over one cleanup edge to a
  join, and following source statements continue from that join.
- For a terminal return or throw, the abrupt operand is captured before
  cleanup. Cleanup instead targets a dedicated RETURN or THROW block, and that
  block consumes the pre-cleanup ValueId without reloading a local changed by
  `finally`.
- ExecIR construction preserves both adjacencies and marks the cleanup block
  with `ZR_EXEC_IR_BLOCK_FLAG_CLEANUP`.

`tests/parser/test_ssa_source_cleanup_cfg.c` covers this source-to-ExecIR path.

## Fail-closed boundary

The same test keeps nonlinear return/throw payloads and a combined
catch-plus-finally statement on the legacy path. Calls, declarations, nested
nonlinear control flow, ownership cleanup, multiple completion kinds, and
exceptional cleanup entry are also outside this slice. Those shapes must not
publish a partial cleanup graph or restart a detached CFG after rejection.

The builder now accepts a separately tested `CLEANUP_DISPATCH` only when a
cleanup block carries one explicit SSA selector and an ordered switch
case/default edge list. The single terminal-return source shape has only one
possible post-cleanup destination, so it uses a direct cleanup edge and retains
only the captured return payload. A later source milestone must create a
selector before accepting exceptional entry, throw propagation, mixed normal
and abrupt completion, or break/continue.

## Validation evidence (2026-09-19)

- The focused source cleanup suite passes 6/6 on Windows MSVC and WSL GCC and
  Clang; the 100-test pre-SemanticIR producer suite passes on all three
  toolchains.
- The adjacent 14-test SSA matrix passes 14/14 on all three toolchains.
- GCC ASan+UBSan passes the focused suite five consecutive times and the
  producer suite once (100/100), with leak detection and halt-on-error enabled.
- Wiki validation passes for 116 Markdown files, 115 manifest pages, and 644
  local links; the validator unit suite passes 5/5.
