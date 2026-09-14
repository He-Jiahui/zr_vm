---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_loops.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_loops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_loops.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_profile.c
tests:
  - tests/parser/test_ssa_loops_specialization.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/05-loops-specialization.md
doc_type: module-detail
status: draft
---

# Loop, profile, and specialization contracts

Loop analysis is parser-owned and stores only stable block/value/instruction
identities. Natural loops are formed from CFG reachability and dominators;
irreducible or multiple-entry loops are recorded with an explicit blocked
reason. Trip-count facts are advisory and never change baseline semantics.

The LICM pass moves only an existing pure, memory-free, non-trapping invariant
instruction into an existing preheader. It does not invent a preheader, move a
zero-trip operation, or hoist an operation whose exception/effect timing is not
proven. Strength reduction is currently limited to identity arithmetic (`* 1`,
`+ 0`, and the equivalent safe forms), preserving the original SSA result and
re-validating structure/SSA after a mutation.

Profiles use an address-free key consisting of module identity/hash, IR hash,
signature hash, layout hash, and compiler ABI. Import validates schema, key,
site uniqueness, and scalar counters. A stale or incomplete profile is ignored
with a reason; it never invalidates baseline code. Site observations are bounded
by overflow checks.

Specialization policy has independent site, function, and module version/code
budgets. It requires a minimum sample count and hit/miss thresholds, rejects
megamorphic sites, and retains a baseline implementation for every decision.
Repeated deoptimization enters cooldown; `SpecializationTick` clears cooldown
only after the requested number of samples and does not re-enable megamorphic
sites. Thresholds are centralized in `SZrExecIrSpecializationPolicy` and default
to conservative finite limits.

The focused test fixture covers loop discovery/trip counts, non-trapping LICM,
zero-trip and throwing-loop rejection, identity strength reduction, profile key
mismatch, and specialization version/cooldown budgets. Full CTest registration
is intentionally handled by the shared SSA test integration rather than this
module document.
