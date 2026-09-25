# SSA CFG unsupported effect diagnostics

## Scope and RED (2026-09-26)

The direct CFG producer previously reported success without writing memory
or effect facts for a store in a loop without a forward entry, and for a
two-entry irreducible cycle. The focused test first failed at
`FAIL: loop without forward entry is diagnosed at its first instruction`.
An additional pure two-entry cycle initially failed when all unsupported
graph shapes were rejected unconditionally.

Now an effectful unsupported graph reports `UNSUPPORTED` with its unresolved
block and first instruction, preserves all original token/phi fields, and
allows `ZrParser_ExecIr_Build` to keep its existing transactional failure
path. A purely control-flow graph without memory reads or observable effects
still succeeds without requiring an effect-token fixed point. Builder-level
source fixtures, exception edges and full compiler oracle coverage remain
outside this direct-producer stage.

## Verification (2026-09-26)

- WSL GCC 11.4: rebuilt CFG effects and builder CFG targets; CTest regex
  `^ssa_(cfg_effects_builder|builder_cfg)$` passed 2/2.
- WSL GCC ASan: rebuilt CFG effects target; CTest
  `^ssa_cfg_effects_builder$` passed 1/1.
- WSL Clang: `-std=c11 -Wall -Wextra -Werror -fsyntax-only` passed on
  modified producer modules and CFG fixture.
- Windows MSVC 19.44: rebuilt CFG effects target through VSDevCmd;
  CTest `^ssa_cfg_effects_builder$` passed 1/1.

This is an intermediate 01.03 stage, not an M1 exit gate.
