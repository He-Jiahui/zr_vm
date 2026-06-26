# 2026-06-24 AOT 12-S7A Function Stripping Statistics

## Scope

12-S7A adds the first size-statistics surface for opt-in AOT C code stripping:

- Generated C records whether code stripping was enabled.
- Generated C records function count before filtering.
- Generated C records function count after filtering.
- Generated C records removed function count.

This is only the function-count part of 12-S7. It does not implement trim warnings, byte attribution for functions,
types, layouts, metadata, or release symbol stripping.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  captures `functionTable.count` before and after opt-in reachability filtering and emits:
  - `code_stripping.enabled`
  - `code_stripping.functionsBefore`
  - `code_stripping.functionsAfter`
  - `code_stripping.functionsRemoved`
- `tests/parser/test_aot_c_code_stripping.c`
  checks statistics for the removed-child, export-root-preserved, and manifest-root-preserved cases.

## RED / GREEN

RED:

- `zr_vm_aot_c_code_stripping_test` built, then all 3 tests failed because generated C had no code-stripping
  statistics comments.

GREEN:

- The unreachable static callable fixture reports `functionsBefore = 3`, `functionsAfter = 2`, and
  `functionsRemoved = 1`.
- The export-root fixture reports `3`, `3`, and `0`.
- The manifest-root fixture reports `3`, `3`, and `0`.

## Validation

- `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test"`
  - 3 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7A. Function-level count statistics are now visible in opt-in generated C output. Full 12-S7 remains
open for trim diagnostics, richer byte-size attribution, and symbol stripping.
