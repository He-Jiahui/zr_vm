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
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/gc_budget_contract.h
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_contract.c
  - zr_vm_core/include/zr_vm_core/gc_major.h
  - zr_vm_core/include/zr_vm_core/gc_compact.h
  - zr_vm_core/src/zr_vm_core/gc/gc_budget.c
  - zr_vm_core/src/zr_vm_core/gc/gc_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_compact.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_concurrent_major.c
  - zr_vm_core/src/zr_vm_core/gc/gc_budget_runtime.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/02-major-budget.md
tests:
  - tests/core/test_ssa_major_budget.c
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_gc_budget_constructor_defaults.inc
  - tests/acceptance/2026-09-29-gc-budget-constructor-defaults.md
  - tests/acceptance/2026-09-29-gc-major-max-objects-slice.md
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

The scalar budget contract remains separate from most of the collector's
concurrent-major state machine. The existing concurrent-major code remains
responsible for root snapshot, mark queue ownership, remark, sweep, and the
collector lock protocol. Collector construction must establish the budget
fields before those public getters can observe them.

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

## First runtime consumer: per-slice `maxObjects`

`garbage_collector_concurrent_major_mark_slice` now uses an installed,
nonzero `maxObjects` to lower the caller's queue-pop cap for that invocation.
It never raises the caller cap. A zero `maxObjects` retains the caller's
existing cap, including the current default of eight pops for one worker. The
limit counts gray objects popped; it does not bound the scan cost of one object,
the initial pause, remark, sweep, or the complete major cycle. This is a
per-slice limit, not a claim that the complete major collection is bounded.

The real collector regression is in
`test_concurrent_major_slice_honors_max_objects` and its zero/large-limit
variants. It verifies a live gray queue prefix placed ahead of pre-existing
roots, then checks the result of one public `GcStep` and cleans up the cycle and
temporary roots before Unity assertions. The RED and root-owned GREEN evidence
are recorded in
`tests/acceptance/2026-09-29-gc-major-max-objects-slice.md`.

Before the temporary malformed-comment edit, root-owned MSVC Debug verification
completed the focused 16-target build at 471/471 steps and directly ran all 14
`zr_vm_gc_concurrent_major_test` cases, including the `maxObjects=1`, `0`, and
`64` variants. A later GCC/WSL 18-target attempt failed at 6/969 steps while
parsing that malformed `gc.h` comment (C compilation failure, exit 1); the
comment was corrected afterward. With the corrected current header, a new
root-owned MSVC Debug incremental build covering five targets completed
352/352 steps, and the direct GC Unity run passed 14/14, exit 0. A later Clang
GC direct run completed seven cases, including all three `maxObjects` variants,
then deadlocked in the eighth case. GDB confirmed the main marker waiting for
the mutation mutex while a mutator waited at a nested safepoint boundary. The
owned test process was terminated after preserving its state and thread stacks;
this is partial Clang evidence, not a green full suite. The large GCC build is
still running. The target has no CTest registration; see the acceptance record
for exact commands and logs.

This path does not call `ZrCore_GcBudget_EvaluateBudgetStep`; after `SetBudget`,
budget stats therefore remain at their configured baseline (`ACCEPTED/IDLE`,
cursor/work zero). Elapsed-time, work-unit, byte, pause, pressure, and phase
budgets are not connected to concurrent-major execution by this slice. The
overall 06.02 plan remains in progress.

The slice reads `budgetConfigured` and `maxObjects` under the domain mutation
lock. `SetBudget` uses the same recursive lock for its configuration and budget
snapshot writes, so an update is serialized with a concurrent mark slice in a
registered GC domain. This does not make
`ZrCore_GarbageCollector_GetBudget`,
`ZrCore_GarbageCollector_EvaluateBudgetStep`,
`ZrCore_GarbageCollector_GetBudgetStats`, or `ZrCore_Gc_GetStats` generally
thread-safe; callers must still serialize those APIs with budget writes and
evaluation. When a global has no initialized GC domain, the lock helpers
provide no cross-thread synchronization.

`ZrCore_GarbageCollector_GetStatsSnapshot` is outside that synchronization
guarantee too: it does not take the mutation lock used by `SetBudget`, while
mark-slice counters are currently updated after releasing that lock. Callers
must serialize snapshot reads with budget changes and GC stats updates.

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
