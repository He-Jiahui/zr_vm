---
related_code:
  - zr_vm_core/include/zr_vm_core/contiguous_view.h
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
plan_sources:
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
tests:
  - tests/core/test_ssa_arrays_slices.c
doc_type: acceptance-record
status: partial
---

# SSA 05.02: contiguous view boundary validation

## Scope and baseline

This slice changes the core metadata view helpers only. A zero-length child
slice at a representable tail byte offset must remain a valid descriptor.
Window bounds, arithmetic overflow, and missing output pointers must report
their distinct diagnostic categories, and failure must preserve the output.
The parser index-lowering wrapper delegates directly to the core helper.

Before the change, `ZrCore_View_Validate` added `elementSize` even when
`length == 0`; `ZrCore_View_Slice` could publish a child that then failed
validation. Slice arithmetic overflow was reported as `BOUNDS`, while a
nonnegative index equal to `length` and a missing index output pointer were
reported as `OVERFLOW`. The focused test previously covered a normal nonempty
slice, a negative index, one out-of-window slice, and an invalid descriptor,
but did not distinguish these cases. The 05.02 requirement row remains open.

The repository's bundled reference implementations support the same basic
empty-tail rule: Rust's `split_at(len)` example returns an empty right slice
(`lua/rust/library/core/src/slice/mod.rs:1940`), and .NET's `Span<T>.Slice`
tests accept `Slice(array.Length, 0)` while rejecting `Slice(array.Length, 1)`
(`lua/runtime/src/libraries/System.Memory/tests/Span/Slice.cs:52`, `:67`).
This work keeps the already planned zr semantics and does not adopt either
runtime's ownership or exception model.

## Test inventory and RED evidence

The updated `tests/core/test_ssa_arrays_slices.c` checks:

- a normal tail slice `(start=length, length=0)` remains valid and rejects
  index zero as `BOUNDS`;
- a one-element parent ending at `SIZE_MAX` creates a zero-length child at
  offset `SIZE_MAX`, and that child passes `Validate` with its generation;
- descriptor creation reports `OVERFLOW` when either stride multiplication or
  the final element extent exceeds `SIZE_MAX`, preserves the caller's prior
  descriptor on failure, and continues to report zero stride as `INVALID`;
- multiplication and addition overflow at a legal tail start report
  `OVERFLOW` and preserve the prior child descriptor;
- a positive index equal to `length` reports `BOUNDS` without changing the
  output offset; missing slice or index outputs report `INVALID`;
- the earlier normal slice, negative index, out-of-window slice, and invalid
  parent descriptor cases still run.

No valid descriptor can exercise `IndexOffset` arithmetic overflow: the
descriptor validator checks the byte extent through the last element, and an
in-range index cannot exceed that extent. Its overflow return remains a
defensive classification; the reachable regression checks distinguish
`index == length` as `BOUNDS` from a missing output pointer as `INVALID`.

GCC directly compiled the three registered target sources into
`/mnt/d/tmp/zr_vm/ssa-view-boundaries`. Each new defect assertion was run
against the pre-fix implementation before its fix: the empty tail failed at
`test_empty_tail_slice_remains_valid`, multiplication overflow at
`test_tail_slice_arithmetic_overflow_is_reported`, positive out-of-range index
at `test_index_past_end_reports_bounds`, and missing index output at
`test_index_requires_output_pointer`. The missing slice output and addition
overflow assertions were separately run against the `HEAD` version of the
core source, failing at `test_slice_requires_output_pointer` and
`test_tail_slice_offset_add_overflow_is_reported`. All were assertion failures
on the expected diagnostic or postcondition, not compilation failures.

## Tooling evidence and results

WSL GCC 11.4 and Clang 14 directly compiled the target's three C sources with
`-std=c11 -g -O0 -Wall -Wextra`; both current binaries exited zero without
compiler warnings. The source list was:

```text
tests/core/test_ssa_arrays_slices.c
zr_vm_core/src/zr_vm_core/object/contiguous_view.c
zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
```

