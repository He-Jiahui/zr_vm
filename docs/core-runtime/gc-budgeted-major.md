---
related_code:
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
  - zr_vm_core/include/zr_vm_core/gc_budget_contract.h
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget.c
  - zr_vm_core/src/zr_vm_core/gc/gc_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_compact.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/gc_budget_contract.h
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
  - zr_vm_core/include/zr_vm_core/gc_major.h
  - zr_vm_core/include/zr_vm_core/gc_compact.h
  - zr_vm_core/src/zr_vm_core/gc/gc_budget.c
  - zr_vm_core/src/zr_vm_core/gc/gc_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_compact.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/02-major-budget.md
tests:
  - tests/core/test_ssa_major_budget.c
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_gc_budget_constructor_defaults.inc
  - tests/acceptance/2026-09-29-gc-budget-constructor-defaults.md
doc_type: module-detail
status: in-progress
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

The scalar budget contract remains separate from the collector's concurrent
major state machine. The existing concurrent-major code remains responsible for
root snapshot, mark queue ownership, remark, sweep, and the collector lock
protocol. Collector construction must nevertheless establish the budget fields
before those public getters can observe them.

## Collector budget defaults

`ZrCore_GarbageCollector_New` receives storage from the non-zeroing raw
allocator. Its constructor must explicitly initialize the budget object and
runtime telemetry; clearing the whole collector would hide unrelated lifecycle
initialization omissions. The default contract for a newly constructed
collector is:

- budget is not configured, so `GetBudget`, global `GetBudgetStats`, and state
  `GetStats` return false;
- status is `ZR_GC_BUDGET_STEP_ACCEPTED`, phase is
  `ZR_GC_BUDGET_PHASE_IDLE`, and pause reason is `ZR_GC_BUDGET_PAUSE_NONE`;
- cursor, work, elapsed time, debt, and counters are zero; pressure and fallback
  are false;
- the corresponding budget fields in the stats snapshot carry the same
  defaults.

The unconfigured `GetBudget` output is unspecified and is not part of this
contract. Initialize the budget record with `ZrCore_GcBudget_Init` and assign
each collector and snapshot telemetry field explicitly. The allocator-seeded
constructor regression is in `tests/core/test_gc_budget_constructor_defaults.inc`.
Its RED and GREEN results and exact commands are recorded in
`tests/acceptance/2026-09-29-gc-budget-constructor-defaults.md`; the production
initialization code now sets these defaults explicitly. The root-owned MSVC
C11 Debug build completed 71/71 steps, and the direct concurrent-major
executable passed all 11 tests, including this regression. That executable has
no CTest registration.

`tests/core/test_ssa_major_budget.c` covers bounded cursor advancement, budget
rejection without cursor publication, atomic-pause and pressure reporting,
compact deferral, malformed input, and cursor overflow. It is run as a direct
focused fixture in this stage; shared CTest registration belongs to the parent
integration task.

## Runtime seams

`SZrGcBudgetLedger` provides a pointer-free cumulative view for a scheduler.
`ZrCore_GcBudget_LedgerAccumulate` accepts only coherent results and preserves
cursor monotonicity; telemetry counters saturate instead of wrapping. The
`SZrGcMajorState` contract makes the ordered
`INITIAL_SNAPSHOT -> CONCURRENT_MARK -> REMARK -> SWEEP -> COMPACT -> COMPLETE`
transitions explicit and rejects phase skips or an inconsistent publication
boundary. It is an adapter witness, not a second heap queue: the existing
`gc_concurrent_major.c` remains the owner of locks, roots and object marking.

`ZrCore_GcCompact_Plan` is the admission boundary for selective compaction. It
counts fragmented old regions, excludes pinned/large/permanent regions, and
selects whole regions that fit the requested byte budget. A non-moving or
budget-insufficient request is reported as deferred; `plannedBytes` is never
reported as moved object bytes until a relocation implementation has completed
its own transaction. The focused test covers pinned-region exclusion,
whole-region budgeting, non-moving fallback, and malformed region facts.
