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
  control. Each call may take no arguments or up to three exact,
  ownership/reference/GC-neutral integer, `bool`, or `float` identifiers or
  literals by value; integer arguments must match the resolved parameter type
  exactly. A direct call may also be the right side of a plain `=` assignment
  to a previously initialized local.
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
- When a protected call is assigned to a local, its normal continuation stores
  the result before branching to cleanup. The exceptional landing block is
  emitted only after that normal block is closed; its cleanup entry still sees
  the pre-invoke definition. A `finally` read merges those two definitions in
  one value phi and never reads the incomplete invoke result.
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
`tests/parser/test_ssa_source_cleanup_cfg_interrupted_assignment.inc` checks
the two cleanup predecessors and the exact pre-invoke/normal-result phi. It
also checks that compound assignment retains complete legacy fallback.
The terminal `break` through `finally` and protected `INVOKE` rethrow cases
also assert that the resulting ExecIR passes the effect verifier. They cover
the interaction between private cleanup Place promotion and surviving heap
memory operations on loop and exceptional edges.

## Fail-closed boundary

The same test keeps nonlinear return/throw payloads, mixed return/throw sites,
and a combined catch-plus-finally statement on the legacy path. Converting,
non-value, multiple, or conditional arguments, dynamic or unresolved calls,
declarations, compound call assignment, nested nonlinear control flow,
ownership cleanup, and mixed
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

- The focused source cleanup suite passes 50/50 on Windows MSVC and WSL GCC and
  Clang. It includes repeated and conditional protected invokes sharing one
  exception landing, linear sibling cleanup joins, dynamic-call fail-closed
  guards, terminal and conditional
  `break`/`continue`, repeated same-kind transfers across
  `while`/`for`/`foreach`, and explicit mixed-kind fail-closed cases.
- The adjacent 14-test SSA matrix passes 14/14 on all three toolchains.
- GCC ASan+UBSan passes the focused suite 50/50 five consecutive times, with
  leak detection and halt-on-error enabled.
- Wiki validation passes for 116 Markdown files, 115 manifest pages, and 644
  local links; the validator unit suite passes 5/5.

## Effect synthesis ordering regression (2026-09-26)

Before the fix, the two new source-level effect assertions failed: a promoted
cleanup STORE had become NOP after tokens were generated, leaving a surviving
heap operation or join phi dependent on its obsolete memory version. The
builder now promotes Places before synthesizing CFG effects and verifies the
resulting effect graph before publishing the candidate.

- WSL GCC: rebuilt `ssa_source_cleanup_cfg`, `ssa_cfg_effects_builder`,
  `ssa_effects_verifier`, and `ssa_builder_cfg`; all four passed. The complete
  `ctest --test-dir build/ssa-gcc-debug -L ssa` run passed 80/80 (other targets
  were not rebuilt as part of this slice).
- WSL Clang: rebuilt the same four targets and passed 4/4.
- Windows MSVC: rebuilt the same four targets and passed 4/4.

## Interrupted assignment (2026-09-27)

`test_invoke_assignment_finally_reads_preinvoke_value` started RED because
the protected `result = identity()` failed finally preflight (1/51). After
reusing the catch assignment shape, its first builder run rejected an unowned
normal-path `STORE` (`INVALID_RANGE`, source instruction 21, 39 instructions
versus 38 claimed). Closing the normal block before emitting the exceptional
landing block keeps every source instruction owned and prevents the unfinished
invoke result from reaching the cleanup entry. The cleanup read now consumes
a two-incoming phi: its exceptional edge supplies the pre-invoke
initialization and its normal edge supplies the completed call result.
`test_compound_invoke_assignment_finally_keeps_legacy_cfg` confirms that a
compound assignment still rejects the entire source cleanup graph.

- WSL GCC 11.4 and Clang 14: source cleanup suites pass 52/52 each; pre-
  semantic source suites pass 108/108 each. Cleanup, construction, effect
  verifier, and Place promotion CTest selection passes 4/4 per compiler.
- Windows MSVC 19.44 Debug: shared parser and source cleanup target rebuild;
  the same four CTest targets pass 4/4. The separate pre-semantic source target
  remains unavailable due four existing unexported compiler-internal symbols.
- Wiki validation passes for 116 pages, 115 manifest pages, and 646 local
  links. No runtime exception payload or executable backend subtype semantics
  are claimed by this source-level fixture. The available ASan cache has no
  built source-cleanup target; this slice has no sanitizer execution result.
