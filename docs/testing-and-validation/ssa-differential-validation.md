---
related_code:
  - tests/harness/ssa_differential_support.c
  - tests/harness/ssa_differential_support.h
  - tests/parser/test_ssa_oracle_parallel_edges.c
  - tests/parser/test_ssa_oracle_memory_differential.c
  - tests/parser/test_ssa_oracle_call_differential.c
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
  - tests/parser/test_ssa_oracle_memory_differential.c
  - tests/acceptance/ssa-oracle-execbc-parallel-differential.md
  - tests/acceptance/ssa-oracle-execbc-memory-differential.md
  - tests/acceptance/ssa-oracle-execbc-call-differential.md
doc_type: testing-guide
---

# SSA differential validation harness

`tests/harness/ssa_differential_support.[ch]` provides the first shared
observation boundary for the SSA plan.  An observation records the exact
result type/bit pattern, exception type/source, effect counters, backend
identity, and ordered semantic events (`get`, `write`, `writeback`, `call`, `drop`,
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
The `ssa_oracle_projections` suite also executes a fully verifier-checked
STORE/LOAD/RETURN function on separate oracle and projected memory providers.
It compares observed write/read/return order, source IDs, the STORE address
and value snapshots, return bits, and external memory contents. A changed
event kind or STORE address must mismatch at index zero; a separate verified
STORE-only function also covers successful provider-free event recording.
Missing and rejected providers and invalid loaded values
report their actual instruction/source and preserve the previously published
projected result. This establishes the pointer-free memory-provider subset,
not full effects, drop, exception, writeback, production ExecBC, or AOT parity.
The same `ssa_oracle_projections` target now verifies ordinary CALL with five
arguments and a nine-argument allocation-path case. It compares direct-oracle
and projected return values plus four bounded CALL operand snapshots. The
provider receives every argument, including those omitted from event storage;
a deliberate fourth-snapshot mutation is detected at event index three.
Missing/rejected callbacks and undefined callback values keep the earlier
projected result and report the call's instruction/source identity. This
provider-only result does not establish exception or native calling parity.
Invalid initial-value and constant kinds are rejected before either CALL
provider runs, with diagnostic instruction ID zero and unchanged projected
result. Event-allocation OOM parity remains unverified: the projection
reserves before invoking the provider, but the oracle appends afterward.
