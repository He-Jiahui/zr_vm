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

Status 2026-10-04: **finite loop11, foundation53 and arithmetic40 accepted from
direct current-workspace evidence**. Root owns native execution; this document
adopts the recorded target results and independent audits. Whole V27/V28
acceptance remains false, and no repeat or full milestone acceptance is implied.

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

Fixture SHA256 remains
`0f2e62ab6cedc0d2be9719672d24357070ddd2ce7cd90ab6f922951f8078d286`.
The RED producer SHA256 was
`b94f8cd91c7cb7b36486e65c0e9931caea30316c15dd54d56d091d1fe2fcc229`.
The current implementation changes that producer and adds a private helper TU;
therefore the prior receipt is not current GREEN evidence.

Current workspace source pins: producer SHA256
`6694d547a1337664009665ab6223006566e866d4cf528d58ae300f672938f7cb`,
loop C `5873074f9f3e7c7973973305b922629f65a8f735f67118d09f6b7090f61787af`,
loop header `ba8c5aa4a83381e8bd31253c383cdec5307a6c015853e0c0586fe08a0215c78b`.
Public branch and private interval headers remain unchanged. All three frozen
fixture hashes remain unchanged. The direct-workspace receipts and independent
adoption reports record actual source/dependency hashes and link participation;
no copied source tree is a current acceptance input.

Root V26 compilation failed because the guard named nonexistent instruction
member `phiIncoming`. Root directly corrected it to `phiRange` in the workspace;
the current helper pin above includes that minimal correction. This compilation
failure is not semantic RED evidence, and prior static review did not establish
build success. V27 completed the loop and foundation targets after that correction.

## Current direct-workspace evidence

All paths below are under
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/`.

| Evidence | SHA256 | Adopted scope |
| --- | --- | --- |
| `direct-workspace-v27/Root-receipt.json` | `740fab0d33fbb4051c53054fe0d07b08c00ff8d660217dabd09b28f838845984` | Completed loop11 and foundation53 |
| `independent-direct-workspace-v27-loop11-foundation53-adoption-v1.json` | `e6f714d559c522e25384e0babca95631aa828c9da20887fc61cddf45fd25e7c1` | Independent adoption of those two targets |
| `direct-workspace-resume-v28/Root-receipt.json` | `04ba575d7189ea4f073def18580858c26a6db1f31846e2f96152b9fe9460323b` | Arithmetic40 natural runtime completion |
| `independent-direct-workspace-v28-arithmetic40-adoption-v1.json` | `16e7fcde0a24a3431a304f44c352e9dcb92bca4a62516c600c1f06b3d8608938` | Independent arithmetic40 adoption |

V27 compiled fixture + BranchFacts producer + private helper + 19 Core support
TUs: all 22 project C TUs were UBSan instrumented. Loop runtime naturally exited
zero with 11 CASE PASS, 33 precondition PASS and
`branch loop ranges: 11 cases, 0 failures, 0 precondition failures`.
The original foundation runtime naturally exited zero with
`branch range/null: 53 cases, 0 failures`. Both completed jobs were reaped with
empty job accounting and no termination actions.

V27 whole acceptance remains false: the arithmetic process never resumed after
process creation exhausted its entry budget, and Root terminated its own job.
Its empty runtime log is not a semantic test failure. V28 reused audited V27
UBSan support objects and compiled the current arithmetic fixture. It naturally
exited zero with exactly 40 distinct CASE PASS and the actual summary
`branch arithmetic constraints: 40 cases, 0 failures`. That log has no
precondition field. V28 whole acceptance remains false because the harness
expected a different summary format; the independent audit adopts the actual
40-case result while preserving both false receipts.

Source and actual dependency pins, object compile provenance and map participation
were checked in the independent audits. Original expectations are unchanged.
At this documentation update, current C source pins still match the recorded
native inputs. Five CMake fragment context pins have subsequently changed while
Root prepares formal CMake validation (loop, foundation, arithmetic constraints,
AOT conditional and AOT arithmetic). The prior native result does not validate
those updated fragments; their fresh CMake acceptance remains open.
Repeat runs and fresh formal CMake validation remain separate open gates.
Earlier copied-source V17/V18 evidence is historical only. Root removed obsolete
owned copied inputs and compiled products; cleanup report
`owned-leaves-cleanup-v27.json`, SHA256
`7941909da5e737634f4114d867c437cd45999128768699da083de45c473f5994`,
records 171 copied-input leaves and 15 obsolete compiled leaves removed,
freeing 5,880,219 bytes. This document does not rely on those removed inputs.

## Limits

Eleven cases cover five positive range proofs plus overflow, missing witness,
poison, two-entry and nested fallback, and malformed PHI diagnostics. Dedicated
overshoot, swapped predicates, permuted roles, missing result witnesses,
nontermination, budget exhaustion and allocation faults are not yet runtime
accepted. The module's finite contract and static reasoning are documented in
[the module note](ssa-branch-loop-range-analysis.md). This evidence does not
close full 02.02, check elimination, 02.05 or the 47-leaf milestone. Linux,
ASan and native32 are not established by these Windows UBSan runs. All validation
products belong under `E:/cargo-targets/zr_vm`; Root owns runs and commits.
