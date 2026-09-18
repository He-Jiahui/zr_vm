# SSA 01.02: one terminator at the end of each nonempty block

## RED and semantic fixtures

A one-instruction block ending with a constant was previously published as
though that constant were its terminator. The MSVC standalone target failed
at `FAIL: builder published a nonterminal tail as a block terminator`.
Another fixture places a `RETURN` before a second `RETURN` in the same
block. The builder must reject the first return as a premature terminator.

The nonterminal tail now reports `MISSING_TERMINATOR` at block 1,
instruction 1; the early return reports `INVALID_RANGE` at block 1,
instruction 2. Both failed builds leave caller output unchanged. Existing
empty-block CFG fixtures and the structurally verified constant/branch/
return and constant/call/return fixtures remain accepted.

## Observed validation

- MSVC (VSDevCmd, D:-backed focused build): the expanded core/effects/
  dominator/builder/value/oracle/pass-manager CTest selection passed 8/8.
- WSL GCC 11.4 directly compiled the focused real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed binary printed
  `ssa builder CFG PASS` (exit 0), without a sanitizer report.
- WSL Clang 14 directly compiled the same focused sources without sanitizer;
  its D:-backed binary printed `ssa builder CFG PASS` (exit 0). No full
  Clang CTest, Clang sanitizer, or cross-backend execution result is claimed.

## Remaining gates

This checks terminator placement and diagnostic location, not branch target
arity, optional chaining, exceptional-result availability, pruned phi
construction, or four-backend language behavior. The user-modified
`test_ssa_construction.c` was left untouched.
