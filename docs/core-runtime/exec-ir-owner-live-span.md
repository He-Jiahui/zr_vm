---
related_code:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
  - tests/core/support/ssa_exec_ir_owned_module_shim.h
  - tests/core/test_ssa_exec_ir_owner_live_span.c
  - tests/unity_config.h
  - tests/cmake/ssa-owner-live-span.cmake
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-owner-live-span-direct-validation/CMakeLists.txt
implementation_files:
  - tests/core/support/ssa_exec_ir_owned_module_allocator.h
  - tests/core/support/ssa_exec_ir_owned_module_allocator.c
plan_sources:
  - 'Root: 2026-10-09 finite test-ledger live-span query contract'
  - .codex/plans/20261009-ssa-exec-owner-live-span-protocol-v1.md
tests:
  - tests/core/test_ssa_exec_ir_owner_live_span.c
  - docs/testing-and-validation/exec-ir-owner-live-span.md
  - docs/acceptance/exec-ir-owner-live-span.md
doc_type: module-detail
status: finite-green
---

# ExecIR test-ledger live-span query

## Current implementation and purpose

The additive `ZrTests_ExecOwner_TryLiveSpan` API belongs to the strict test allocator, not to the Core execution API. It is intended to let an observer check that a numeric byte interval lies inside exactly one current live tracked allocation before reading separate, already validated typed data. The allocator continues to allocate through the real CRT heap. This query creates no owner, authenticates no type, retains no registry, and admits no module for execution.

The current implementation checks the numeric query geometry, searches all current live records for unique containment, then checks the requested record window and publishes one temporary typed result. The dedicated Clang UBSan runtime gate is accepted at GREEN r1 with twelve PASS results, following the actual functional RED r3. A separate four-unit MSVC compile-only check is accepted. These are finite test-support results; no complete allocator-failure sweep or commit is claimed here.

## API and output storage

```c
typedef struct SZrOwnerLiveSpan {
    unsigned record;
    const void *allocation;
    size_t bytes;
} SZrOwnerLiveSpan;
int ZrTests_ExecOwner_TryLiveSpan(unsigned first, unsigned last,
                               const void *pointer, size_t count, size_t width,
                               SZrOwnerLiveSpan *output);
```

`record` is the allocator record index. `allocation` is its current allocation base, borrowed while that allocation remains live. `bytes` is the current recorded allocation size, including any storage beyond the queried subspan. There is no fixed public struct byte-size assumption; its layout is the actual C typed layout and `sizeof(SZrOwnerLiveSpan)`.

A nonnull output must be genuine writable independent storage. Callers serialize query and allocator activity and keep that output separate from ledger internals and live input storage. The implementation is not responsible for probing arbitrary output pointers. A null output is a refusing input. On refusal, all existing output fields remain unchanged. No Core nine-field diagnostic is part of this primitive integer-returning API.

## Containment rules

The half-open record window `[first,last)` must be nonempty, ordered, and inside the currently recorded inventory and fixed ledger capacity. The current allocator has 32,768 record slots. This capacity is a bookkeeping limit, not a span length or an ownership guarantee.

The query requires a nonnull pointer, nonzero count, and nonzero width. Multiplication must be representable in `size_t`, and the resulting byte count must round-trip through `uintptr_t`. The pointer must round-trip through its numeric address representation, and the interval end must not wrap. These checks apply before any containment result is published.

Uniqueness is determined among all current live records, then the matching record must fall inside the requested window. This avoids treating an omitted or overlapping live record as irrelevant merely because its index lies outside the window. Ambiguous containment refuses. A requested interval may start at the allocation base or a valid interior byte and may end exactly at the allocation end. A nonempty query beginning one past that end refuses.

Containment uses current allocation bytes, not a historical realloc size, logical element count, or separately assumed pool capacity. Realloc success preserves the record identity but replaces its current pointer and byte size. Realloc failure leaves that record unchanged under the existing allocator contract. Released records have null current pointers and cannot witness a live span.

Every nonnull current row encountered in the global scan must have zero recorded frees, nonzero bytes, representable byte-count conversion, a pointer that round-trips through `uintptr_t`, and a nonwrapping allocation end. A malformed current live row refuses the entire query, even outside the requested window. The implementation contains these guards; the finite real-heap tests do not construct malformed rows or duplicate live containment. Only after all rows and the selected window satisfy the checks is the temporary result assigned to the caller's output.

## State and lifetime boundaries

The query is nonasserting and read-only: no CRT allocation, free, fault consumption, event append, counter update, reset, callback, or Unity assertion. It leaves attempts, records, live, frees, errors, failures, existing events, and a pending armed fault unchanged. Queries do not dereference the candidate pointer or allocation body. The finite fixture checks all six statistics around each query and separately observes one armed third allocation attempt and its actual events.

Success borrows the returned base; it does not extend its lifetime. Callers must save numeric witnesses while the allocation is live and avoid reading an obsolete pointer value after free or successful realloc. An address reconstructed from a saved integer may be rejected numerically without being dereferenced; this does not establish pointer readability or portable typed access.

A caller that later reads owner data must separately prove initialization, element geometry, readable extent, relevant tagged-union state, and that the same allocation remains live throughout the read. Live-span containment alone proves none of those facts. A reused numeric address does not identify a historical allocation.

## Finite build boundary

The dedicated executable has exactly three selected repository translation units: the live-span test main, the actual strict allocator C, and actual Unity C. The observed compile graph has no forced include or linked Core/Parser/VM archive. The allocator includes its own shim with `ZR_TEST_EXEC_IR_OWNER_NO_REDIRECT`, preserving real CRT allocation; actual symbols and the link map identify the allocator object as the query provider.

The standalone project uses the actual `E:/Git/zr_vm` checkout and the separate build child `E:/cargo-targets/zr_vm/build/ssa-20261009-01a0fe2b-owner-live-span`. No source snapshots were created. Actual settings select Clang 19.1.5 `clang-cl`, Ninja Debug, C11 without extensions, `/W4 /UNDEBUG`, dynamic CRT, and UBSan with recovery disabled. GREEN r1 recompiled main and allocator and linked in three build steps; Unity's existing object was reused. This is an incremental accepted build, not a clean full rebuild.

MSVC GREEN r1 separately compiled the actual main, allocator, Unity, and a fresh tiny typed declaration probe with `/TC /std:c11 /W4 /UNDEBUG /Od /MD /utf-8 /c`, no forced include, four natural exits 0 and zero warnings. It supplies compilation and actual dependency evidence only; no MSVC executable was linked or run in that check. Root added the module include to `tests/cmake/ssa-tests.cmake`; normal full-project configuration/runtime remains unexercised by this finite child.

## Related validation and limits

The [testing guide](../testing-and-validation/exec-ir-owner-live-span.md) records the exact twelve finite real-heap cases and observed RED/GREEN matrix. The [acceptance record](../acceptance/exec-ir-owner-live-span.md) records accepted receipts, preserved rejected attempts, current input pins, and compile-only boundaries.

Duplicate live containment cannot be manufactured through normal unique CRT allocations; its implemented refusal and malformed-row guards still need separate experimental evidence. Dedicated 32-bit behavior, failed-realloc preservation around this query, broader capacity saturation, complete OOM/retry coverage, Source/Binder provenance, canonical validation, registry lifetime, default adoption, Linux, full MSVC runtime, and all47 acceptance remain OPEN. Existing owner/module/finalizer successes do not supply new live-span runtime credit.
