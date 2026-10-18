---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_gc_map_internal.h
  - tests/parser/test_ssa_gc_map_source_preflight.c
  - tests/parser/support/ssa_gc_map_source_preflight_fixture.h
  - tests/parser/support/ssa_gc_map_source_preflight_fixture.c
  - tests/cmake/ssa-gc-map-source-preflight.cmake
  - tests/cmake/ssa-execir-owned-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_frame_validation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_finalized_module_validation.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_gc_map_internal.h
plan_sources:
  - .codex/plans/20261017-ssa-gc-map-source-preflight.md
tests:
  - tests/parser/test_ssa_gc_map_source_preflight.c
  - tests/parser/support/ssa_gc_map_source_preflight_fixture.h
  - tests/parser/support/ssa_gc_map_source_preflight_fixture.c
  - tests/cmake/ssa-gc-map-source-preflight.cmake
  - tests/cmake/ssa-execir-owned-direct-validation/CMakeLists.txt
doc_type: testing-guide
status: validation-accepted
---

# Source GC-map outer-header preflight

## Contract and implementation

The private primitive-frame storage validator and finalized-module storage
validator use `zr_exec_ir_gc_map_outer_shape_is_valid` before reading the GC-map
header or its inner pools. The canonical Core predicate admits exactly:

| Outer state | gcMap | gcMapCount | gcMapCapacity |
| --- | --- | ---: | ---: |
| Absent | NULL | 0 | 0 |
| Present | Readable owned header | 1 | 1 |

A present header may have empty or multiple inner pools. The outer-header rule
adds no restriction to valid inner entries, pool capacities or safepoint rows.
After existing owner/header prerequisites succeed, the guard rejects malformed
outer shapes with `ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE` before GC-map inner-header
access. The guard itself allocates nothing; prerequisite witness growth and OOM
priority retain their existing behavior. Frame storage retains its private error
channel; finalized and public wrappers preserve their nine-field diagnostic order.

The production changes are limited to the two private validators and their
canonical internal-header includes. The predicate itself, Core validation,
frame publication, Source execution and runtime GC behavior are unchanged.

## Actual-checkout fixture

`tests/parser/test_ssa_gc_map_source_preflight.c` and its private support fixture
exercise four actual paths: primitive storage, selected-function storage,
finalized-module storage and finalized candidate preflight. Each malformed case
uses real readable, aligned, ledger-owned headers. The fixture restores mutated
outer fields before assertions and owner cleanup.

| Group | Lifecycles | Purpose |
| --- | ---: | --- |
| Legal outer shapes | 24 | Absent, empty present and populated present |
| Malformed outer shapes | 54 | Count/capacity/null inconsistencies and multiple outer headers |
| Profile controls | 12 | Real Core prerequisites then the existing unsupported profile |
| Priority controls | 16 | Earlier owner/header failures retain first-error order |
| Healthy controls | 2 | Established healthy validation path |
| Grow/fault controls | 2 | Real 127-to-254 witness growth, allocation failure and retry |
| Total | 110 | Fresh owner and strict allocation ledger per lifecycle |

Storage, public-wrapper and profile observations are checked separately. Public
refusals initialize writable diagnostics to a nonzero pattern and verify all nine
fields; NULL-diagnostic calls verify the same validation and lifetime behavior.
Growth tests use 128 distinct real projection arrays, measure the actual
allocation site, preserve the existing owner on failure, release temporary
storage and retry after disarming the fault. Ledger-live and ledger-error counts
must both return to zero.

The selected direct CMake target is
`zr_vm_ssa_gc_map_source_preflight_test`; its CTest name is
`ssa_gc_map_source_preflight`. Its finite closure is 34 unique C translation
units. Production roles use the existing owned-module allocation shim; the
allocator implementation uses the ordinary CRT without redirecting itself.
The runtime configuration uses Clang UBSan with recovery disabled.

## Preserved reproduction and acceptance gates

The actual pre-fix run completed all 110 lifecycles: 63 passed and 47 failed,
with natural Unity exit 47. All 24 legal and 32 control cases passed. Seven
malformed cases were already rejected; 44 private-path malformed admissions and
three public diagnostic observations exposed the missing outer-header guard.
The allocation ledger ended with zero live allocations and zero errors, and both
fault/retry protocols completed. The raw failed run remains immutable evidence.

The repaired 34-unit build completed with zero cached-object credit. A second
link of the same objects with the sole `/verbose` observation flag identified
the physical archives actually read. This is 34 compile events and two link
events, rather than a second compilation. Physical archive reads, extracted
members and map-contributing archives are recorded separately.

The following checks completed against the actual checkout on 2026-10-17:

| Check | Actual result | Evidence scope |
| --- | --- | --- |
| Repaired focused run | 110 passed, zero failures or ignores | 24 legal, 54 malformed and 32 control lifecycles |
| Allocation ledger | 24,255 allocation attempts; zero live allocations and errors | Both measured fault/retry protocols completed |
| Existing primitive-frame regression | 18 passed, zero failures or ignores | Exact existing CTest target |
| Existing finalized-module regression | 24 passed, zero failures or ignores | Exact existing CTest target |
| Regression build | 13 compile events and two links | 24 reused production providers have current source, argv, dependency and object proofs |
| MSVC syntax | Four checks passed without warnings or errors | Four C files, three compile roles, 139 actual headers and 156 consumed inputs |

The regression build and test receipt contains 18 actual phases, including the
two CTest invocations. Its first controller stopped after a successful link
because the output parser did not recognize ordinary linker ICF messages. The
original output is retained; a finite parser correction accepted those messages
and the remaining link and tests completed without repeating the 13 compiles.

The first MSVC invocation returned zero without diagnostics, but its dependency
check found that `stdarg.h` was missing from the BEFORE input list. That attempt
receives no syntax acceptance credit. All four checks were rerun after fresh
input hashes were recorded; every actual dependency then matched BEFORE and
AFTER. These are syntax-only checks, not a full MSVC build.

Raw outputs, input hashes, actual commands and independent review receipts are
retained in the task report directory. Publication additionally checks the exact
ten owned paths, preserves foreign index entries and the foreign shared-CMake
working-tree prefix, and records the resulting commit. Owned temporary helpers
and Git metadata are cleaned after publication and review; reused build outputs
and required success and failure evidence are retained under the artifact ledger.

## Scope

This is metadata and ownership validation. It does not execute Source/VM code,
materialization, resume, Oracle, FFI or network/security paths. It supplies no
Linux, whole-project MSVC, backend-runtime or complete SSA-leaf acceptance.
Build products and temporary helpers remain in the configured physical
`E:/cargo-targets/zr_vm` roots; the actual checkout is compiled directly without
source snapshots.

- [Primitive-frame contract](../parser-and-semantics/primitive-frame-validation.md)
- [Finalized-module acceptance](../acceptance/source-finalized-module.md)
- [Related Parser consumers](ssa-gc-map-parser-consumers.md)
