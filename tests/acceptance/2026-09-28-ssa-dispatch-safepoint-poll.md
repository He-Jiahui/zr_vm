# SSA 03.01 256-fetch safepoint integration

## Scope

- Routes the interpreter's existing 256-fetch mutator poll through
  `ZrCore_Execution_SafepointPoll`.
- Distinguishes a poll that continued without parking from a completed pause
  that reloaded the frame, and resumes through `LZrReturning` only after the
  latter.
- Covers the no-pause dispatch path, direct moved-root reload, and an actual
  full-GC pause that parks and resumes a running dispatcher. This is one 03.01
  slice; the plan remains open for other dispatch boundaries.

## Baseline

The added 257-NOP runtime test was run before the production change using the
existing GCC cache at `/mnt/d/tmp/zr_vm/close-proxy-core-red`. It reached the
existing poll with a saved resume PC offset of 255 while
`state->previousProgramCounter` remained 0. The direct Unity run reported 5
tests and 1 expected failure; registered CTest `ssa_dispatch_boundaries`
reported the same assertion failure.

## Test Inventory

- `tests/core/test_ssa_dispatch_boundaries.c` validates invalid publish/reload
  inputs, normal publish/reload, replacement of frame/instruction/stack/domain/
  profile roots, and a 257-NOP `ZrCore_Execute` run. The dispatch case checks
  that the active call frame's saved resume offset is nonzero and equals
  `state->previousProgramCounter`. Its full-GC integration case starts an
  observer mutator and a collector worker, observes the dispatcher parked via
  the public mutator snapshot, enters `MutatorPoll` during the pause, and
  verifies that it resumes after the full collection releases the pause and
  the dispatcher returns after cancellation.
- `tests/core/test_execution_dispatch_callable_metadata.c` is an adjacent
  runtime dispatch regression suite.
- The direct moved-root case still calls `ReloadBoundary`; the full-GC case
  separately proves a real collector pause and dispatcher resume.

## Tooling Evidence

All build artifacts were written to the existing D-drive cache. The cache was
configured for GCC 11.4 and Ninja; no other cache was created.

```bash
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_dispatch_boundaries_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_dispatch_boundaries_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^ssa_dispatch_boundaries$' --output-on-failure
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_execution_dispatch_callable_metadata_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_execution_dispatch_callable_metadata_test
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_gc_domain_multimutator_test zr_vm_gc_concurrent_major_test -j 4
timeout 30s /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_gc_domain_multimutator_test
timeout 30s /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_gc_concurrent_major_test
```

## Results

- Before implementation, the new assertion failed with `resume offset=255` and
  `previous PC=0`; the four existing boundary tests passed.
- The first full-GC fixture build identified a missing typedef alias on its
  worker context declaration. Adding `} ZrDispatchPauseWorkerContext;` fixed
  the source; the focused GCC target then built and linked successfully.
- After implementation and the full-GC case, the dispatch boundary Unity
  executable passed all 6 tests, and registered CTest
  `ssa_dispatch_boundaries` passed 1/1 (0.10 seconds).
- The adjacent callable-metadata runtime executable passed all 18 tests.
- The adjacent `gc_domain_multimutator` executable passed all 11 tests, and
  `gc_concurrent_major` passed all 10 tests.
- The adjacent target has no matching CTest registration in this cache; direct
  execution of its built Unity binary was used instead.
- GCC emitted the dispatch file's existing computed-goto pedantic and unused
  label warnings; the target linked successfully.

## Acceptance Decision

Accepted for the 256-fetch poll integration slice. A successful pause returns
`ZR_EXECUTION_BOUNDARY_RELOADED`, refreshes the local call-info pointer from the
reloaded context, and re-enters the existing local-state reload path. Boundary
errors are sent to the VM runtime-error path with their status name. The test
proves the no-pause path, direct root reload behavior, and actual full-GC
parking/resume at the 256-fetch dispatch poll. Other call, native, debug, and
suspend boundaries remain open in 03.01.
