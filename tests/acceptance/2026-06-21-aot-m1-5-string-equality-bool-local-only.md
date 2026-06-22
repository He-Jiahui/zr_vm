# AOT M1.5 / 07-S5 String Equality Bool-Result Local-Only Path

Timestamp: 2026-06-22 01:51:29 +08:00

Status: complete for this slice; 07-S5 remains partial; 08-12 not started.

## Scope

- Optimize `LOGICAL_EQUAL_STRING` and `LOGICAL_NOT_EQUAL_STRING` when the bool destination can skip its value slot.
- Preserve existing string operand validation and byte comparison behavior.
- Keep the old `ZR_VALUE_FAST_SET` fallback for destinations that still need a value-slot write.

## Changes

- Added `backend_aot_c_write_string_bool_scalar_local()` in the generic logical C lowering module.
- String equality emitters now receive the instruction index and write `zr_aot_bN` directly when bool result-skip proof holds.
- Scalar-local write analysis now records declared bool destinations for `LOGICAL_EQUAL_STRING` and `LOGICAL_NOT_EQUAL_STRING`.
- Logical source contracts now require the local-only marker, result-skip proof, function-body instruction index pass-through, and scalar-local recording coverage.

## RED / GREEN

- RED: `zr_vm_aot_c_logical_contracts_test` failed 1/4 with missing `backend_aot_c_write_string_bool_scalar_local`.
- GREEN: focused validation passed logical contracts 4/0, logical shared-library smoke 4/0, source contracts 19/0, and aggregate shared-library smoke 8/0.
- GREEN: broader focused AOT validation passed call contracts 4/0, call shared-library smoke 3/0, aggregate shared-library smoke 8/0, power contracts 2/0, power shared-library smoke 1/0, source contracts 19/0, generic numeric contracts 1/0, generic numeric shared-library smoke 1/0, global shared-library smoke 9/0, logical contracts 4/0, logical shared-library smoke 4/0, typed scalar 1/0, return contracts 1/0, and frame setup contracts 1/0.

## Generated Checks

- `string_equality_project/bin/aot_c/src/main.c` contains `zr_aot_string_logical_bool_scalar_local` in string equality blocks.
- The generated string equality block writes direct bool locals, for example `zr_aot_b3 = (TZrBool)(zr_aot_equal != 0u);`.
- The local-only result path does not emit `ZR_VALUE_FAST_SET` for the string equality bool result.
- Scoped `git diff --check` over the files touched by this slice exited 0 with only LF/CRLF warnings.
- Full-repo `git diff --check` currently reports an existing unrelated `docs/plans/lsp/index.md:201 new blank line at EOF`.

## Notes

This slice removes the destination bool result value-slot write. String operands still use the existing frame-slot string value reads and validation; removing those reads requires a separate object/string local representation slice.
