---
related_code:
  - tests/parser/test_ssa_source_scalar_constant.c
  - tests/parser/support/ssa_source_scalar_constant_noalloc_shim.h
  - tests/parser/support/ssa_literal_script_fixture.h
  - tests/parser/support/ssa_literal_script_fixture.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - tests/parser/test_ssa_source_scalar_constant.c
  - tests/parser/support/ssa_source_scalar_constant_noalloc_shim.h
plan_sources:
  - .codex/plans/20261008-ssa-source-scalar-constant-source-tests-design.md
tests:
  - tests/parser/test_ssa_source_scalar_constant.c
doc_type: test-acceptance-record
status: source-scalar-constant-functional-red-established
---

# Actual source scalar constant tests

## Inventory and observations

Exactly 24 `RUN_TEST` calls execute in the default binary:

| Cases | Actual checks |
| --- | --- |
| 1-2 | Independently constructed source9/source8 lower-pipeline prerequisites |
| 3-4 | Actual typed pool-derived nine/eight bridge extraction |
| 5 | Two simultaneously live sources and independent values after source free |
| 6-7 | NULL arguments/output preservation and optional diagnostic |
| 8-10 | Certification/termination, SCRIPT eligibility, declared callable effect |
| 11-15 | Pool-index presence/bounds, counts, element width and capacity |
| 16-18 | Compiler/runtime tags and actual I64 disagreement |
| 19-22 | Canonical source kind, result type and bounded RETURN operand |
| 23 | Actual finalized signature witness disagreement |
| 24 | Fresh Build with only the production bridge constant, binder/verification/free |

Case9 mutates currentAst and compiler.hasError. The independent second-State
variant is outside this finite profile and is not credited as executed coverage.

Expected successful bits come from both real INT64-tagged compiler/runtime rows;
typeToken comes from the actual canonical CONSTANT. Fixed source9/source8 values
are prerequisite semantic checks, not an extraction implementation oracle.
Whole diagnostics are poisoned before calls, then all nine fields are checked.
The output is poisoned and preserved byte-for-byte on refusal.

## Allocation, mutation and cleanup

The bridge's actual C translation unit alone is force-included with the shim.
Its four CRT entry wrappers record calls without allocating. The real Global
uses a separate bounded strict allocator ledger with actual size/type checks,
and allocation/free counts are compared across the bridge call. Read-only helper
closure review is separate evidence; other Core CRT allocations are not claimed
as instrumented by this shim.

Faults modify bounded live scalar/header fields. While-call digests and typed
rows are captured, then the original facts restore before every assertion.
No canonical interned node is overwritten. The test registers partial fixtures
before preparation and initializes the fresh module before case execution.
Teardown frees module/fixtures, restores saved stopGc and frees actual Global;
only independently saved numeric diagnostics are read after owner destruction.
The allocator caches whether the request is new before calling realloc.

Fresh Build and binder allocations occur outside bridge observations. Case24
checks module zeroing and VM live count restoration; this is not a separate
ledger of all Core CRT allocations. Oracle support may be linked by the existing
fixture, but no Oracle/source execution occurs.

## Current actual result

Source is frozen: C535 lines, shim19; actual SHA256 values and the finite driver
scope are recorded in [acceptance](../../docs/acceptance/ssa-source-scalar-constant.md).
Current actual r3 runs all24 cases: source9/source8 prerequisite and NULL
argument cases PASS,21 feature FAIL,0 ignored. Feature case3 actually reports
UNSUPPORTED28 from the linked stub after immutable input/output and zero
allocation observations. No VM ledger or UBSan error occurs. The finite
module/test/fixture MSVC compilation has0 exits/0 warnings. GREEN remains pending.

R1's lower128-versus16 byte metadata cleanup failure is preserved as rejected
evidence; its independent6-case repair is committed. Saved GC-state restoration
is guarded by both an initialized valid flag and a live collector. This test
uses normally returned actual Global owners; it does not probe partial bootstrap.

The normal registration links Parser/Core/Library explicitly; it avoids the
broader helper that includes FFI. The direct target records the actual source
graph and checks final map selection. Both use assertions, zero profiling,
unset output/trace and bounded CTest timeout. Linux, full MSVC, returned-source
retention and all47 SSA leaves remain OPEN.
