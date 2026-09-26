---
related_code:
  - tests/harness/ssa_differential_support.c
  - tests/harness/ssa_differential_support.h
  - tests/parser/test_ssa_oracle_parallel_edges.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
implementation_files:
  - tests/harness/ssa_differential_support.c
  - tests/harness/ssa_differential_support.h
plan_sources:
  - docs/plans/ssa/00-measurement-contracts/03-differential-harness.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/core/test_ssa_differential_harness.c
  - tests/parser/test_ssa_oracle_parallel_edges.c
  - tests/acceptance/ssa-oracle-execbc-parallel-differential.md
doc_type: testing-guide
---

# SSA differential validation harness

`tests/harness/ssa_differential_support.[ch]` provides the first shared
observation boundary for the SSA plan.  An observation records the exact
result type/bit pattern, exception type/source, effect counters, backend
identity, and ordered semantic events (`get`, `write`, `writeback`, `drop`,
allocation, suspension, throw, and return).  Comparison fails at the first
event mismatch and reports its index and source IDs; it never reduces a
difference to a final numeric value.

Fixtures run through an explicit callback.  A missing callback is reported as
`BACKEND_UNSUPPORTED`, not as an empty successful observation.  Coverage tracks
required, executed, and failed backend bits, so a partially executed matrix
cannot be accepted even if the executed rows happen to agree.
Coverage recording consumes the actual observation and semantic comparison
outcome, not a caller-supplied success flag alone. A fallback can have identical
results and events yet fail the requested backend's native-coverage gate; a
reported actual backend mismatch or malformed observation also fails that gate.
Failures remain sticky across subsequent recordings of the same backend.

The focused `ssa_differential_harness` CTest exercises result bit-pattern,
exception, event-order, unsupported-runner, and coverage failure paths.  The
same support API can later be connected to the existing runtime/reference
harnesses as ExecIR, ExecBC, AOT, and JIT runners become available. The
`ssa_oracle_parallel_edges` test now runs one verified, pointer-free function
through the direct ExecIR oracle and projected ExecBC runner as separate
backend identities. Its four conditional-branch/switch observations compare
the selected parallel phi result and the actual return source event. A
test-only source mismatch confirms the event comparison fails at index zero.
This proves only the scalar/control return subset; effect, drop, exception,
writeback, and runtime-default ExecBC parity remain pending.
