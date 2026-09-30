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
instruction and phi effect facts unpublished. At that stage, token-pool
allocation failure injection and a full compiler/source-language loop oracle
were outstanding; token-pool OOM evidence is recorded below.

## Allocation-failure atomicity (2026-09-30)

The focused `ssa_cfg_effects_faults` test redirects `realloc` only in the real
core `exec_ir.c` translation unit and sweeps both pool append allocations:
the phi-incoming pool append and the memory-token pool append. Each injected
OOM must return the ExecIR OOM diagnostic while instruction effects, block
phi records, and memory-token pool pointer/count/capacity remain unchanged.
When the phi append succeeded before memory-token allocation failed, the
producer restores the prior logical `phiIncomingCount`; allocated backing
capacity may remain available for retry.

After each injected failure, the test disables injection and retries synthesis
on the same natural-loop function. It checks the exact forward and latch
memory/effect phi inputs and requires `ZrCore_ExecIr_VerifyEffects` to pass.
This exercises both core pool realloc failure points and verifies that
neither attempt leaves published partial effect facts.

This direct typed ExecIR fixture does not run the full compiler or a
source-language loop oracle. That oracle remains outstanding.

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

## Pool OOM execution evidence (2026-09-30)

- Native Windows MSVC: the root's `current-gap-build` Ninja build completed
  36/36 steps, including `zr_vm_ssa_cfg_effects_faults_test`. The build log is
  `D:/tmp/zr_vm/ssa-control/current-gap-build.log`; CMake cache is
  `D:/tmp/zr_vm/ssa-artifact-v6-msvc`.
- Standalone CTest `ssa_cfg_effects_faults` passed 1/1 in 40.96 seconds
  (exit 0). The wider `current-gap-ctest` group had 1 pass and 4 failures;
  this record covers only the focused fault test.

This is a stage, not the 01.02 or M1 gate. Irreducible and non-declaration-order
CFGs are still left untouched, and exception-edge state must be split at
throw sites before effect synthesis can claim complete coverage.
