# GC domain nested mutation and stop-the-world pause

Date: 2026-09-30

## Scope

This regression covers the runtime GC domain's recursive mutation lock and
domain-local stop-the-world handshake during an active concurrent major mark.
The production change tracks successful locked mutation scopes per registered
mutator. A nested mutation entry and any poll or VM/native entry inside that
scope must not park while the thread still owns the mutation lock. The outer
scope releases the lock and polls if a domain pause is still requested.

## Baseline

The root-owned Clang run and GDB inspection reported the real SSA 06 regression
as RED. The collector was stopped in
`ZrCore_GcDomain_MutationLock` from `major_mark_slice` while finishing a
concurrent major step. The worker was stopped in
`gc_domain_wait_for_entry_boundary_locked` from a nested `MutationBegin` in
`object_set_existing_pair_value_after_fast_miss_unchecked_core`, called during
`object_set_value_core`. The worker had been marked parked while still holding
the outer recursive mutation lock; the collector waited for that lock and the
worker waited for the pause to end.

The handoff explicitly prohibited rerunning that unbounded pre-fix test. This
acceptance records the stack evidence supplied by the root-owned investigation;
it does not claim a new pre-fix execution from this worktree.

## Test inventory

- `zr_vm_gc_nested_mutation_test` / `gc_nested_mutation` runs one real
  concurrent-major cycle and one deterministic domain pause.
- A worker enters a locked outer mutation scope, waits until the public domain
  snapshot shows `pauseRequested`, then updates an existing integer-key object
  entry through `ZrCore_Object_SetValue`. That is the production fast-miss path
  that opens nested mutation scopes.
- The collector calls public `ZrCore_GcDomain_StopTheWorldBegin` with a
  1,500 ms timeout. Before releasing the pause, the fixture records whether the
  nested object mutation has finished. This catches the reported failure mode:
  the old code reports the worker parked from inside nested `MutationBegin`,
  before the object write can finish.
- Regardless of observations, the fixture ends a successfully acquired pause,
  releases the worker, waits for its atomic exit flag for a bounded interval,
  then joins it before draining the major cycle with a fixed step cap and
  full-GC fallback. If the worker misses that deadline or join fails, the
  fixture prints a diagnostic and exits the test process without freeing state
  or stack context that a live worker could still use. After a successful join,
  it checks the stored value through public `ZrCore_Object_GetValue` before GC
  draining and removing the ignored root; assertions run after cleanup.
- CTest has a 15 second process timeout. No sanitizer or platform matrix was
  run as part of this subtask.

## Tooling evidence

- Root-owned Clang and GDB evidence identified the deadlocked production stack
  before changes. The full command and binary path remain in the root-owned
  session record.
- The first diagnostic CTest attempt timed out after 41.91 seconds and produced
  no stdout or stderr. Its cause was not determined, and that timeout has not
  been reproduced by the latest bounded diagnostic run.
- `D:/tmp/zr_vm/ssa-control/gc-nested-direct-diagnostic.log` records the current
  direct fixture run: 1 test, 0 failures, 0 ignored; owned PID 44176, elapsed
  0.296 seconds, exit 0. Its flushed stage records show STW acquired, no
  timeout, pause active, one parked mutator, nested mutation completed, worker
  joined, getter verified the value, and major drain finished.
- `D:/tmp/zr_vm/ssa-control/gc-nested-diagnostic-ctest.log` records 1/1 CTest
  passing in 0.60 seconds, 0.75 seconds total, exit 0.
- `D:/tmp/zr_vm/ssa-control/accessor-vm-gc-build.log` records a 13/13 build
  including the new target; `gc-current-suite-build.log` records the actual
  1/1 relink.
- `D:/tmp/zr_vm/ssa-control/gc-major-depth-direct.log` records 14 current
  tests, 0 failures, 0 ignored, elapsed 41.251 seconds, `timedOut=False`, exit
  0. This elapsed time is recorded as observed; it is not evidence of a strict
  30 second upper bound.
- Current Clang verification passed: `clang-current-ten-target-build.log`
  records 85/85 build steps across ten targets; `clang-current-eight-ctests.log`
  records the focused `gc_nested_mutation` CTest passing 1/1 in 0.37 seconds
  and its eight-test GC group passing 8/8; `clang-gc-current-depth-suite.log`
  records the actual current-source major-depth suite passing 14 tests with 0
  failures and 0 ignored, exit 0.
- The optional old-source D-only probe was abandoned after infrastructure,
  parser, bootstrap, and link failures. One old-source object compiled, but no
  old-source test ran; this probe adds no RED result and is not a remaining
  acceptance gate. The prior GDB evidence remains the valid old-source RED.
- `D:/tmp/zr_vm/ssa-control/gcc-current-thirteen-ctests.log` records the
  current GCC rebuild completing 107/107 steps and all 13 targeted CTests
  passing. `gc_nested_mutation` took 1.77 seconds; the group took 71.52 seconds
  total.
- A GCC 14 direct run is still pending and is not claimed here. Other SSA
  06.02 leaf gates remain open.

## Results

- Added `mutationDepth` to the internal mutator record. Registry growth copies
  the full record and unregister shifts full records under `coordinationLock`.
- `MutationBegin` re-finds a record after releasing `coordinationLock`, and
  increments depth only after acquiring the recursive mutation lock. `End`
  re-finds and decrements under the coordination lock, releases one lock level,
  then polls at the outermost scope when pause remains requested.
- Entry-boundary waits and `MutatorPoll` defer parking while mutation depth is
  nonzero.
- The current direct fixture and registered CTest pass; their exact logs and
  the prior unreproduced timeout are recorded under Tooling evidence.

## Acceptance decision

The nested-mutation regression gate passes on the current native static-core,
Clang, and GCC builds, direct fixture, and bounded CTests. Overall SSA 06.02
acceptance remains pending while other leaf gates remain open. A GCC 14 direct
run is pending and is not claimed here. The earlier 41.91 second timeout
remains unexplained and unreproduced; the optional old-source D-only probe was
abandoned without executing an old-source test.
