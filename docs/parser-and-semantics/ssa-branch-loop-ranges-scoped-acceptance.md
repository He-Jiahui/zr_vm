---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
  - tests/parser/test_ssa_branch_loop_ranges.c
  - tests/cmake/ssa-branch-loop-ranges-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_loop_ranges.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_branch_facts.c
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/02-gvn-range.md
  - .codex/plans/20261004-ssa-branch-loop-widening-narrowing.md
tests:
  - tests/parser/test_ssa_branch_loop_ranges.c
  - tests/parser/test_ssa_branch_range_null.c
  - tests/parser/test_ssa_branch_arithmetic_constraints.c
doc_type: scoped-acceptance
---

# Branch loop ranges: scoped evidence

Status2026-10-04: actual semantic RED established by Root; current private helper
candidate supplied, **GREEN and current53/40 regression acceptance pending**.
This agent performed static review and file edits only, without compiler,
runtime, WSL, native jobs or Git/index/lock actions.

## Actual RED

Root V22 receipt: `reports/task/llvm-branch-loop-red-v22/Root-receipt.json`,
SHA256 `907e53fd7e9e94bff06eaab8846649f3eb274e04997279bf7297151f32238f91`,
128463 bytes,5.4483443s whole run. Compiler/link natural exit0; intended runtime
natural exit1, Job EMPTY/reaped/no actions. Fixture and producer were UBSan
instrumented with19 plain Core support TUs. There were11 cases,5 named positive
failures, six conservative/diagnostic passes,33 CoreVerify/Analyze/Freshness PASS
lines and zero precondition failures. The failures were induction0→10, zero
trip, descending ADD, descending SUB and strict MAX. Malformed PHI diagnostics
were checked separately and did not manufacture feature RED.

Frozen fixture SHA256 remains
`0f2e62ab6cedc0d2be9719672d24357070ddd2ce7cd90ab6f922951f8078d286`.
The RED producer SHA256 was
`b94f8cd91c7cb7b36486e65c0e9931caea30316c15dd54d56d091d1fe2fcc229`.
The current candidate changes that producer and adds a private helper TU;
therefore the prior receipt is not current GREEN evidence.

Current source freeze: producer SHA256
`6694d547a1337664009665ab6223006566e866d4cf528d58ae300f672938f7cb`,
loop C `5873074f9f3e7c7973973305b922629f65a8f735f67118d09f6b7090f61787af`,
loop header `ba8c5aa4a83381e8bd31253c383cdec5307a6c015853e0c0586fe08a0215c78b`.
Public branch and private interval headers remain unchanged. All three frozen
fixture hashes remain unchanged; the loop design records the current fragment
pins. Native evidence must match these current sources and actual dependencies.

Root V26 compilation failed because the guard named nonexistent instruction
member `phiIncoming`. Root directly corrected it to `phiRange` in the workspace;
the current helper pin above includes that minimal correction. This compilation
failure is not semantic RED evidence, and prior static review did not establish
build success. V27 native validation remains pending in this note.

## Current validation gate

Root must compile fixture + BranchFacts producer + private loop helper + the19
Core support TUs (22 total), capture current actual dependencies and source pins,
and observe `branch loop ranges: 11 cases, 0 failures, 0 precondition failures`,
11 CASE PASS,33 precondition PASS and natural exit0. A repeat and fresh original
53/new arithmetic40 regressions with repeats remain required. Original fixture
expectations are unchanged. Parent CMake includes and genuine CMake validation
remain Root integration gates; standalone fragments contain the mandatory TU.

The earlier arithmetic GREEN V17/V18 remains historical acceptance for the
before-loop revision. Root preserved its nine-file snapshot under
`tmp/task/branch-intervals-40-before-loop/manifest.json`, SHA256
`810198a28f5869e9b8e4dc72833f9a0681efbe495c713b06c5b4121d5d20066c`.
It cannot establish this candidate's current regression result.

## Limits

Eleven cases cover five positive range proofs plus overflow, missing witness,
poison, two-entry and nested fallback, and malformed PHI diagnostics. Dedicated
overshoot, swapped predicates, permuted roles, missing result witnesses,
nontermination, budget exhaustion and allocation faults are not yet runtime
accepted. The module's finite contract and static reasoning are documented in
[the module note](ssa-branch-loop-range-analysis.md). This evidence does not
close02.02, check elimination,02.05 or the47-leaf milestone. All validation
products belong under `E:/cargo-targets/zr_vm`; Root owns runs and commits.
