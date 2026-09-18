# SSA 01.02: source-located opcode arity preflight

## Failure fixtures

Two nonterminal instructions deliberately omit a canonical fact: a
`CONSTANT` has no result value, and a `LOAD` has no fixed operand. Each
has a following semantic `RETURN` to isolate the missing fact from the
block-terminator check. Before this slice the MSVC fixture failed with
`builder lost the source block for a missing canonical result`; the
generic downstream validator only identified instruction 1 without its
source block. Both cases now report `INVALID_RANGE` at function token 42,
block 1, semantic instruction 1, with expected/actual count [1,0]. A
failed build does not replace the caller's output.

The preflight runs after the logical operand-pool bounds check, so a
missing operand storage range still produces its existing more specific
diagnostic. Existing valid constant/call/return and switch fixtures remain
covered by the builder CFG target. The builder does not invent operands or
results when the canonical semantic producer omitted them.

## Observed validation

- MSVC (VSDevCmd, D:-backed build): adjacent SSA CTest selection passed 9/9.
- WSL GCC 11.4 directly compiled the real builder/core sources with
  `-fsanitize=address,undefined`; the D:-backed builder CFG fixture printed
  `ssa builder CFG PASS` (exit 0), without a sanitizer report.
- WSL Clang 14 directly compiled the same sources without sanitizers; its
  D:-backed fixture printed `ssa builder CFG PASS` (exit 0). Full GCC/Clang
  CTest and Clang sanitizer coverage were not run for this slice.

## Remaining gates

The compiler-side canonical producer still has incomplete per-op facts;
this slice diagnoses them rather than claiming Place lowering, pruned phi,
SSA rename or full 01.02 acceptance. The independently dirty
`test_ssa_construction.c` was not modified.
