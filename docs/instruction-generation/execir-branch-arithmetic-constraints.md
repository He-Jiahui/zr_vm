---
doc_type: module-detail
status: focused-validated-integration-pending
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_intervals.h
tests:
  - tests/parser/test_ssa_branch_arithmetic_constraints.c
  - tests/parser/test_ssa_branch_range_null.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
---

# BranchFacts checked arithmetic and interval constraints

The parser-owned producer remains read-only and edge-sensitive on DAGs.
The private `exec_ir_branch_intervals.h` supplies checked endpoint mathematics
and interval projections; it introduces no public ABI, allocation, runtime
execution or additional translation unit.

## Checked ADD/SUB

Results need their own explicit signed domain witness. Both operands must be
available signed values with complete, ordered, nonpoisoned intervals. Numeric
type-token identities and pool-backed constants provide no format proof.
Instruction flags or memory/effect tokens block inference.

ADD computes `[left.lower+right.lower, left.upper+right.upper]`; SUB computes
`[left.lower-right.upper, left.upper-right.lower]`. Each endpoint is checked
against i64 MIN/MAX before evaluating the signed host operation. Both endpoint
proofs must succeed; this establishes representability for every pair of values
in the input intervals.

Possible overflow clears bounds and sets `overflowed`. An overflowed input
propagates this poison. Missing witnesses or bounds leave UNKNOWN without
deriving a numerical conclusion. Consumers cannot use `overflowed=false` alone
as proof; they must require complete domain/bounds and their own freshness,
generation, position and operation-specific constraints. Analysis neither
executes an exception nor treats possible overflow as an unreachable edge.

## Comparisons

Pure COMPARE selectors keep the existing true/false inverse mapping. Equality
intersects both intervals. A less-than constraint uses the original right upper
bound to refine the left upper bound, and the original left lower bound to
refine the right lower bound. Less/equal omits the strict unit adjustment;
greater comparisons swap the operands. Both projections use original snapshots.

Empty intersections and impossible strict MIN/MAX boundaries make only that
successor occurrence unreachable. The checks precede any `+1` or `-1` operation.
The interval domain does not retain relations or internal holes. Unequal uses
the original singleton/endpoint exclusion rules and otherwise preserves bounds.
Unknown/poisoned domains and flags or memory/effect tokens prevent refinement.

## Ownership, failures and validation

These helpers allocate nothing and do not modify source instructions. The
existing producer owns facts arrays, validates Core STRUCTURE|SSA, publishes
only successful results and checks revision/generation/IR freshness. The original
53-case fixture remains unchanged, including arithmetic with unknown operands.

The new finite fixture has 40 cases: 18 checked arithmetic and 22 comparison
constraints. It validates source fixtures and freshness before semantic
assertions; it checks no source instruction mutation, result availability and
both successor operands. CHECK remains active under NDEBUG. Root observed
actual preimplementation RED: 40 cases, 31 failures, natural exit 1, including
`add-one` and `lt-true`, without verifier/analysis/freshness precondition errors.
The original V13 controller stayed FAILED because its planned marker name did
not match the actual case name; Root separately audited current products and
the expected RED. No controller receipt is relabeled GREEN here.

Root has now validated the current implementation and repeated both suites:

| Run | Actual result | Root receipt SHA256 | Duration |
| --- | --- | --- | --- |
| V17 new arithmetic/constraints | 40 cases, 0 failures; repeat also PASS | `6594ab37c0214b8ebe5378d02a4d4aae4bed82f9256cc52e72407c0c7b56cdf0` | 6.3969307 s |
| V18 original foundation against extended producer | 53 cases, 0 failures; repeat also PASS | `0e27e0bf7bfc475e61b279b75e43096ceceeec4211f1a57a40dda399546a3450` | 5.1509873 s |

V17 receipt is `reports/task/llvm-branch-green-v17/Root-receipt.json` within
the Root report bundle; V18 is identified by its supplied hash without guessing
a directory name. Root supplied the receipt hashes and actual results; this agent
did not run the executables. The producer and fixture were UBSan-instrumented
with 19 plain Core support translation units. Both jobs naturally exited zero,
ended EMPTY with no cleanup actions, and had current source/tool/resource pins
and actual dependency records. These durations describe validation runs, not
compiler/analysis performance improvements.

The new 40-case CMake fragment still awaits Root inclusion in the parent test
integration; these standalone results are not an integrated CTest pass.
Independent static interval review found no defect in this scoped change.
Dedicated alias, half-bounded and joined-poison cases remain additional coverage
gaps, not implied passes. A separate scoped acceptance record lists the frozen
implementation hashes and evidence limits.

Loop widening/narrowing, pooled representation production, production proof
consumers, real null/bounds-check deletion, memory GVN, runtime/backend
differential validation and proof remarks remain unfinished requirements of
02.02. This slice does not close the 47-leaf acceptance matrix.
