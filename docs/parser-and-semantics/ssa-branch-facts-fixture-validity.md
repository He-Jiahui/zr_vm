---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_branch_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - tests/parser/test_ssa_branch_range_null.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_branch_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
  - docs/parser-and-semantics/ssa-branch-range-null-analysis.md
tests:
  - tests/parser/test_ssa_branch_range_null.c
doc_type: testing-guide
---

# Branch facts fixture validity and rejection diagnostics

## Verification boundary

The branch facts producer validates owned storage and exact module/function
binding, then calls the Core function verifier with STRUCTURE and SSA levels.
Range and null facts are computed only after this boundary succeeds. Fixtures
that aim to exercise dataflow must satisfy the same Core identity and SSA rules
as a production function.

The Core function validator requires both a nonzero function ID and a nonzero
`functionToken`. Assigning only `id` does not establish a valid function. The
generic fixture constructor is reused by predicate, scalar and graph cases;
the 128-block DAG has a separate constructor and must satisfy this requirement
independently. Module binding still depends on the exact owned function pointer.

The fixture's `analyze` helper logs a rejected call's diagnostic code, block,
instruction, source, expected value and actual value. These diagnostics precede
the assertion failure so the layer rejecting the input can be distinguished
from an incorrect fact. Negative cases still assert their original diagnostic
codes and positions; no assertion or verifier requirement is relaxed.

## Evidence status

The Root native Clang/LLD run in
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/llvm-branch-green-v7`
compiled and linked the producer and fixture, then reported 53 cases with 49
failures. Most positive cases stopped at `analyze`, before fact assertions.
The shared fixture and independent DAG both had zero `functionToken`; static
inspection identifies the Core identity check as the candidate common cause.
Root confirmed the identity precondition against the Core validator and the
actual rejection log. Both fixture constructors now assign `functionToken = 1`.
The producer and verifier remain unchanged; the repaired fixture awaits an
actual Root run described below.

The `llvm-branch-green-v9` Root receipt records an actual 53-case run and a
repeat run, both with zero failures and natural exit code 0. The unchanged
Core verifier and original fact expectations were used. UBSan instrumented the
new producer and fixture; the 19 current Core compile objects were validated
and reused without sanitizer instrumentation. This establishes the scoped
branch facts fixture result, not completion of milestone 02.02. Full evidence
and the earlier missing-feature RED are recorded in
[the scoped acceptance note](ssa-branch-facts-scoped-acceptance.md).
