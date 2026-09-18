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
harnesses as ExecIR, ExecBC, AOT, and JIT runners become available.
