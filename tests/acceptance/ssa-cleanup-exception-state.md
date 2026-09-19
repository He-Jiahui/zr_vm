# SSA cleanup exception-state acceptance

This checkpoint closes the SSA plan's interrupted-assignment boundary before a
source `try/finally` producer is allowed to publish exceptional cleanup entry.

## Contract

The focused graph defines a pending-state selector before a value-producing
`INVOKE`. Its ordered normal and exception successors each enter the same
cleanup block over an explicit cleanup edge. The cleanup block uses that
pre-invoke selector in a one-operand `SWITCH` and dispatches through ordered
case/default successors. Builder lowering must preserve the exception block,
cleanup block, selector, and exact adjacency, and the resulting ExecIR must pass
structural plus SSA verification.

The paired negative substitutes the `INVOKE` result for the pending selector.
That result is committed only on the normal successor. Because the exceptional
successor can also reach the cleanup block, SSA verification must report
`ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE` at the cleanup `SWITCH`; block dominance
alone is insufficient. This prevents a call or assignment interrupted by a
throw from exposing an uncommitted result to `finally`.

`tests/parser/test_ssa_cleanup_exception_state.c` owns both fixtures. It is a
representation/verifier gate, not source production: the compiler still does
not emit exceptional `finally` entry, path-specific pending selectors,
exception payload storage, or abrupt cleanup propagation.

## Validation

- MSVC 19.44.35228, WSL GCC 11.4.0, and WSL Clang 14.0.0 each rebuilt and
  passed the focused target.
- The adjacent SSA selection, now including cleanup dispatch and exception
  state, passed 14/14 on all three toolchains.
- The WSL-native GCC ASan+UBSan build at
  `/home/hejiahui/codex-validation/zr-vm-ssa-cleanup-dispatch-gcc-asan-phase76`
  passed the focused target five consecutive times with leak detection and
  halt-on-error enabled.
- Wiki validation passed for 116 Markdown files, 115 manifest pages, and 644
  local links; its validator unit suite passed 5/5.

No verifier code change was required: the existing conservative exceptional
reachability closure already enforced the planned invariant. This checkpoint
makes that cleanup-specific interaction independently executable and prevents a
future source producer from weakening it accidentally.
