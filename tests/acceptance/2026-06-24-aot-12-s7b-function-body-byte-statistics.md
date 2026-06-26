# 2026-06-24 AOT 12-S7B Function Body Byte Statistics

## Scope

12-S7B extends opt-in code-stripping statistics from function counts to emitted generated-C function body spans:

- Each generated AOT C function body gets a `code_stripping.functionBodyBytes[flatIndex]` comment.
- Removed functions do not get a body-byte statistic.
- Export-root and manifest-root preserved functions do get body-byte statistics.

This does not implement full byte attribution for types, layouts, metadata, trim diagnostics, or release symbol stripping.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  records `ftell(file)` before and after `backend_aot_write_c_function_body()` when code stripping is enabled, then
  writes the byte-span comment for the emitted function's original flat index.
- `tests/parser/test_aot_c_code_stripping.c`
  verifies that the trimmed fixture emits statistics for functions 0 and 1 but not the removed function 2, while
  export-root and manifest-root fixtures emit statistics for all 0/1/2 functions.

## RED / GREEN

RED:

- `zr_vm_aot_c_code_stripping_test` built, then all 3 tests failed because generated C had no
  `code_stripping.functionBodyBytes[...]` comments.

GREEN:

- The unreachable static callable fixture reports function-body bytes for retained functions 0 and 1 only.
- The export-root fixture reports function-body bytes for 0, 1, and 2.
- The manifest-root fixture reports function-body bytes for 0, 1, and 2.

## Validation

- `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_code_stripping_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_code_stripping_test"`
  - 3 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7B. Generated-C function-body byte spans are now visible for opt-in stripped output. Full 12-S7 remains
open for richer byte attribution, trim warnings, and symbol stripping.
