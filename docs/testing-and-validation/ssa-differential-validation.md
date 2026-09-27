---
related_code:
  - tests/harness/ssa_differential_support.c
  - tests/harness/ssa_differential_support.h
  - tests/parser/test_ssa_oracle_parallel_edges.c
  - tests/parser/test_ssa_oracle_memory_differential.c
  - tests/parser/test_ssa_oracle_call_differential.c
  - tests/parser/test_ssa_oracle_invoke_differential.c
  - tests/parser/test_ssa_execbc_place.c
  - tests/parser/test_ssa_oracle_iterator_differential.c
  - tests/parser/test_ssa_oracle_resume.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_execbc.h
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
  - tests/acceptance/ssa-oracle-execbc-place-differential.md
  - tests/acceptance/ssa-oracle-execbc-iterator-differential.md
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
The Place differential fixture separately runs both `PLACE_BASE` and
`PLACE_PROJECT` with a caller-owned token provider followed by `LOAD/RETURN`.
It compares the independent Oracle and ExecBC return values, provider counts,
and address/event source identity. Missing, rejecting, and undefined-result
ExecBC Place providers fail at the precise instruction without replacing the
previously published execution result. This enables pointer-free projection
testing, not host-pointer materialization or production bytecode execution.
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
The same target now compares verifier-valid ownership cleanup: direct-oracle
and projected DROP snapshots, return values, and owner states for ordinary
DROP, repeated guarded DROP, MOVE followed by guarded no-op, and a branch
join with owner initialization on only one path. The uninitialized branch
must skip DROP with no event. A mutated RETURN that reads a consumed owner
reports INVALID_VALUE with its source and preserves the prior result. These
tests do not establish exceptional cleanup, ownership-provider effects, or
production backend parity.

The verified BARRIER fixture records two ordered, distinct value snapshots
plus RETURN in the same oracle/projected harness. Changing the second
barrier payload is detected at event index one; an invalid projected barrier
operand reports INVALID_VALUE at its instruction/source and leaves the prior
published result intact. This asserts observable pointer-free barrier events,
not an actual GC write-barrier implementation.

The verifier-valid terminal THROW fixture compares pointer-free payload,
exception observation, source/instruction identity, event order, and
non-returning termination between the direct oracle and projected ExecBC.
A changed THROW payload mismatches at event index zero; a bad projected
operand reports INVALID_VALUE at its source without replacing the prior
result. Handler entry and resumption remain unverified.

The terminal SUSPEND differential compares oracle/projected suspended flags,
payload and SSA result, execution length, and bounded event snapshots for
one- and five-operand verifier-valid functions. A fifth invalid operand
reports INVALID_VALUE with source identity and preserves the published
result even though it is not present in the four-value event snapshot.
Checkpoint capture and resume are not covered by this projected runner.

The verifier-valid INVOKE fixture compares normal and exceptional CFG edges,
CALL event operand/source identity, instruction count, return block, and
handler payload across the direct oracle and projected ExecBC. Mutating the
event snapshot fails differential comparison. Missing or rejecting providers,
and an undefined normal result, leave the last published projection intact;
the exceptional edge does not define a normal result. This fixture exercises
pointer-free handler entry, not production exception state, checkpoint/resume,
or AOT execution.

The verifier-valid ITER_INIT, ITER_MOVE_NEXT, and ITER_CURRENT fixtures now
compare independent Oracle/ExecBC provider calls, ordered normal/exception
successors, ITERATOR event snapshots, and handler payloads. Missing or rejected
ExecBC providers and an undefined normal result report precise iterator
instruction/source diagnostics without replacing the previously published
result. A throw with an undefined callback value leaves the normal result slot
undefined. These are pointer-free test-runner checks, not executable production
iterator or AOT coverage.

The `ssa_oracle_resume` fixture also stops immediately after a verifier-valid
INVOKE effect, compares its complete CALL snapshot with an independent
projected run, then resumes the oracle and compares ordered CALL/RETURN
observations and results for both normal and exceptional successors. The
provider count stays one after resume even when its edge choice is changed
between pause and resume. An injected event mismatch is detected at index
zero. This does not exercise projection-side checkpoint restoration.
