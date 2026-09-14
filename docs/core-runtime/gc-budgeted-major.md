---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/gc_budget_contract.h
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/gc_budget_contract.h
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/02-major-budget.md
tests:
  - tests/core/test_ssa_major_budget.c
doc_type: module-detail
status: focused-passed
---

# Budgeted major GC contract

`gc_budget_contract` is the scalar boundary for a resumable major-collection
step. It is deliberately separate from the collector's managed state: the
record contains no VM object, queue, callback, or host address. A collector can
validate the budget, execute one bounded unit, and persist the returned cursor
and telemetry only at the reported coherent boundary.

## Budget and phase rules

`SZrGcBudget` supports independent elapsed-microsecond, work-unit, byte, and
object limits. A zero limit means that dimension is not independently limited;
at least one of the four scheduling dimensions must be configured. Unknown
flags, nonzero reserved data, malformed magic/schema, and a compact permission
without a nonzero byte budget are rejected with a scalar diagnostic.

The step evaluator accepts only the active collection phases from initial
snapshot through compact. It advances `nextCursor` only when all configured
limits permit the unit. A unit that would exceed a scheduling budget remains at
the input cursor and reports `ZR_GC_BUDGET_PAUSE_BUDGET`; callers can retry it
with a later budget without exposing a partially published scan.

The cursor addition is checked for unsigned overflow. This matters for a
persisted cursor because wrapping to a low value could repeat already scanned
work and invalidate the major-cycle invariant.

## Pause and pressure telemetry

An atomic pause larger than `maxAtomicPauseUs` is accepted as completed work but
reported as `ZR_GC_BUDGET_STEP_OVER_BUDGET`, with an over-budget count and the
measured maximum pause. Positive debt at or above `pressureThresholdBytes`
sets the pressure bit when pressure reporting is enabled. These fields are
observations for scheduling and backpressure, not a hard real-time guarantee.

Compaction is a separate safe boundary. A compact unit is deferred when the
caller has not enabled compact or when its byte budget is too small; the result
keeps the cursor unchanged and reports
`ZR_GC_BUDGET_PAUSE_DEFERRED_RELOCATION`. This prevents a scheduler from
silently turning a selective compact request into an unbounded relocation.

## Scope and follow-up

This leaf does not alter `gc_cycle.c`, the public `gc.h` collector lifecycle, or
the CMake test registry. It supplies the validation and telemetry contract for
the later major state-machine integration. The existing concurrent-major code
remains responsible for root snapshot, mark queue ownership, remark, sweep,
and the actual collector lock protocol.

`tests/core/test_ssa_major_budget.c` covers bounded cursor advancement, budget
rejection without cursor publication, atomic-pause and pressure reporting,
compact deferral, malformed input, and cursor overflow. It is run as a direct
focused fixture in this stage; shared CTest registration belongs to the parent
integration task.
