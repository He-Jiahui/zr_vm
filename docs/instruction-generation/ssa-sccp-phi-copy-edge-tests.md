---
related_code:
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/cmake/ssa-sccp-phi-copy-edges-tests.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - tests/parser/ssa_pass_manager_state_maps_cases.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/cmake/ssa-sccp-phi-copy-edges-tests.cmake
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - .codex/plans/20261004-ssa-sccp-phi-copy-edges.md
  - .codex/plans/20261004-scalar-checkpoint-ownership-fixture-diagnosis.md
tests:
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/parser/test_ssa_sccp_copy_availability.c
  - tests/parser/test_ssa_sccp_conversion.c
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/parser/ssa_pass_manager_state_maps_cases.inc
doc_type: test-module-detail
---

# SCCP PHI COPY edge fixture

Status: Root independently adopted actual semantic RED V32. The finite COPY
module and PHI rewrite are implemented. Independent review requires two new
interior-boundary guards. Root adopted their actual semantic RED V33 and the
predecessor scan repair is implemented; Root independently accepted actual
GREEN V36 and actual scalar checkpoint repair V43. The finite regression
evidence remains scoped to the separate gates recorded below.
No source snapshots or copies are created by
this slice.

## Input and assertions

The fixture constructs bounded, explicit ExecIR arrays with a nonzero function
token, external SSA definitions and opaque type identities. Core STRUCTURE|SSA
must accept every ordinary fixture before SCCP runs. Dominators are computed
explicitly; the finite implementation uses only COPY definitions
inside the actual incoming predecessor, strictly before its terminator.
Cross-block definitions remain conservative even when dominance is available.

The original twelve cases cover predecessor COPY, COPY chains, cross-block preservation,
two parallel incoming occurrences with distinct identities, ownership, MOVE,
DROP, logical map presence, exceptional/suspend predecessors and exact malformed
predecessor/definition-order/non-dominance diagnostics. Each legal fixture runs
SCCP twice and is verified after each run. Tests preserve counts, predecessor
slots, PHI result, source COPY opcode and RETURN use of the PHI result. Positive
cases require the expected original source at each incoming slot and a stable
second pass.

The parallel case supplies tagged signed payloads7 and11 and a tagged boolean
condition to the pointer-free Oracle. True and false select actual successor
ordinals0 and1, respectively, despite sharing a target. Before/after/repeat all
must return the corresponding payload with no observable events. This directly
guards slot multiplicity rather than merely checking one matching predecessor.
Only pure COPY/conditional/PHI/RETURN instructions enter this Oracle fixture;
INVOKE, DROP, SUSPEND and the two new interior CALL cases are analysis-only and
supply no callbacks.

The mapped case uses an empty logical table with matching function/generation
identity and independently calls ValidateStateMap before/after/repeat. It tests
conservative map presence, not complete mapped-checkpoint liveness. Malformed
cases first establish a legal input, then mutate one operand/incoming and require
an exact Core diagnostic with unchanged IR hash. They are never optimized or
executed after mutation and do not count as valid Oracle negative examples.

## Output and integration

The independently observed RED is:

```text
SCCP PHI copy edges: 12 cases, 3 failures, 0 precondition failures
```

The observed named failures were `predecessor-copy`, `predecessor-copy-chain` and
`parallel-occurrences-distinct-values`. The nine conservative/diagnostic cases
passed. Observed markers are82 PRECONDITION PASS, six PATH PASS and three
EXPECTED_DIAGNOSTIC PASS. Root's independent adoption receipt is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-sccp-phi-red-v32-adoption-v1.json`,
SHA256 `a1c2d2fc0e90091a45f6d0ead5db2f5a477c0faa4238095e68cc6bfed293b12a`.
The original twelve-case fixture SHA256 was
`ee4576120df16d9480bb7a0f5ffc47c3477b055c463e6bbe200a4675fe5f1ecf`.
Any precondition failure invalidates this semantic
RED contract. The current fourteen-case GREEN requires failure count0 and natural exit0;
Root must observe that result rather than infer it from file presence.

The standalone fragment defines target `zr_vm_ssa_sccp_phi_copy_edges_test` and
CTest `ssa_sccp_phi_copy_edges`. It inherits the current conversion target's
source/include/definition closure and substitutes only the fixture source.
The RED closure had34 TUs, detailed in the design plan. The current GREEN
closure adds the mandatory private COPY helper TU (35 total). Root owns
parent includes, current source/dependency capture, native execution and repeat.
The original twelve routes and other fixtures remain unchanged; two interior
CALL guards extend this dedicated fixture while production remains frozen.

## Interior-boundary review guards

