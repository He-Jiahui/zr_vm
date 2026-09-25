# SSA CFG topological effect propagation

## Scope and RED (2026-09-26)

The preceding natural-loop stage handled declaration-ordered CFGs. A new
four-block diamond lists the right arm (block 4) after its merge (block 3).
Before this stage, GCC `ssa_cfg_effects_builder` exited 1 at
`FAIL: reverse-declared join gets phis from both complete arms`. Its merge
had not waited for the second arm's terminal memory/effect versions.

The producer now topologically processes the predecessor graph with actual
backedges removed, storing instruction token ranges independently of block
declaration order. A reverse-declared acyclic join receives exact per-edge
token phis; a reverse-declared non-backedge within a reducible loop still
carries a loop-head phi. A two-entry irreducible cycle leaves all effect facts
unpublished. The preceding phi-pool capacity-overflow fixture retains the
failure atomicity check. This stage does not claim irreducible CFG synthesis,
exception-point block splitting, or a full source-compiler loop oracle.

## Verification (2026-09-26)

- WSL GCC 11.4: rebuilt focused CFG effects, core effect verifier, linear
  effects and builder CFG targets; CTest regex
  `^ssa_(cfg_effects_builder|effects_verifier|linear_effects_builder|builder_cfg)$`
  passed 4/4.
- WSL GCC ASan: rebuilt CFG effects target; `^ssa_cfg_effects_builder$`
  passed 1/1.
- WSL Clang: strict `-Wall -Wextra -Werror -fsyntax-only` on modified C files
  and fixture passed.
- Windows MSVC 19.44 through VSDevCmd: rebuilt focused CFG effects target;
  `^ssa_cfg_effects_builder$` passed 1/1.

This is an intermediate 01.02/01.03 stage, not the M1 exit gate.
