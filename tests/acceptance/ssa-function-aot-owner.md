---
related_code:
  - tests/core/test_ssa_function_aot_owner.c
  - tests/core/support/ssa_function_aot_owner_fixture.h
  - tests/core/support/ssa_function_aot_owner_fixture.c
  - tests/parser/support/ssa_aot_owned_module_fault_shim.h
  - zr_vm_core/src/zr_vm_core/function_aot_ir.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/gc/gc_internal.h
  - tests/gc/gc_test_utils.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - tests/core/test_ssa_function_aot_owner.c
  - tests/core/support/ssa_function_aot_owner_fixture.h
  - tests/core/support/ssa_function_aot_owner_fixture.c
plan_sources:
  - .codex/plans/20261008-ssa-function-owner-tests-design.md
  - .codex/plans/20261008-ssa-function-owner-source-handoff-v2.md
tests:
  - docs/acceptance/ssa-function-aot-owner.md
doc_type: testing-guide
status: function-owner-functional-red-validated
---

# Actual Core Function AOT owner test guide

## Target and case coverage

Target: `zr_vm_ssa_function_aot_owner_test`. CTest: `ssa_function_aot_owner`.
Its default no-argument run must execute all twelve cases, with no selector or
skipped cases. Two failed attempts exposed a lower allocation-size failure.
After repair, actual r3 ran all twelve cases and established the required
three-pass, nine-UNSUPPORTED-failure functional RED.

| Case | Required observation |
| --- | --- |
| 1. Raw Core module prerequisite | ValidateModule and strict no-argument i64 ABI admission |
| 2. Independent Core owner prerequisite | Producer released, actual immutable view remains valid, one free |
| 3. Absent owner lifecycle | NULL getter, New absence, explicit Free and actual shutdown allocate no arena |
| 4. Snapshot after producer release | Independent constant/frame/map/state-map values remain readable |
| 5. Independent replacement | Nine-to-eight publication frees the old arena exactly once |
| 6. Exact self-source | Bounded semantic witness retained without reading freed addresses |
| 7. Invalid source and NULL | Actual diagnostics and prior owner remain correct; no allocation |
| 8. One-site OOM and retry | Old owner unchanged, whole OOM diagnostic, same measured retry bytes |
| 9. By-value move and donor detach | Donor becomes absent; destination alone owns and frees the arena |
| 10. Owned parent/child/grandchild | Three distinct arenas released by actual descendant cleanup |
| 11. Borrowed immediate shells | Only aliases cleared; original child/grandchild preserved; parent arena released |
| 12. Explicit Free then real collector shutdown | No second free for tombstone, still-attached Function released once |

The first two cases are support prerequisites. With safe Snapshot scaffolding,
case 3 also passes and nine attachment cases must genuinely fail at status 14.
Infrastructure or fixture failures cannot count as that RED. All twelve must
later pass for finite GREEN; see [acceptance](../../docs/acceptance/ssa-function-aot-owner.md).

The failed r1/r2 results each contained ten failures and zero ignores. Case 3
passed the zero-live-allocation check, then failed at 38 allocator errors.
The diagnostic revision prints only bounded values after actual shutdown:
all 380 records across ten shutdowns are fixed-capacity short-string allocation
size mismatches with matching OBJECT flags. No ledger checks were relaxed.
The unsigned instruction-mask complement also received a precise type fix;
the subsequent three-unit MSVC check produced zero warnings.

Actual r3 preserves those strict assertions: the absent-owner shutdown passed,
and no VM allocator or UBSan diagnostic appeared. Its receipt SHA256 is
`781081AB25A9F4123715043D2B23D22ED11B5067573CA70411949A1280E5B2B3`.
The common GC short-string size helper now matches fixed constructor capacity.
The existing synthetic GC STRING helper received the same minimum allocation
repair; this finite target does not run or claim the broad GC suite.

## Fixture and allocator boundary

The source is an actual heap-backed bounded Core metadata module, rather than a
Parser projection or decoded bytecode. Its two real source-map rows and actual
frame/value storage are validated before use; a present-empty logical map must
remain present after cloning. The tests use real GlobalState/State/Function and
collector shutdown. C-local records are protected through the saved stop-GC
flag. Typed inline child shells are not independently registered GC allocations;
only real donor records live on that allocation list.

The private owner OBJECT reuses the existing forced allocator shim; the test TU
opts out of macro redirection and supplies fixed-ledger wrapper functions.
VM bootstrap/New allocation is never deliberately failed. Wrappers keep no
dynamic bookkeeping and perform no Unity assertions. They clear a live address
before calling real free/realloc, restoring only a still-live address if realloc
fails. Subsequent tests use record IDs/counters and semantic values, never freed
pointer comparisons or assumed allocator reuse. Cleanup after failed assertions
uses registered live fixture resources; successful tests assert ledger state
before fallback cleanup could conceal a leak.

## Build and execution boundary

Both registrations explicitly use static Core and actual Unity. The direct
target additionally lists Common and Core vendors; no generic test harness,
Parser, Library, scalar emitter or runtime support is linked. A fresh map and
compilation/source metadata must verify the actual selection. The normal
registration is available when static Core is enabled; a configured entry alone
does not claim that route executed.

Use the existing approved build tree under `E:/cargo-targets/zr_vm/build`, and
logs/receipts under its reports root. Compile the real checkout directly. Scope
CTest to `^ssa_function_aot_owner$`; do not run broad test selectors that reach
source executors or generated native programs. Keep the four profiling inputs
zero, remove profile output/bootstrap trace for the child, and leave callbacks
for providers/loaders NULL. Do not fault VM allocation, install tools, copy
sources, invoke SourceExecute/Oracle or test network/FFI/security functionality.

Root records actual native PIDs, terminal exits, raw logs and source/artifact
pins. A failed or incomplete attempt is preserved rather than rewritten as
successful evidence. Exact finite commits use the requested He-Jiahui identity;
the full forty-seven SSA plans remain OPEN until their real gates are satisfied.
