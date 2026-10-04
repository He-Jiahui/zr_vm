---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
  - tests/parser/ssa_pass_manager_state_maps_cases.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.h
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - .codex/plans/20261004-scalar-checkpoint-ownership-fixture-diagnosis.md
tests:
  - tests/parser/test_ssa_sccp_copy_availability.c
  - tests/cmake/ssa-sccp-copy-availability-tests.cmake
  - tests/acceptance/2026-10-03-ssa-sccp-copy-availability.md
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/ssa_pass_manager_state_maps_cases.inc
doc_type: module-detail
---

# SCCP COPY aliases and value availability

## Contract

COPY defines a separate SSA value. Equality of its payload does not establish
that its source and result can replace one another across ownership transitions.
MOVE consumes its operand, and DROP and DROP_IF_INITIALIZED can end its lifetime.
The reference Oracle records these transitions for UNKNOWN values too: a consumed
operand's payload becomes undefined independently of its declared ownership.

For example, `a = 7; b = COPY a; c = MOVE a; RETURN b` is valid and returns 7.
Changing its final operand to `a` reads an unavailable value. Similarly,
`a = 7; b = COPY a; c = MOVE b; RETURN a` becomes invalid if MOVE consumes `a`.

## Conservative alias eligibility

The SCCP COPY rewrite must retain an independent source/result identity whenever
either value has declared ownership or either value appears as an operand of
MOVE, DROP, or DROP_IF_INITIALIZED anywhere in the function. The whole-function
consumer check is conservative, including consumers after a potential rewrite
and in other blocks; this pass does not attempt to prove availability per edge.

Existing flags, effect tokens, memory tokens, instruction order, and dominance
checks still constrain ordinary COPY aliases. The implementation now lives in
the [private COPY module](execir-copy-aliases.md). Its finite PHI path additionally
resolves pure UNKNOWN COPY chains inside the actual incoming predecessor,
strictly before its terminator, preserving every ordered occurrence. Cross-block,
mapped and exceptional/suspend PHI cases retain their original IDs. Eligible
ordinary UNKNOWN scalar COPY values continue to propagate. V39 independently
ran the existing19 availability cases through three iterations each plus its
consumption budget test, with natural exit0 after the extraction and PHI repair.

This restriction concerns value substitution. It does not change SCCP's numeric
lattice, constant representation, executable-edge calculation, or DCE semantics.

## Verification boundary

The dedicated test first verifies and executes each original function, then
executes the actual SCCP pass and the actual Oracle. It checks signed return 7,
DROP event identity and payload, every before/after ownership state at every
instruction, and three repeated pass invocations. UNKNOWN, UNIQUE, and SHARED
values cover source and result consumption; conditional cleanup uses its legal
owned-value cleanup-block form.

The focused acceptance document records actual toolchain runs and their limits.
The broader scalar and GVN/range milestones remain open.

## Current PHI and scalar regression evidence

Root independently accepted V36's14 PHI cases with94 precondition passes,
six pure Oracle paths and three diagnostics;420 input pins were checked twice
unchanged and35 actual project TUs used UBSan/UNDEBUG. Evidence:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-sccp-phi-green-v36-current-v1.json`,
SHA256 `1298e24afed4cdf9005b553700ac1de4249d65c93944ee1caae08dcb244bc86d`.
V39 also passed33 conversion cases. These partial V39 passes do not rewrite
its failed overall receipt: its scalar checkpoint fixture still expected GC
identity substitution despite the older UNKNOWN-only alias contract.

V40 accepted16 DCE PHI-liveness groups, natural exit0:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/sccp-dce-v40/Root-receipt.json`,
SHA256 `e4c820433f31f4b172d0cec0bded1426623037fdfa5a2144df1572f451755dab`
(231793 bytes). Root also accepted actual scalar checkpoint repair V43 with
natural exit0 and all assertions active, fresh fixture/formal24-TU closure.
Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/sccp-scalar-v43/Root-receipt.json`,
SHA256 `ec86e523923d6aff3068f91214889b0723afac78d8a59a290b9850a627b24a1a`
(223262 bytes). The failed overall V39 receipt remains immutable.

The repaired scalar entry retains a GC case with copied RETURN, one copied
live value/root and copied logical materialization. Its additional UNKNOWN
mode starts with live copied and zero roots, then requires source RETURN/live/
materialized identity and zero roots. Exact counts/IDs, preserved GC COPY
definition, and VerifyALL are asserted. Two modes retain16 main entry calls;
other rollback cases remain unchanged. MaterializeState uses only logical ID
arrays and a zero-initialized request; it performs no provider/resume execution.
The actual V43 pass supplies direct GC preservation and UNKNOWN checkpoint
rebuild evidence without broadening production ownership. Root retains final
independent regression adoption and finite commit ownership; broader scalar
and SSA milestones remain open.

The final scoped regression audit is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-sccp-four-regressions-current-v1.json`,
SHA256 `2510ef577e5561b84f7effc3274e818fa7bf33fa0b1e719490ac7a6e2a43e964`.
It checked449 relevant current pins twice and accepted the four natural-success
native gates, including only V39's successful conversion/COPY closures.
