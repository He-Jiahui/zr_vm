# AOT M1.5 / 07-S5 Typed Bool Logical Local-Only Path

Timestamp: 2026-06-22 01:24:18 +08:00

Status: complete for this slice; 07-S5 remains partial; 08-12 not started.

## Scope

- Optimize typed `LOGICAL_EQUAL_BOOL`, `LOGICAL_NOT_EQUAL_BOOL`, and `LOGICAL_NOT_BOOL` so proven bool scalar-local inputs and destinations stay in `zr_aot_bN` locals.
- Preserve `zr_aot_bool_compare_exec` and `zr_aot_bool_not_exec` fallback behavior for unproven or non-declared-bool destinations.
- Keep generic conversion fallback correctness when a bool opcode writes a value slot that is not declared as a bool scalar local.

## Changes

- Source contracts now require `zr_aot_bool_compare_scalar_local` and `zr_aot_bool_not_scalar_local` markers, bool source write-history checks, and local-only assignment templates.
- The bool logical smoke fixture now exercises typed bool equality and inequality in addition to unary not.
- Typed bool logical emitters receive the instruction index, consult bool scalar-local proof, and write `zr_aot_bN` directly when the result slot can skip value-slot storage.
- Scalar-local analysis records typed bool logical destinations only when declared bool evidence exists, preventing invalid local branches in fallback paths.

## RED / GREEN

- RED: source contracts failed 18/1 after the test contract required `zr_aot_bool_compare_scalar_local`.
- SEMFAIL: an intermediate implementation caused the aggregate shared-library smoke generic-conversion fixture to compile a branch on undeclared `zr_aot_b6` after fallback `zr_aot_bool_not_exec`.
- GREEN: focused validation passed source contracts 19/0, logical shared-library smoke 4/0, and aggregate shared-library smoke 8/0.
- GREEN: broader focused AOT validation passed call contracts 4/0, call shared-library smoke 3/0, aggregate shared-library smoke 8/0, power contracts 2/0, power shared-library smoke 1/0, source contracts 19/0, generic numeric contracts 1/0, generic numeric shared-library smoke 1/0, global shared-library smoke 9/0, logical contracts 4/0, logical shared-library smoke 4/0, typed scalar 1/0, return contracts 1/0, and frame setup contracts 1/0.

## Generated Checks

- `bool_logical_project/bin/aot_c/src/main.c` contains `zr_aot_bool_compare_scalar_local` and `zr_aot_bool_not_scalar_local`.
- The same generated bool logical fixture has no `zr_aot_bool_compare_exec` or `zr_aot_bool_not_exec` marker.
- `generic_conversion_project/bin/aot_c/src/main.c` preserves `zr_aot_bool_not_exec` fallback and no longer emits the invalid `zr_aot_b6` local branch.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Notes

This is a register-model narrowing slice, not a full 07-S5 completion. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates still belong to later 07 work.