`interior-suspend-call-preserved` and `interior-memory-ordering-preserved` add
COPY -> interior CALL -> pure BRANCH predecessors. Both CALLs have a real SSA
result, zero legal variadic operands, MAY_THROW|MAY_ALLOCATE, actual recognized
heap/FFI tagged token inputs version1 and outputs version2, and effect1->2.
Only the suspension case adds MAY_SUSPEND. No fabricated NOP or unsupported
scheduler token class is used. The native pre/post/repeat verifier level remains
STRUCTURE|SSA; EFFECT/VerifyALL is not claimed and CALL never enters the Oracle.

Both require the incoming COPY identity and unchanged first/repeated SCCP, with
full post-Dominators function hash, source ID and structure preservation. Each
adds six existing preconditions. The observed guard RED against the original
helper was14 CASE lines, the two new semantic failures,94 PRECONDITION PASS,
the unchanged six pure PATH PASS and three EXPECTED_DIAGNOSTIC PASS, exit1:

```text
SCCP PHI copy edges: 14 cases, 2 failures, 0 precondition failures
```

Root independently adopted that actual RED; the original twelve cases passed.
Evidence:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-sccp-phi-guard-red-v33-adoption-v1.json`,
SHA256 `69c80b44c5360dac13065b2daf08caa431c9036d7c4db8d21f876244bb429a81`.
The original receipt remains format-false due to a stdout/stderr-interleaved
precondition line. Main now requests unbuffered stdout without changing any
semantic case, precondition, diagnostic or Oracle assertion.

The repair adds a budgeted predecessor-body scan before each PHI incoming
rewrite. It rejects actual metadata and intrinsic opcode memory/observable
boundaries, including CALL with zero encoded flags. Current expected GREEN is
14 cases,0 failures,0 precondition failures,94 precondition passes, the same
six pure paths and three diagnostics, natural exit0. Root observed and
independently accepted that result in V36, with420 input pins checked twice
unchanged and all35 actual project TUs compiled with UBSan and UNDEBUG.

## Accepted GREEN and regression gates

Actual V36 evidence:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-sccp-phi-green-v36-current-v1.json`,
SHA256 `1298e24afed4cdf9005b553700ac1de4249d65c93944ee1caae08dcb244bc86d`.
This is dedicated STRUCTURE|SSA and pure Oracle acceptance; it does not establish
EFFECT/VerifyALL for every PHI boundary or complete mapped-checkpoint liveness.

V39 separately passed33 conversion cases and19 COPY availability cases with
three iterations each plus the consumption budget test, natural exit0 for both.
The overall V39 receipt remains historically failed because scalar checkpoint
assertions expected a GC-owned COPY to alias. The documented support diagnosis
shows that expectation predates the PHI alias extraction. No historical
receipt is rewritten.

V40 accepted16 DCE PHI-liveness groups, natural exit0. Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/sccp-dce-v40/Root-receipt.json`,
SHA256 `e4c820433f31f4b172d0cec0bded1426623037fdfa5a2144df1572f451755dab`
(231793 bytes).

The scalar checkpoint entry now has two ownership subcases inside its existing
main call: GC preserves copied RETURN/live/root/materialized identities;
UNKNOWN starts with live copied/root0 and must rebuild to live source/root0.
Both assert exact counts/IDs and VerifyALL. The16 main entry calls and other
rollback cases remain unchanged. Materialization is logical ID-array only,
with zero callback/provider fields. Root accepted actual scalar V43: natural
exit0, all assertions active,16 main entry calls and both ownership subcases,
with a fresh fixture in the24-TU formal closure and pinned support objects.
Receipt:
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/sccp-scalar-v43/Root-receipt.json`,
SHA256 `ec86e523923d6aff3068f91214889b0723afac78d8a59a290b9850a627b24a1a`
(223262 bytes). This scoped acceptance does not rewrite V39's historical
failure or close the scalar milestone; Root retains final independent
regression adoption and finite commit ownership.

The final scoped regression adoption is
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b/independent-sccp-four-regressions-current-v1.json`,
SHA256 `2510ef577e5561b84f7effc3274e818fa7bf33fa0b1e719490ac7a6e2a43e964`.
It checked449 relevant current pins twice and accepted the four natural-success
native gates. V39 contributes only its successful conversion/COPY steps;
its failed scalar step and unrelated old control context are excluded.
The three current CMake fragments derive their source root from their actual
location, and the PHI fragment is included after its conversion prerequisite.
The separate registration audit is static; no full parent build is claimed.

Actual semantic RED preceded the cohesive private COPY alias module because
SCCP was964 lines. Its new PHI path matches each actual ordered occurrence and
requires predecessor-local definitions strictly before the actual terminator.
Conversion, COPY availability, DCE and repaired scalar pass manager gates have
the scoped evidence above. Wider propagation and logical liveness acceptance are
separate gates. This test does not close02.01 or the47-leaf milestone.
