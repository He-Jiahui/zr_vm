# AOT M1.5 07-S5 Typed Direct-Call Bitwise Smoke Support Split

Date: 2026-06-22 09:52:35 +08:00

## Scope

This support slice keeps the bitwise typed direct-call smoke below the large-file warning threshold before adding more 07-S5 direct-call coverage.

Covered:

- Extracted common Unix shared-library smoke helpers into `tests/parser/aot_c_typed_direct_call_bitwise_smoke_support.h`.
- Kept `tests/parser/test_aot_c_typed_direct_call_bitwise_shared_library_smoke.c` focused on concrete bitwise typed thunk cases and `main()`.
- Preserved the current five bitwise smoke cases: two-arg OR, two-arg XOR, one-arg NOT, one-arg AND-constant, and one-arg OR-constant.
- Reduced the main bitwise smoke from 753 physical lines / 697 non-empty lines to 162 physical lines / 150 non-empty lines.
- Added a 197 physical line / 178 non-empty line support header.

Out of scope:

- New typed thunk coverage.
- Production AOT behavior changes.
- CMake target reorganization.
- Marking 07-S5, 07, or M1.5 complete.
- Starting 08-12 work.

## RED

No behavior RED was added for this support refactor. The change only moves existing test helper code behind a focused support header.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_typed_direct_call_bitwise_shared_library_smoke_test`: 5/0.

## Notes

This split keeps room for later 07-S5 bitwise direct-call slices without continuing to stack fixture code into the same test file. Wider typed return ABI lowering and dynamic/deopt bridge work remain open.
