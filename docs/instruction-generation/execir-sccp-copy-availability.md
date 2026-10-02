---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
tests:
  - tests/parser/test_ssa_sccp_copy_availability.c
  - tests/cmake/ssa-sccp-copy-availability-tests.cmake
  - tests/acceptance/2026-10-03-ssa-sccp-copy-availability.md
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
checks still constrain COPY aliases. PHI incoming values retain their original
IDs because this rewrite lacks an edge-use context. Eligible ordinary UNKNOWN
scalar COPY values continue to propagate, and repeated passes reach a fixed point.

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
