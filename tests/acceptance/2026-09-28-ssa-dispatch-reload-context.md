---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c
tests:
  - tests/core/test_ssa_dispatch_boundaries.c
plan_sources:
  - docs/plans/ssa/index.md
  - docs/plans/ssa/03-interpreter-binding/01-dispatch-boundaries.md
doc_type: testing-guide
---

# SSA Dispatch Reload Context Acceptance

## Scope

This record closes one test coverage gap in plan 03.01. The new boundary test
publishes a context from one frame and stack, replaces the state roots with a
different call frame and stack backing buffer, then verifies reload follows the
current state, call-frame, global, and profile roots.

This is a test-only change. No production code changed. The 03.01 plan remains
open; in particular, the dispatch loop's bounded mutator poll has not yet been
rewired to use the publish/reload helpers.

## Baseline

Before this test, `test_publish_and_reload_rebuilds_frame_state` reloaded after
clearing the cached PC while keeping the same call frame and stack allocation.
It did not prove that context values from a previous frame were replaced after
the state roots changed.

The new test passed against the existing implementation, so this change closes
a coverage gap and does not claim a production fix. No failing pre-change run
was observed.

## Test Inventory

`test_reload_rebuilds_context_from_replaced_frame_roots` checks the resumed
callInfo, function, PC, instruction bounds, frame base and top, stack top, GC
domain, and profile runtime. It uses distinct initial and resumed stack arrays.
It also poisons the cached domain and profile values before reload to verify
they are overwritten from the current state roots.

## Tooling Evidence

Validation used WSL Linux, GCC Debug, with the existing build tree at
`/mnt/d/tmp/zr_vm/close-proxy-core-red`. Its CMake cache points to
`/mnt/e/Git/zr_vm`, uses `/usr/bin/gcc`, and has `CMAKE_BUILD_TYPE=Debug`.
The compiled target, build outputs, and CTest run logs stayed under the D:
cache (`Testing/Temporary` within that build tree).

Commands:

```bash
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_ssa_dispatch_boundaries_test -j4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_dispatch_boundaries_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -R '^ssa_dispatch_boundaries$' --output-on-failure --no-tests=error
```

## Results

- The focused target build exited 0.
- Direct Unity execution reported `4 Tests 0 Failures 0 Ignored`.
- Registered CTest `ssa_dispatch_boundaries` passed `1/1` with exit code 0.
- The new case passed without a production change.

## Acceptance Decision

Accepted as a focused 03.01 test coverage closure. The dispatcher boundary
rewiring and the other 03.01 plan tasks remain open; this record does not mark
the plan leaf or M3 complete.
