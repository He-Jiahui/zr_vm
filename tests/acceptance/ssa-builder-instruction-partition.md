# SSA 01.02: semantic instruction ownership by CFG block

## RED and boundary fixtures

Two reachable semantic blocks both claim the same single `BRANCH`
instruction. Before the repair, the MSVC standalone fixture printed
`FAIL: builder let two blocks claim one semantic instruction`; the builder
could emit the source instruction twice. A separate fixture has one semantic
instruction that no block claims; the builder must not discard it silently.

Both cases now report `INVALID_RANGE`. Overlap identifies the second block,
duplicated semantic instruction, and expected/actual ownership counts [1,2].
A gap identifies the first unowned instruction and covered/total counts [0,1].
On failure, caller-owned ExecIR output is unchanged. A positive two-block
fixture deliberately stores the return slice before the entry block's
constant/branch slice; emitted instructions follow block order, source IDs
remain mapped, and the result passes core structural verification.

## Observed validation

- MSVC (VSDevCmd, D:-backed focused build): the expanded core/effects/
  dominator/builder/value/oracle/pass-manager CTest selection passed 8/8.
- WSL GCC 11.4 directly compiled the focused builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed fixture printed
  `ssa builder CFG PASS` (exit 0), without a sanitizer report.
- WSL Clang 14 directly compiled the same sources without sanitizers; the
  D:-backed fixture printed `ssa builder CFG PASS` (exit 0). Clang sanitizer,
  full CTest, and cross-backend execution parity are not claimed.

## Remaining gates

This covers source instruction ownership, not a general SSA renamer, pruned
phi placement, exception-edge definition availability or source-language
backend parity. The user-modified `test_ssa_construction.c` is untouched.
