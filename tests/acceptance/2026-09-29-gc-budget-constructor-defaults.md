# GC budget constructor defaults

Date: 2026-09-29

## Scope

`ZrCore_GarbageCollector_New` obtains its collector object from a non-zeroing
raw allocator. A new collector must not inherit budget configuration or
telemetry left in reused storage. This regression exercises the real global
state, main state, collector, and registry construction path. The production
initialization fix has passed the root-owned MSVC Debug build and direct test
run below.

## RED

Root-owned MSVC Debug evidence:

- Build command (PowerShell, repository root `E:/Git/zr_vm`):

  ```powershell
  cmd.exe /d /s /c 'call "E:\Visual Studio\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64 && set "TEMP=D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp" && set "TMP=D:\tmp\zr_vm\ssa-artifact-v6-msvc\compiler-tmp" && cmake --build D:\tmp\zr_vm\ssa-artifact-v6-msvc --target zr_vm_gc_concurrent_major_test -j 2'
  ```

- Build: 72/72 steps, exit 0.
- Direct command:

  ```powershell
  & 'D:\tmp\zr_vm\ssa-artifact-v6-msvc\bin\zr_vm_gc_concurrent_major_test.exe'
  ```

- Direct result: 11 tests, 1 failure, 0 ignored, exit 1. The existing 10 tests
  pass. `test_gc_budget_constructor_initializes_optional_state` fails at
  `tests/core/test_gc_budget_constructor_defaults.inc:202`: `GetBudget` was
  expected false but returned true.
- This executable has no CTest registration; the evidence is the direct run.

The test allocator delegates normal allocation/reallocation/free operations.
It seeds only the first allocation whose memory type is
`ZR_MEMORY_NATIVE_TYPE_MANAGER` and whose size equals
`sizeof(SZrGarbageCollector)`. It writes a valid `SZrGcBudget` initialized by
`ZrCore_GcBudget_Init`, plus legal typed enum/boolean values and distinct
nonzero telemetry. It records the injected address and calls the budget APIs
only after confirming that this exact pointer is the collector published by
the real global-state constructor. This avoids relying on random uninitialized
bytes or an invalid boolean representation. The fixture initializes the
registry, captures budget observations, frees the global state, and only then
runs Unity assertions so a RED assertion does not strand test allocations.

The test checks `GetBudget`, global `GetBudgetStats`, and state `GetStats` return
false before a budget is configured. It also checks default budget status,
phase, pause reason, counters, pressure/fallback flags, and matching snapshot
telemetry. It does not treat global GC allocation/debt counters as constructor
defaults, and the unconfigured `GetBudget` output value is outside the
contract.

## Repair

The collector constructor now initializes the budget record with
`ZrCore_GcBudget_Init`, sets `budgetConfigured` false, establishes
`ACCEPTED/IDLE/NONE`, zeros cursor/work/elapsed/debt/counters, and clears
pressure/fallback. It copies those values into the budget portion of the stats
snapshot. This is local field initialization; the constructor does not clear
the whole collector or change global factory OOM behavior.

## GREEN

Root reran the same build command against the repaired constructor on
2026-09-30 UTC. MSVC 19.44.35228.0, C11 Debug, completed 71/71 incremental build
steps with exit 0. The direct executable command above then completed with
exit 0: **11 tests, 0 failures, 0 ignored**, including the allocator-seeded
constructor regression and the 10 existing concurrent-major tests.

Independent read-only review found no blockers in initialization order,
snapshot consistency, typed allocator seeding, or cleanup before assertions.
This executable has no CTest registration; no CTest pass is claimed.

Build products and compiler temporary files are in the active reusable
`D:/tmp/zr_vm/ssa-artifact-v6-msvc` cache. This gate created no disposable
standalone probe directory. The cache remains in use for the next SSA gates;
there was no cross-volume move.

## Remaining coverage

- NDEBUG, Release, GCC, Clang, and sanitizer variants have not been run for this
  constructor-default change.
- This bounded constructor repair does not complete the larger 06.02 budget,
  slicing, pressure, and concurrent-GC acceptance matrix.
