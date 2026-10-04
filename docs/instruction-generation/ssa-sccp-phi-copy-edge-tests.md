---
related_code:
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/cmake/ssa-sccp-phi-copy-edges-tests.cmake
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
implementation_files:
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/cmake/ssa-sccp-phi-copy-edges-tests.cmake
plan_sources:
  - docs/plans/ssa/02-automatic-optimization/01-pass-manager-scalar.md
  - .codex/plans/20261004-ssa-sccp-phi-copy-edges.md
tests:
  - tests/parser/test_ssa_sccp_phi_copy_edges.c
  - tests/parser/test_ssa_sccp_copy_availability.c
  - tests/parser/test_ssa_sccp_conversion.c
doc_type: test-module-detail
---

# SCCP PHI COPY edge fixture

Status: dedicated fixture supplied; production SCCP unchanged; actual RED is
pending Root execution. This document describes the test contract, not accepted
runtime results. No source snapshots or copies are created by this slice.

## Input and assertions

The fixture constructs bounded, explicit ExecIR arrays with a nonzero function
token, external SSA definitions and opaque type identities. Core STRUCTURE|SSA
must accept every ordinary fixture before SCCP runs. Dominators are computed
explicitly; the finite requested implementation will use only COPY definitions
inside the actual incoming predecessor, strictly before its terminator.
Cross-block definitions remain conservative even when dominance is available.

Twelve cases cover predecessor COPY, COPY chains, cross-block preservation,
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
INVOKE, DROP and SUSPEND cases are analysis-only and supply no callbacks.

The mapped case uses an empty logical table with matching function/generation
identity and independently calls ValidateStateMap before/after/repeat. It tests
conservative map presence, not complete mapped-checkpoint liveness. Malformed
cases first establish a legal input, then mutate one operand/incoming and require
an exact Core diagnostic with unchanged IR hash. They are never optimized or
executed after mutation and do not count as valid Oracle negative examples.

## Output and integration

The current static expected RED is:

```text
SCCP PHI copy edges: 12 cases, 3 failures, 0 precondition failures
```

The named failures must be `predecessor-copy`, `predecessor-copy-chain` and
`parallel-occurrences-distinct-values`. The nine conservative/diagnostic cases
must pass. Expected markers are82 PRECONDITION PASS, six PATH PASS and three
EXPECTED_DIAGNOSTIC PASS. Any precondition failure invalidates this semantic
RED contract. Expected GREEN changes the failure count to0 with natural exit0;
Root must observe that result rather than infer it from file presence.

The standalone fragment defines target `zr_vm_ssa_sccp_phi_copy_edges_test` and
CTest `ssa_sccp_phi_copy_edges`. It inherits the current conversion target's
source/include/definition closure and substitutes only the fixture source.
That current RED closure has34 TUs, detailed in the design plan. Root owns
parent includes, current source/dependency capture, native execution and repeat.
Existing fixtures and production source are unchanged in this RED-only slice.

The subsequent production phase requires actual semantic RED first, then a
cohesive private COPY alias module because SCCP is already964 lines. Fresh
existing conversion, COPY availability and scalar pass manager regressions
remain required; wider propagation, budget and logical liveness acceptance are
separate gates. This test does not close02.01 or the47-leaf milestone.
