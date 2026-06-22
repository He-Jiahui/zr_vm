---
doc_type: acceptance-record
phase: 6.4
title: heap summary and CLI heap-summary switch
status: accepted
updated_at: 2026-06-22 07:08:48 +08:00
---

# Debug Phase 6.4 Heap Summary

## Scope

- Expose a lightweight core `ZrCore_Debug_HeapSummary(state, FILE*)` report for heap and GC diagnostics.
- Count active objects by raw object type, track base object bytes, and include prototype and GC counter sections.
- Add CLI `--heap-summary[=out]` for normal project runs and `--compile ... --run`.
- Keep the report read-only and non-invasive; this is not a full heap graph analyzer.

## Baseline

| Check | Initial result |
|------|----------------|
| `tests/debug/test_heap_summary.c` | Windows/MSVC build failed with `unresolved external symbol ZrCore_Debug_HeapSummary` |
| CLI args heap-summary parse test | Failed because `SZrCliCommand` did not expose `heapSummaryEnabled` / `heapSummaryOutputPath` |
| CLI import fixture heap report | Failed before runtime wrote the requested heap summary file |

The worktree already contained unrelated parser/AOT/runtime changes. Normal WSL multi-target builds attempted during validation were pulled into broad rebuilds and timed out; focused object rebuilds, direct link scripts, and registered CTest runs were used to validate the touched heap-summary paths.

## Implemented

- Added `zr_vm_core/src/zr_vm_core/debug_heap.c` and kept the public declaration in `zr_vm_core/include/zr_vm_core/debug.h`.
- `ZrCore_Debug_HeapSummary` emits `ZR_HEAP_SUMMARY objects`, per-type `type ... count ... bytes ...` rows, prototype tracking rows, and GC region/byte/collection counters.
- The implementation skips released/unreferenced objects and uses existing GC/object metadata for base-size accounting.
- Added `tests/debug/test_heap_summary.c`.
- Added CLI `--heap-summary` and `--heap-summary=<out>` parse support.
- Runtime writes the heap summary after successful execution: stdout for bare `--heap-summary`, file output for `--heap-summary=<out>`.
- CLI rejects empty output paths and non-run modes.

## Validation

| Build | Command / range | Result |
|------|------------------|--------|
| Windows/MSVC Debug | `cmake --build build-msvc --config Debug --target zr_vm_debug_heap_summary_test zr_vm_cli_args_test zr_vm_cli_import_basic_fixture_test -- /m:2 /v:minimal`; `ctest --test-dir build-msvc -C Debug -R "^(debug_heap_summary|cli_args|cli_import_basic_fixture)$" --output-on-failure` | Build passed; focused gate 3/3 PASS |
| WSL/GCC Debug | `cmake -S . -B build-wsl-gcc`; focused object rebuild and `tests/CMakeFiles/zr_vm_cli_import_basic_fixture_test.dir/link.txt`; `ctest --test-dir build-wsl-gcc -R "^(debug_heap_summary|cli_args|cli_import_basic_fixture)$" --output-on-failure` | Focused gate 3/3 PASS |
| WSL/Clang Debug | `cmake -S . -B build-wsl-clang`; focused object rebuild and `tests/CMakeFiles/zr_vm_cli_import_basic_fixture_test.dir/link.txt`; `ctest --test-dir build-wsl-clang -R "^(debug_heap_summary|cli_args|cli_import_basic_fixture)$" --output-on-failure` | Focused gate 3/3 PASS |

## Results

- Core heap summary test confirms the object header, string/object/thread type rows, GC region row, and GC collection row.
- CLI args test covers bare `--heap-summary`, `--heap-summary=<out>`, empty-path rejection, and compile-only rejection.
- CLI import fixture confirms binary run output remains `hello from import` and the report file contains `ZR_HEAP_SUMMARY objects`, type counts, and GC collection counters.
- WSL heap-summary build processes left by timed-out broad rebuild attempts were cleaned before final verification. A later process scan showed an unrelated `codex-semantic` WSL build, which was left untouched.

## Acceptance Decision

Accepted for Phase 6.4 heap summary. Remaining limitation: this is a lightweight base-size and GC counter summary, not an object-reference graph or leak path analyzer; the optional GC event hook remains deferred because the plan allows it as optional follow-up.
