# AOT M1.5 07-S5 Typed Direct-Call Arithmetic Smoke Support Split

Date: 2026-06-22 08:41:44 +08:00

## Scope

This support slice keeps the arithmetic typed direct-call smoke below the large-file warning threshold before adding more 07-S5 direct-call coverage.

Covered:

- Extracted common Unix shared-library smoke helpers into `tests/parser/aot_c_typed_direct_call_arithmetic_smoke_support.h`.
- Kept `tests/parser/test_aot_c_typed_direct_call_arithmetic_shared_library_smoke.c` focused on concrete arithmetic typed thunk cases and `main()`.
- Preserved the current five arithmetic smoke cases: two-arg multiply, two-arg bitwise-and, one-arg multiply-constant, one-arg subtract-constant, and one-arg negate.
- Reduced the main arithmetic smoke from 863 physical lines / 786 non-empty lines to 753 physical lines / 697 non-empty lines.
- Added a 116 physical line / 93 non-empty line support header.

Out of scope:

- New typed thunk coverage.
- Production AOT behavior changes.
- CMake target reorganization.
- Marking 07-S5, 07, or M1.5 complete.

## RED

No behavior RED was added for this support refactor. The change only moves existing test helper code behind a focused support header.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_typed_direct_call_arithmetic_shared_library_smoke_test`: 5/0.

## Notes

This split keeps room for later 07-S5 arithmetic direct-call slices without continuing to stack fixture code into the same test file. Wider typed return ABI lowering and dynamic/deopt bridge work remain open.
