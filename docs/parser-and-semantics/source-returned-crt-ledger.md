---
related_code:
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
  - tests/parser/support/ssa_source_returned_retention_fixture.h
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - tests/parser/test_ssa_source_returned_crt_ledger.c
  - tests/cmake/ssa-source-returned-crt-ledger.cmake
  - tests/cmake/ssa-source-returned-crt-ledger-direct-validation/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/aot_ir.h
implementation_files:
  - tests/parser/support/ssa_source_returned_crt_ledger.h
  - tests/parser/support/ssa_source_returned_crt_ledger.c
plan_sources:
  - 'Root: 2026-10-10 finite Returned CRT ledger extraction'
  - .codex/plans/20261010-ssa-returned-crt-ledger-extraction.md
tests:
  - tests/parser/test_ssa_source_returned_crt_ledger.c
  - docs/testing-and-validation/source-returned-crt-ledger.md
  - docs/acceptance/source-returned-crt-ledger.md
doc_type: module-detail
status: finite-green
---

# Returned Source CRT ledger extraction

## Purpose and current state

The extracted test-support ledger owns the existing two CRT inventories and allocator callbacks formerly held inside the Returned Source lifecycle fixture. The lifecycle coordinator delegates to this one authoritative ledger. There is no second shadow allocator, fabricated provider, source snapshot or new production allocation service.

Root accepted the finite Windows clang-cl UBSan RED/GREEN sequence and a separate five-unit MSVC compile-only retry on 2026-10-10. The initially always-refusing `ZrTests_Returned_TryStats` produced exactly sixteen public-feature failures and two genuine guard passes; its guard/copy implementation then passed all eighteen cases. Other observations and real allocation callbacks retained their behavior. The [testing guide](../testing-and-validation/source-returned-crt-ledger.md) preserves the exact case order; the [acceptance record](../acceptance/source-returned-crt-ledger.md) records actual receipts and the prior MSVC failure.

## Inventories and geometry

Kinds are `ZR_RETURNED_DESCRIPTOR = 0` and `ZR_RETURNED_OWNER = 1`. Each ledger has `ZR_RETURNED_CRT_ROWS = 256` event and allocation slots. The six named counters are `attempts`, `successes`, `frees`, `live`, `errors` and `failures`. These are observed fields, not a claim about any struct byte size.

Events contain `size_t count`, `width`, `bytes`, then `unsigned site`, `record` and the actual common `TZrBool injected`. `record` is an allocation inventory index on success; an injected attempt has the 256 sentinel and no successful allocation. Sites are local to the current call base and start at one.

Descriptor allocations use actual CRT `calloc` and the following ordered geometry; widths are actual typed `sizeof` expressions, never guessed ABI numbers:

| Local site | Count | Width |
|---|---:|---|
| 1 | 3 | `sizeof(SZrAotIrInstruction)` |
| 2 | 1 | `sizeof(SZrAotIrBlock)` |
| 3 | 1 | `sizeof(SZrAotIrFrameSlot)` |
| 4 | 3 | `sizeof(SZrAotIrSourceMap)` |
| 5 | 1 | `sizeof(SZrExecIrConstant)` |
| 6 | 1 | `sizeof(SZrExecIrLayout)` |

Owner requests use real CRT `malloc`, with count one and the requested byte width. Free only releases an exact current tracked pointer in its own kind. Null free is a no-op. A genuine readable foreign pointer is refused with an error and is never passed to CRT free.

Zero count/width, multiplication overflow, incorrect descriptor site/shape and the actual attempt limit are refused with an error before any event or attempt increment. An injected otherwise-valid request creates its actual event and increments attempts/failures without incrementing successes/live. Successful records remain inventoried after free so record-free counts and historical events remain readable until explicit reset.

## Public read contracts

The four integer-returning queries are:

```c
int ZrTests_Returned_TryStats(unsigned kind, SZrReturnedHeapStats *output);
int ZrTests_Returned_TryRecordFrees(unsigned kind, unsigned record, unsigned *output);
int ZrTests_ReturnedCrt_TryEvent(unsigned kind, unsigned index, SZrReturnedHeapEvent *output);
int ZrTests_ReturnedCrt_TryFindLiveRecord(unsigned kind, const void *pointer, unsigned *output);
```

Callers serialize reads and allocation activity. Outputs are genuine independent writable storage; refusal preserves every existing named field. Invalid kinds, null outputs and out-of-inventory records/events refuse. Exact current-live lookup also refuses null, interior, one-past, foreign, freed and other-kind pointers. It does not dereference candidate pointers or certify numeric containment, readable initialized bytes, typed ownership or historical identity after address reuse.

The queries are nonasserting, allocate and free nothing, mutate no counters/events/records, and consume no pending fault. `TryStats` refuses a null output or kind at least two, otherwise copies that kind's six named counters and returns one. The finite tests accepted this public behavior; GREEN changed only this five-line guard/copy function. The internal `ZrTests_ReturnedCrt_Stats` retains the old direct counter copy for the coordinator and requires a literal or validated kind below two. Its use for test health/purity supplies no public `TryStats` feature credit.

## Coordination and lifetime

`ZrTests_ReturnedCrt_BeginCall(descriptorSite, ownerFault)` requires descriptorSite in 0..6 and rebases both local site inventories to current attempts. Nonzero descriptor site schedules its future attempt; a true owner flag schedules the next owner attempt. An unarmed argument does not clear a previously scheduled fault. The coordinator retains its pending scheduling state separately and calls the extracted ledger at its existing boundary.

`Disarm` explicitly clears both scheduled faults. `Recover` records one error per kind with live allocations, frees all current live rows and preserves counters/events; it neither resets observations nor implicitly disarms faults. Repeated recovery with no live allocations changes nothing. `Reset` requires both ledgers already have zero live allocations and is performed before a test acquires resources. Neither precondition is tested through deliberately invalid calls.

Borrowed pointers end their live identity at free; tests save numeric witnesses before that point and never dereference reconstructed freed addresses. Callbacks and read APIs make no general readability promise.

## Finite link boundary and OPEN work

The accepted executable selects only the new test main, actual extracted ledger C, and actual Unity C. Actual common and Core schema headers supply types; no Core/Parser/VM archive or lifecycle fixture is linked. The actual route used C11, assertions, warning flags, dynamic MSVC CRT and Clang 19.1.5 UBSan without recovery. Configuration used the actual E checkout and separate E build child, with source/settings metadata, compile commands, latest dependencies and a link map naming the sole extracted ledger provider. RED compiled all three C units; GREEN recompiled only the ledger and linked, so it supplies incremental finite runtime evidence.

The separate MSVC retry compiled the test, ledger, Unity, changed lifecycle coordinator and typed header probe with zero warnings and no forced include. It supplies declaration/compiler compatibility only; the coordinator and probe were not executed. Root tracks shared registration and Git/normal-index transactions separately; these receipts cover finite runtime and compiler gates only. The extraction does not establish returned Source ownership, compiler/Binder provenance, Function/GC lifecycle, VM execution, post-Attach/Verify finalizer handoff, normal Source/full-project gates, Linux, full MSVC runtime, 32-bit parity or all47. These remain OPEN until separately exercised and accepted.