Both drivers wrote executables under `/mnt/d/tmp/zr_vm/ssa-view-boundaries`.
MSVC 19.44.35228 used the repository's VsDevCmd wrapper and compiled the same
three sources with `/utf-8 /std:c11 /W3 /Zi /Od`; its executable also exited
zero. An initial manual `cl` attempt without `/utf-8 /std:c11` failed while
parsing Unicode/C11 headers. The corrected invocation passed, and the failed
attempt changed no source files.

The registered GCC Debug gate used these commands and directory:

```bash
cmake -S /mnt/e/Git/zr_vm -B /mnt/d/tmp/zr_vm/ssa-view-gcc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build /mnt/d/tmp/zr_vm/ssa-view-gcc --target zr_vm_ssa_arrays_slices_test -j 4
ctest --test-dir /mnt/d/tmp/zr_vm/ssa-view-gcc -R '^ssa_arrays_slices$' --output-on-failure --no-tests=error
```

Configuration succeeded, Ninja built 4/4 steps, and CTest passed 1/1. After
adding the last two boundary assertions, a fresh incremental build rebuilt
the focused test object and executable (Ninja 2/2); its CTest passed 1/1
again. Direct GCC and Clang binaries were also rebuilt from the final test
source and exited zero without warnings. The MSVC binary was built after the
last test-source edit and exited zero on a final rerun. The 05.02 plan names
the registered GCC `ssa_arrays_slices` test as this slice's focused gate;
the direct Clang and MSVC builds provide additional compiler coverage.

## Follow-up: descriptor extent overflow diagnostics

This follow-up separates arithmetic extent failures from malformed descriptor
fields. Before the implementation change, `ZrCore_View_Validate` folded checked
multiply/add failures into `ZR_VIEW_DIAGNOSTIC_INVALID`. The updated test first
uses a valid descriptor as the output sentinel, then checks both a final-element
addition overflow (`length=2`, `stride=SIZE_MAX`) and a stride multiplication
overflow (`length=3`, `stride=SIZE_MAX/2+1`, `elementSize=1`). Both must report
`ZR_VIEW_DIAGNOSTIC_OVERFLOW` without replacing the sentinel. A zero stride
control remains `ZR_VIEW_DIAGNOSTIC_INVALID`.

The current-source RED build succeeded (Ninja 4/4). Direct execution then exited
134 at the addition-overflow assertion in
`test_basic_view_boundaries` (line 128): the expected `OVERFLOW` diagnostic was
actually `INVALID`. Registered CTest `ssa_arrays_slices` also failed 1/1 with
exit 8 at that same assertion. The process aborts at its first failed `assert`,
so the multiplication and zero-stride assertions were not independently
observed failing in that RED run.

The implementation now keeps descriptor shape checks classified as `INVALID`
and reports checked extent multiply/add failure as `OVERFLOW`. `Create` still
validates a local candidate before publishing it, preserving the prior output
on failure. Using WSL GCC 11.4.0, CMake 3.22.1, and the existing D-only Ninja
cache `/mnt/d/tmp/zr_vm/close-proxy-core-red`, the incremental target build
completed 2/2 steps. The direct Unity binary exited 0, and the registered
`ssa_arrays_slices` CTest passed 1/1 (exit 0). No build products were written
outside the D cache.

## Remaining boundary

These helpers validate metadata only. They do not compare the descriptor with
real array storage, root an owner across GC, validate a current generation in
`Slice` or `IndexOffset`, enforce a lifetime loan, exclude resize while a view
is live, validate an actual pin, or remove an ExecIR guard. The existing
`zr.container` Span runtime and FFI pinned views remain separate adapters;
this acceptance slice does not close the full 05.02 lifecycle, resize, or pin
gates and does not claim production backend differential behavior.

## Acceptance decision

Accepted for the core metadata view boundary subtask: all focused assertions
pass in the registered GCC test and direct GCC, Clang, and MSVC builds. The
full 05.02 milestone remains open for the integration gates above.
