# Syntax 12 M2 Acceptance: Task/Frame Runtime

## Scope

This acceptance covers only Syntax 12 M2 from
`docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md`. It
establishes the core runtime Task/frame state machine that later lowering and
scheduling milestones consume. It does not claim source `await` lowering,
Job/Scheduler/ThreadScheduler support, artifact or AOT frame rows, debug/LSP
projection, or migration of the legacy `TaskRunner` surface.

## Accepted Contract

- Starting a task invokes its poll callback without allocating a coroutine
  frame. Only `Suspend(stateId)` rents a capacity-matched frame from the pool.
- The layout owns the valid state-id range, each retained slot's GC-root and
  drop requirements, and an optional once-only finally callback. No dynamic
  object property or callable name supplies those facts.
- Complete, fault, and early task free execute the layout finally callback at
  most once. It executes while initialized frame slots remain observable, then
  the runtime releases roots and drops initialized slots before returning a
  frame to its pool.
- GC-rooted slots and completed task results use `SZrGcRootHandle` resolution
  across collection. A non-Copy ownership result transfers on its first await;
  later awaits return `RESULT_CONSUMED`. Plain results remain multi-awaitable.

## RED To Green

The initial focused target failed because the Task/frame public header did not
exist. Subsequent RED checks caught missing completed-result roots and a slot
overwrite path that skipped the custom drop callback. The final fault/finally
RED proved that a fault cleaned slots without running a layout cleanup callback;
the completed implementation now observes that callback before frame cleanup
and prevents a later `Task_Free` from invoking it again.

During MSVC replay, the pool reuse test exposed an unrelated test-fixture
lifetime bug: it created the second raw object before a full collection without
a root, then reused that stale C pointer. The fixture now creates that object
after the collection; no runtime semantics were relaxed.

## Validation Evidence

The final source snapshot was `08bcfb8` plus the exact M2 header, source,
test, and CMake target overlays. It included the repository's initialized
read-only submodules because `git archive` deliberately omits gitlinks. Each
toolchain used its own build directory.

| Toolchain | Task/frame target | GC-domain regression | Process result |
|---|---:|---:|---|
| GCC 11.4 | 6/6 | 5/5 | both exit 0 |
| Clang 14 | 6/6 | 5/5 | both exit 0 |
| MSVC 19.44 | 6/6 | 5/5 | both exit 0 |
| Clang 14 ASan/UBSan | 6/6 | not repeated | exit 0 |

The Task/frame target covers synchronous no-allocation completion, multiple
suspend/resume states, initialized-only fault drop with finally-before-cleanup,
GC-rooted suspended values and pool reuse, non-Copy exactly-once await, and a
completed task header root surviving full collection.

LeakSanitizer cannot run reliably in this WSL environment: enabling
`detect_leaks=1` terminates before a diagnostic is emitted. AddressSanitizer
and UndefinedBehaviorSanitizer ran with `detect_leaks=0` and `halt_on_error=1`;
the 6/6 result is memory-error evidence, not a claim of leak-detection
coverage.
