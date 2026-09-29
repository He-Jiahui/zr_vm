# SSA 03.01 256-fetch safepoint integration

## Scope

- Routes the interpreter's existing 256-fetch mutator poll through
  `ZrCore_Execution_SafepointPoll`.
- Distinguishes a poll that continued without parking from a completed pause
  that reloaded the frame, and resumes through `LZrReturning` only after the
  latter.
- Covers the no-pause dispatch path and retains direct moved-root reload
  coverage. This is one 03.01 slice; the plan remains open for other dispatch
  boundaries and a concurrent parked-dispatch integration test.

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
  `state->previousProgramCounter`.
- `tests/core/test_execution_dispatch_callable_metadata.c` is an adjacent
  runtime dispatch regression suite.
- A real concurrent collector parking the dispatcher is not exercised in this
  slice. The moved-root test calls `ReloadBoundary` directly; it does not claim
  end-to-end pause/reload coverage.

## Tooling Evidence

All build artifacts were written to the existing D-drive cache. The cache was
configured for GCC 11.4 and Ninja; no other cache was created.

```bash
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_dispatch_boundaries_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_dispatch_boundaries_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^ssa_dispatch_boundaries$' --output-on-failure --no-tests=error
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_execution_dispatch_callable_metadata_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_execution_dispatch_callable_metadata_test
```

## Results

- Before implementation, the new assertion failed with `resume offset=255` and
  `previous PC=0`; the four existing boundary tests passed.
- After implementation, the dispatch boundary Unity executable passed all 5
  tests, and registered CTest `ssa_dispatch_boundaries` passed 1/1.
- The adjacent callable-metadata runtime executable passed all 18 tests.
- The adjacent target has no matching CTest registration in this cache; direct
  execution of its built Unity binary was used instead.
- GCC emitted the dispatch file's existing computed-goto pedantic and unused
  label warnings; the target linked successfully.

## Acceptance Decision

Accepted for the 256-fetch poll integration slice. A successful pause returns
`ZR_EXECUTION_BOUNDARY_RELOADED`, refreshes the local call-info pointer from the
reloaded context, and re-enters the existing local-state reload path. Boundary
errors are sent to the VM runtime-error path with their status name. The test
proves the no-pause publish path and direct root reload behavior; actual
concurrent dispatcher parking and the other call, native, debug, and suspend
boundaries remain open in 03.01.
