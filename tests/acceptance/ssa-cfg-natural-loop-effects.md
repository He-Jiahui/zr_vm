# SSA CFG natural-loop effect synthesis

## Scope

The declaration-ordered CFG effect producer now classifies backedges by
dominance, walks each latch's predecessor paths to its header, and inserts
per-region memory and observable-effect phis before visiting loop bodies.
Phi inputs are populated from each predecessor's terminal tokens, including
when loops are sequential, nested, or have three latches. An exit-only store
between the header and latch in block order is excluded from the loop's
write set. A self-loop with no forward entry retains the conservative no-op.

## Test-first evidence (2026-09-26)

Before the implementation, GCC `ssa_cfg_effects_builder` exited 1 with
`FAIL: each sequential loop gets independent carried phis`. The final focused
suite checks both headers and exact latch effect tokens, requires core effect
verification of every generated graph, and retains the existing
`phiIncomingCount = UINT32_MAX` failure injection: overflow leaves all
instruction and phi effect facts unpublished. Token-pool allocation failure
injection and a full compiler/source-language loop oracle remain outstanding.

## Execution evidence (2026-09-26)

- WSL GCC 11.4: built `zr_vm_ssa_cfg_effects_builder_test`,
  `zr_vm_ssa_effects_verifier_test`, `zr_vm_ssa_linear_effects_builder_test`,
  and `zr_vm_ssa_builder_cfg_test`; CTest regex
  `^ssa_(cfg_effects_builder|effects_verifier|linear_effects_builder|builder_cfg)$`
  passed 4/4.
- WSL GCC ASan build `build/ssa-gcc-asan-phase80`: rebuilt the CFG effects
  target; CTest `^ssa_cfg_effects_builder$` passed 1/1.
- WSL Clang: `-std=c11 -Wall -Wextra -Werror -fsyntax-only` passed for both
  producer modules, the shared core classifier, and the CFG fixture.
- Windows MSVC 19.44: built the CFG effects target with VSDevCmd;
  CTest `^ssa_cfg_effects_builder$` passed 1/1.

This is a stage, not the 01.02 or M1 gate. Irreducible and non-declaration-order
CFGs are still left untouched, and exception-edge state must be split at
throw sites before effect synthesis can claim complete coverage.
