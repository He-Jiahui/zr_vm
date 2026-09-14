# Major-GC budget runtime integration

`gc_budget_contract.h` defines the pointer-free witness for one bounded major-GC
slice.  The collector now exposes that contract through four optional APIs:

* `ZrCore_GarbageCollector_SetBudget` validates and atomically installs a
  budget.  Invalid updates leave the prior budget, cursor, and counters intact.
* `ZrCore_GarbageCollector_GetBudget` returns the currently validated budget.
* `ZrCore_GarbageCollector_EvaluateBudgetStep` evaluates one phase slice using
  the collector's persisted cursor and GC debt.  Accepted slices publish the
  next cursor; rejected, over-budget, or deferred slices do not.
* `ZrCore_GarbageCollector_GetBudgetStats` returns the last phase, pause reason,
  cursor, work/time totals, pressure, fallback, and deferral counters.

The scheduler state is scalar and lives alongside the collector.  This keeps
legacy `GcStep`/pause-budget behavior unchanged while giving concurrent-major
drivers a collector-owned seam for deterministic slicing.  A rejected or
atomic-overrun result sets `budgetFallback`, allowing a caller to switch to a
safe fallback path explicitly.  Compact slices are deferred unless the
validated contract carries `ZR_GC_BUDGET_FLAG_ALLOW_COMPACT` and a non-zero
`compactBudgetBytes`.

All updates are bounded and transactional: malformed budgets are rejected
before mutation, arithmetic saturates telemetry counters, and the contract's
cursor is advanced only at a consistent accepted boundary.
