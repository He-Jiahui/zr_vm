# SSA 01.02: terminator successor arity at the builder boundary

## RED and boundary fixtures

A semantic `BRANCH` with two CFG successors was previously lowered into an
ExecIR `BRANCH` carrying both targets; the MSVC standalone test failed with
`FAIL: builder lowered a two-target branch as a one-target opcode`.

The independent builder fixture also requires a `RETURN` with one successor
and a selector-bearing `SWITCH` with no successor to fail. Diagnostics use
`INVALID_RANGE`, identify the semantic block and terminator instruction,
and carry expected/actual counts [1,2], [0,1], and [1,0], respectively.
Failures do not replace caller output. Existing one-target branch and
zero-target return fixtures remain accepted; a new selector-bearing switch
with one target preserves its edge and passes core structural verification.

## Observed validation

- MSVC (VSDevCmd, D:-backed focused build): the expanded core/effects/
  dominator/builder/value/oracle/pass-manager CTest selection passed 8/8.
- WSL GCC 11.4 directly compiled the focused real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed binary printed
  `ssa builder CFG PASS` (exit 0), with no sanitizer report.
- WSL Clang 14 directly compiled the same sources without sanitizers; the
  D:-backed binary printed `ssa builder CFG PASS` (exit 0). No Clang
  sanitizer or full-backend parity result is claimed for this slice.

## Remaining gates

This check applies only to the fixed normal-edge meaning of branch, switch,
and return. Conditional branch lowering, exception/cleanup/resume edge
classification, pruned phi construction, and differential backend behavior
remain separate work. No claim is made for `THROW`/`SUSPEND` successor arity.
