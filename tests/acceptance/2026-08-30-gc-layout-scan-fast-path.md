# GC layout scan fast path

Timestamp: 2026-08-30 06:20:00 +08:00

Plan slice: benchmark optimization / memory-object GC, Task 5

## Scope

- Prove that a validated plain structure layout can skip inline-array GC value
  visits without changing the conservative behavior for managed value layouts.
- Reject a stale layout hash before it can be considered for the fast path.
- Keep descriptor resolution and all existing write-barrier/drop paths intact.

## RED

- Before `ZrCore_TypeLayout_CanSkipGcScan` existed, the focused test failed to
  compile with an implicit declaration for the new descriptor contract.

## GREEN

- `test_inline_struct_array_skips_verified_non_gc_layout` now proves:
  - a plain `STRUCT` with no GC/ownership/ref/nested fields qualifies;
  - `SZrTypeValue` (`VALUE` layout) does not qualify;
  - a layout with a modified hash does not qualify;
  - a nested-layout field does not qualify even without direct map entries;
  - corrupting the recorded length fails the range check before the fast path;
  - a 1024-element inline array takes the visitor path successfully with zero
    callback invocations.
- The existing managed inline-struct test continues to visit both values and
  to drop them in reverse order.

## Verification

- WSL GCC strict syntax: `test_inline_struct_array_layout.c` passed.
- WSL GCC strict syntax: `type_layout.c` passed.
- WSL GCC strict syntax: `object_inline_array.c` passed.
- WSL Clang strict syntax: `type_layout.c` passed.
- WSL Clang strict syntax: `object_inline_array.c` passed.
- MSVC Debug focused executable rebuilt and passes 3 tests, 0 failures,
  including the nested-layout and corrupt-length assertions.
- A fresh ext4 Clang 14 ASan Debug build passes the focused executable 3/3
  with `detect_leaks=1`, `halt_on_error=1`, and `abort_on_error=1`.
- The same sanitizer build passes the complete standalone GC binary 67/67.
  The run exposed and fixed a shutdown leak: `RELEASED` means finalization has
  completed, so sweep must avoid repeating the finalizer but must still perform
  region cleanup, type deconstruction, and raw allocation release.

No benchmark delta, pause-percentile claim, or focused Valgrind result is
asserted here; those remain deferred to the corresponding performance and
memory gates.
