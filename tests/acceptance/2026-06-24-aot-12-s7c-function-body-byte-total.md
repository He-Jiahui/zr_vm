# 2026-06-24 AOT 12-S7C Function Body Byte Total

## Scope

12-S7C adds the first aggregate function-body size statistic for opt-in AOT C code stripping:

- Generated C records `code_stripping.functionBodyBytesTotal`.
- The total includes emitted/retained generated C function body spans.
- Trimmed functions are excluded because they are not emitted.

This does not implement pre-trim size estimates, type/layout/metadata byte attribution, trim warnings, or release
symbol stripping.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  accumulates the same `ftell()` span used for per-function body statistics and writes a retained function-body total
  after the function-body emission loop.
- `tests/parser/test_aot_c_code_stripping.c`
  verifies the total statistic exists for the removed-child, export-root, and manifest-root generated C cases.

## RED / GREEN

RED:

- `zr_vm_aot_c_code_stripping_test` built, then all 3 tests failed because generated C had no
  `code_stripping.functionBodyBytesTotal` comment.

GREEN:

- The unreachable static callable fixture emits a retained function body total after functions 0 and 1.
- The export-root fixture emits a retained function body total after functions 0, 1, and 2.
- The manifest-root fixture emits a retained function body total after functions 0, 1, and 2.

## Validation

- `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test"`
  - 3 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7C. Opt-in stripped generated C now reports retained function-body bytes in aggregate. Full 12-S7
remains open for broader byte attribution, trim diagnostics, and symbol stripping.
